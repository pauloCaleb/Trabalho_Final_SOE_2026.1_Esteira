#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "hmi.h"
#include "protocol.h"
#include "serial.h"
#include "camera.h"
#include "log.h"
#include "lcd2004_i2c.h"
#include "gpio_button.h"
#include "app_signal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Estados internos da operação autônoma (espelham a FSM do firmware) */
typedef enum {
    HMI_OP_IDLE         = 0,
    HMI_OP_OBJ_DETECTED = 1,
    HMI_OP_CLASSIFYING  = 2,
    HMI_OP_ROUTE_A      = 3,
    HMI_OP_ROUTE_B      = 4,
    HMI_OP_DELIVERED_A  = 5,
    HMI_OP_DELIVERED_B  = 6
} HmiOpState;

/* Contexto compartilhado com a thread RX */
typedef struct {
    int             fd;
    volatile int    running;
    volatile int    rx_event;
    pthread_mutex_t lock;
} HmiCtx;

/**
 * @brief Thread de recepção UART do modo HMI.
 *
 * Mesma estrutura da thread RX do modo FSM: alimenta o parser byte a
 * byte e armazena o comando recebido em ctx->rx_event (sob mutex).
 *
 * @param arg  Ponteiro para HmiCtx.
 * @return     NULL (API pthread).
 */
static void *hmi_rx_thread(void *arg)
{
    HmiCtx *ctx = (HmiCtx *)arg;
    uint8_t byte;
    RxFrame frame;

    log_print(LOG_SYS, "Thread RX (HMI) iniciada.");

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

    log_print(LOG_SYS, "Thread RX (HMI) encerrada.");
    return NULL;
}

/**
 * @brief Monta e envia um frame TX de 2 bytes ao STM32, com log.
 *
 * @param ctx  Contexto HMI (fornece o fd serial).
 * @param cmd  Byte de comando a enviar.
 */
static void hmi_send(HmiCtx *ctx, uint8_t cmd)
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
 * @param ctx  Contexto HMI.
 * @return     Byte do comando recebido (0 se nenhum evento novo).
 */
static int hmi_consume_event(HmiCtx *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    int ev = ctx->rx_event;
    ctx->rx_event = 0;
    pthread_mutex_unlock(&ctx->lock);
    return ev;
}

/**
 * @brief Atualiza as 4 linhas do display com o estado atual da operação.
 *
 * Usa lcd_print_line() para sobrescrever cada linha sem lcd_clear(),
 * evitando flicker.
 *
 * @param state  Sub-estado atual da operação autônoma.
 */
static void hmi_display_state(HmiOpState state)
{
    const char *txt;
    switch (state) {
        case HMI_OP_IDLE:         txt = "Aguardando objeto"; break;
        case HMI_OP_OBJ_DETECTED: txt = "Objeto detectado";  break;
        case HMI_OP_CLASSIFYING:  txt = "Classificando...";  break;
        case HMI_OP_ROUTE_A:      txt = "Rota A";             break;
        case HMI_OP_ROUTE_B:      txt = "Rota B";             break;
        case HMI_OP_DELIVERED_A:  txt = "Entregue - Rota A";  break;
        case HMI_OP_DELIVERED_B:  txt = "Entregue - Rota B";  break;
        default:                  txt = "--";                 break;
    }
    lcd_print_line(0, "ESTEIRA - OPERANDO");
    lcd_print_line(1, txt);
    lcd_print_line(2, "STOP p/ encerrar");
    lcd_print_line(3, "");
}

/**
 * @brief Exibe a tela de espera (antes do handshake/START).
 */
static void hmi_display_waiting(void)
{
    lcd_print_line(0, "ESTEIRA SEPARADORA");
    lcd_print_line(1, "SOE 2026.1");
    lcd_print_line(2, "");
    lcd_print_line(3, "Aperte START");
}

/**
 * @brief Exibe uma mensagem transitória de 2 linhas no display.
 *
 * @param l0  Texto da linha 0.
 * @param l1  Texto da linha 1.
 */
static void hmi_display_msg(const char *l0, const char *l1)
{
    lcd_print_line(0, l0);
    lcd_print_line(1, l1);
    lcd_print_line(2, "");
    lcd_print_line(3, "");
}

/**
 * @brief Executa o ciclo de operação autônoma (pós-handshake).
 *
 * Sobe a thread RX, inicializa a câmera e espelha no display os eventos
 * da FSM do firmware. Verifica o botão STOP a cada iteração do loop.
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera.
 * @param qr_timeout  Timeout de leitura do QR em ms.
 * @param btn_stop    Botão STOP já aberto.
 * @return  1 se deve retornar à tela de espera (STOP ou falha de câmera),
 *          0 se g_interrupted foi setado durante a operação.
 */
static int hmi_operate(int fd, int cam_index, int qr_timeout,
                        GpioButton *btn_stop)
{
    HmiCtx ctx;
    ctx.fd       = fd;
    ctx.running  = 1;
    ctx.rx_event = 0;
    pthread_mutex_init(&ctx.lock, NULL);

    pthread_t tid;
    pthread_create(&tid, NULL, hmi_rx_thread, &ctx);

    hmi_display_msg("Iniciando camera", "aguarde...");
    if (camera_init(cam_index) != 0) {
        log_print(LOG_ERR, "HMI: falha ao inicializar camera.");
        hmi_display_msg("ERRO: camera", "nao inicializou");
        ctx.running = 0;
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&ctx.lock);
        sleep(2);
        return 1;
    }

    HmiOpState state = HMI_OP_IDLE;
    hmi_display_state(state);
    log_print(LOG_FSM, "HMI: operacao iniciada. Aguardando objetos...");

    int interrupted_during_op = 0;

    while (ctx.running) {
        if (g_interrupted) {
            log_print(LOG_SYS,
                "HMI: interrupcao solicitada (Ctrl+C) -- encerrando com seguranca...");
            interrupted_during_op = 1;
            break;
        }

        if (button_was_pressed(btn_stop)) {
            log_print(LOG_SYS, "HMI: STOP pressionado -- encerrando operacao.");
            break;
        }

        int ev = hmi_consume_event(&ctx);

        switch (state) {

            case HMI_OP_IDLE:
                if (ev == OBJ_DETECTED) {
                    state = HMI_OP_OBJ_DETECTED;
                    log_print(LOG_FSM, "HMI: objeto detectado.");
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_OBJ_DETECTED:
                if (ev == CLSS_REQUEST) {
                    state = HMI_OP_CLASSIFYING;
                    log_print(LOG_FSM, "HMI: classificando QR...");
                    hmi_display_state(state);

                    CamResult res = camera_read_qr(qr_timeout);

                    if (res == CAM_ROUTE_A) {
                        state = HMI_OP_ROUTE_A;
                        hmi_display_state(state);
                        hmi_send(&ctx, ROUTE_A_SEND);
                    } else if (res == CAM_ROUTE_B) {
                        state = HMI_OP_ROUTE_B;
                        hmi_display_state(state);
                        hmi_send(&ctx, ROUTE_B_SEND);
                    } else {
                        log_print(LOG_ERR,
                            "HMI: falha na classificacao (timeout/erro).");
                        state = HMI_OP_IDLE;
                        hmi_display_state(state);
                    }
                }
                break;

            case HMI_OP_CLASSIFYING:
                /* Inalcançável como entrada de switch: a transição para este
                 * estado e sua resolução (camera_read_qr, bloqueante) ocorrem
                 * por completo dentro do case HMI_OP_OBJ_DETECTED. Mantido
                 * aqui para que todos os valores do enum sejam cobertos
                 * explicitamente, evitando warnings do compilador. */
                break;

            case HMI_OP_ROUTE_A:
                if (ev == ROUTE_A_OK) {
                    state = HMI_OP_DELIVERED_A;
                    hmi_display_state(state);
                    sleep(2);
                    state = HMI_OP_IDLE;
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_ROUTE_B:
                if (ev == ROUTE_B_OK) {
                    state = HMI_OP_DELIVERED_B;
                    hmi_display_state(state);
                    sleep(2);
                    state = HMI_OP_IDLE;
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_DELIVERED_A:
            case HMI_OP_DELIVERED_B:
                break;
        }

        if (ev == SW_RESET_MSG) {
            log_print(LOG_SYS, "HMI: STM32 reiniciando por SW Reset.");
            state = HMI_OP_IDLE;
            hmi_display_state(state);
        }

        usleep(10000);
    }

    ctx.running = 0;
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&ctx.lock);
    camera_release();
    log_print(LOG_FSM, "HMI: operacao encerrada.");

    return interrupted_during_op ? 0 : 1;
}

/**
 * @brief Executa o loop do modo HMI (display + botões).
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera.
 * @param qr_timeout  Timeout para leitura do QR em ms.
 */
void hmi_run(int fd, int cam_index, int qr_timeout)
{
    if (lcd_init(HMI_I2C_DEVICE, HMI_LCD_I2C_ADDR) != 0) {
        log_print(LOG_ERR, "HMI: falha ao inicializar o display LCD.");
        return;
    }

    GpioButton *btn_start = button_open(HMI_GPIO_CHIP, HMI_BTN_START_LINE,
                                         HMI_BTN_DEBOUNCE_MS);
    GpioButton *btn_stop  = button_open(HMI_GPIO_CHIP, HMI_BTN_STOP_LINE,
                                         HMI_BTN_DEBOUNCE_MS);

    if (!btn_start || !btn_stop) {
        log_print(LOG_ERR, "HMI: falha ao abrir os botoes GPIO.");
        hmi_display_msg("ERRO: botoes GPIO", "verifique fiacao");
        button_close(btn_start);
        button_close(btn_stop);
        lcd_close();
        return;
    }

    log_print(LOG_SYS, "Modo HMI iniciado. Aguardando START...");
    hmi_display_waiting();

    while (!g_interrupted) {
        if (button_was_pressed(btn_start)) {
            log_print(LOG_SYS, "HMI: START pressionado -- iniciando handshake...");
            hmi_display_msg("Conectando ao", "STM32...");

            if (do_handshake(fd) != 0) {
                log_print(LOG_ERR, "HMI: handshake falhou.");
                hmi_display_msg("ERRO: handshake", "tente novamente");
                sleep(2);
                hmi_display_waiting();
                continue;
            }

            int return_to_waiting = hmi_operate(fd, cam_index, qr_timeout, btn_stop);

            if (!return_to_waiting)
                break; /* g_interrupted durante a operação — sai do modo HMI */

            /* STOP pressionado ou falha de câmera: reset do STM32 antes
             * de voltar à tela de espera, para devolvê-lo ao estado inicial. */
            deinit_stm32(fd);
            hmi_display_msg("Parando...", "");
            sleep(1);
            hmi_display_waiting();
            continue;
        }

        if (g_interrupted) break;
        usleep(20000);
    }

    if (g_interrupted)
        log_print(LOG_SYS, "HMI: interrompido (Ctrl+C) -- encerrando.");

    button_close(btn_start);
    button_close(btn_stop);
    lcd_close();
    log_print(LOG_SYS, "Modo HMI encerrado.");
}
