#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "fsm.h"
#include "protocol.h"
#include "serial.h"
#include "camera.h"
#include "log.h"
#include "app_signal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Estados internos da FSM no lado do Raspberry Pi */
typedef enum {
    PC_FSM_IDLE         = 0,
    PC_FSM_OBJ_DETECTED = 1,
    PC_FSM_CLASSIFYING  = 2,
    PC_FSM_ROUTE_A      = 3,
    PC_FSM_ROUTE_B      = 4,
    PC_FSM_DELIVERED_A  = 5,
    PC_FSM_DELIVERED_B  = 6
} PcFsmState;

/* Contexto compartilhado entre a thread RX e o loop principal */
typedef struct {
    int             fd;
    volatile int    running;
    volatile int    rx_event;   /* último comando recebido do STM32 */
    pthread_mutex_t lock;
} FsmCtx;

/**
 * @brief Thread de recepção UART do modo FSM.
 *
 * Roda em background, lendo byte a byte com timeout curto e
 * alimentando o parser de protocolo. Ao montar um frame válido,
 * armazena o comando em ctx->rx_event (sob mutex) para o loop
 * principal consumir.
 *
 * @param arg  Ponteiro para FsmCtx.
 * @return     NULL (API pthread).
 */
static void *rx_thread(void *arg)
{
    FsmCtx *ctx = (FsmCtx *)arg;
    uint8_t byte;
    RxFrame frame;

    log_print(LOG_SYS, "Thread RX (FSM) iniciada.");

    while (ctx->running) {
        int r = serial_read_byte_timeout(ctx->fd, &byte, 100);
        if (r <= 0) continue;

        if (protocol_parse_byte(byte, &frame)) {
            if (frame.status == CMD_ERR) {
                log_print(LOG_ERR, "CMD_ERR recebido do STM32");
                continue;
            }
            log_print(LOG_UART_RX,
                "<< RX [0x%02X][0x%02X]  %s",
                frame.status, frame.cmd,
                protocol_rx_name(frame.cmd));

            pthread_mutex_lock(&ctx->lock);
            ctx->rx_event = frame.cmd;
            pthread_mutex_unlock(&ctx->lock);
        }
    }

    log_print(LOG_SYS, "Thread RX (FSM) encerrada.");
    return NULL;
}

/**
 * @brief Monta e envia um frame TX de 2 bytes ao STM32, com log.
 *
 * @param ctx  Contexto FSM (fornece o fd serial).
 * @param cmd  Byte de comando a enviar.
 */
static void fsm_send(FsmCtx *ctx, uint8_t cmd)
{
    uint8_t buf[2];
    int len = protocol_build_frame(cmd, buf);
    serial_write(ctx->fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x%02X]  %s",
        cmd, protocol_tx_name(cmd));
}

/**
 * @brief Consome o último evento RX pendente, zerando o campo.
 *
 * @param ctx  Contexto FSM.
 * @return     Byte do comando recebido (0 se nenhum evento novo).
 */
static int fsm_consume_event(FsmCtx *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    int ev = ctx->rx_event;
    ctx->rx_event = 0;
    pthread_mutex_unlock(&ctx->lock);
    return ev;
}

/**
 * @brief Executa o loop do modo FSM (operação autônoma).
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera (0 = /dev/video0).
 * @param qr_timeout  Timeout para leitura do QR em ms (0 = sem limite).
 */
void fsm_run(int fd, int cam_index, int qr_timeout)
{
    FsmCtx ctx;
    ctx.fd       = fd;
    ctx.running  = 1;
    ctx.rx_event = 0;
    pthread_mutex_init(&ctx.lock, NULL);

    /* Thread RX sobe ANTES da câmera para não perder eventos
     * enquanto o OpenCV abre o dispositivo (~2 s). */
    pthread_t tid;
    pthread_create(&tid, NULL, rx_thread, &ctx);

    if (camera_init(cam_index) != 0) {
        log_print(LOG_ERR, "Falha ao inicializar camera. Modo FSM abortado.");
        ctx.running = 0;
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&ctx.lock);
        return;
    }

    PcFsmState state = PC_FSM_IDLE;
    int idle_heartbeat = 0;
#define IDLE_HEARTBEAT_TICKS 500

    log_fsm_state(0);
    log_print(LOG_FSM, "Modo FSM iniciado. Aguardando objetos...");

    while (ctx.running) {
        if (g_interrupted) {
            log_print(LOG_SYS,
                "Interrupcao solicitada (Ctrl+C) -- encerrando modo FSM com seguranca...");
            break;
        }

        int ev = fsm_consume_event(&ctx);

        switch (state) {

            case PC_FSM_IDLE:
                if (ev == OBJ_DETECTED) {
                    idle_heartbeat = 0;
                    state = PC_FSM_OBJ_DETECTED;
                    log_print(LOG_FSM, "Objeto detectado -- esteira transportando.");
                    log_fsm_state(1);
                } else {
                    if (++idle_heartbeat >= IDLE_HEARTBEAT_TICKS) {
                        idle_heartbeat = 0;
                        log_print(LOG_FSM, "Aguardando objeto na esteira... (IDLE)");
                    }
                }
                break;

            case PC_FSM_OBJ_DETECTED:
                if (ev == CLSS_REQUEST) {
                    state = PC_FSM_CLASSIFYING;
                    log_print(LOG_FSM, "Objeto sob a camera -- iniciando classificacao QR.");
                    log_fsm_state(2);

                    CamResult res = camera_read_qr(qr_timeout);

                    if (res == CAM_ROUTE_A) {
                        state = PC_FSM_ROUTE_A;
                        log_print(LOG_FSM, "Classificacao: ROTA A -- enviando comando.");
                        log_fsm_state(3);
                        fsm_send(&ctx, ROUTE_A_SEND);
                    } else if (res == CAM_ROUTE_B) {
                        state = PC_FSM_ROUTE_B;
                        log_print(LOG_FSM, "Classificacao: ROTA B -- enviando comando.");
                        log_fsm_state(4);
                        fsm_send(&ctx, ROUTE_B_SEND);
                    } else {
                        log_print(LOG_ERR,
                            "Falha na classificacao (timeout/erro). Voltando para IDLE.");
                        state = PC_FSM_IDLE;
                        log_fsm_state(0);
                    }
                }
                break;

            case PC_FSM_ROUTE_A:
                if (ev == ROUTE_A_FWD)
                    log_print(LOG_FSM, "STM32 confirmou encaminhamento -> ROTA A.");
                if (ev == ROUTE_A_OK) {
                    state = PC_FSM_DELIVERED_A;
                    log_print(LOG_FSM, "Entrega confirmada -- ROTA A. Ciclo encerrado.");
                    log_fsm_state(5);
                    sleep(2);
                    state = PC_FSM_IDLE;
                    log_fsm_state(0);
                    log_print(LOG_FSM, "Aguardando proximo objeto...");
                }
                break;

            case PC_FSM_ROUTE_B:
                if (ev == ROUTE_B_FWD)
                    log_print(LOG_FSM, "STM32 confirmou encaminhamento -> ROTA B.");
                if (ev == ROUTE_B_OK) {
                    state = PC_FSM_DELIVERED_B;
                    log_print(LOG_FSM, "Entrega confirmada -- ROTA B. Ciclo encerrado.");
                    log_fsm_state(6);
                    sleep(2);
                    state = PC_FSM_IDLE;
                    log_fsm_state(0);
                    log_print(LOG_FSM, "Aguardando proximo objeto...");
                }
                break;

            case PC_FSM_DELIVERED_A:
            case PC_FSM_DELIVERED_B:
                break;

            default:
                if (ev == MODE_DEBUG_MSG) {
                    log_print(LOG_SYS, "STM32 entrou em modo DEBUG. Encerrando loop FSM.");
                    ctx.running = 0;
                }
                break;
        }

        if (ev == SW_RESET_MSG) {
            log_print(LOG_SYS, "STM32 reiniciando por SW Reset...");
            state = PC_FSM_IDLE;
            log_fsm_state(0);
        }

        usleep(10000);
    }

    ctx.running = 0;
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&ctx.lock);
    camera_release();
    log_print(LOG_FSM, "Modo FSM encerrado.");

    /* deinit_stm32() fica a cargo de main.c, que o chama
     * independentemente do motivo de saída. */
}
