#include "debug.h"
#include "protocol.h"
#include "serial.h"
#include "log.h"
#include "app_signal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

/* Contexto interno do modo Debug */
typedef struct {
    int             fd;
    volatile int    running;
    uint8_t         sens_flags;  /* último byte de telemetria recebido */
    pthread_mutex_t lock;
} DbgCtx;

/**
 * @brief Thread de recepção UART do modo Debug.
 *
 * Monitora a serial em background. Telemetria de sensores
 * (SENS_STATUS_MSG) é exibida via log_sensors(); demais frames
 * são registrados como LOG_UART_RX.
 *
 * @param arg  Ponteiro para DbgCtx.
 * @return     NULL (API pthread).
 */
static void *dbg_rx_thread(void *arg)
{
    DbgCtx *ctx = (DbgCtx *)arg;
    uint8_t byte;
    RxFrame frame;

    log_print(LOG_SYS, "Thread RX (DEBUG) iniciada -- monitorando UART.");

    while (ctx->running) {
        int r = serial_read_byte_timeout(ctx->fd, &byte, 100);
        if (r <= 0) continue;

        if (protocol_parse_byte(byte, &frame)) {
            if (frame.status == CMD_ERR) {
                log_print(LOG_ERR, "CMD_ERR recebido do STM32");
                continue;
            }

            if (frame.cmd == SENS_STATUS_MSG && frame.has_data) {
                pthread_mutex_lock(&ctx->lock);
                ctx->sens_flags = frame.data;
                pthread_mutex_unlock(&ctx->lock);
                log_sensors(frame.data);
                continue;
            }

            log_print(LOG_UART_RX,
                "<< RX [0x%02X][0x%02X]  %s",
                frame.status, frame.cmd,
                protocol_rx_name(frame.cmd));
        }
    }

    log_print(LOG_SYS, "Thread RX (DEBUG) encerrada.");
    return NULL;
}

/**
 * @brief Monta e envia um frame TX de 2 bytes ao STM32, com log.
 *
 * @param ctx  Contexto de debug (fornece o fd serial).
 * @param cmd  Byte de comando a enviar.
 */
static void dbg_send(DbgCtx *ctx, uint8_t cmd)
{
    uint8_t buf[2];
    int len = protocol_build_frame(cmd, buf);
    serial_write(ctx->fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x%02X]  %s",
        cmd, protocol_tx_name(cmd));
}

/**
 * @brief Monta e envia um frame TX de 3 bytes (com dado) ao STM32, com log.
 *
 * Usado apenas para SET_STPR_TGT_STPS (quantidade de passos do motor).
 *
 * @param ctx   Contexto de debug.
 * @param cmd   Byte de comando a enviar.
 * @param data  Byte de dado (ex: número de passos).
 */
static void dbg_send_data(DbgCtx *ctx, uint8_t cmd, uint8_t data)
{
    uint8_t buf[3];
    int len = protocol_build_frame_data(cmd, data, buf);
    serial_write(ctx->fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x%02X][0x%02X]  %s  DATA=0x%02X",
        cmd, data, protocol_tx_name(cmd), data);
}

/**
 * @brief Lê um caractere do teclado com timeout em ms.
 *
 * Usa select() sobre STDIN para não bloquear a thread principal
 * indefinidamente enquanto a thread RX roda em paralelo.
 *
 * @param timeout_ms  Tempo máximo de espera em milissegundos.
 * @return            Caractere lido (>= 0), ou -1 em timeout.
 */
static int read_char_timeout(int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) return -1;

    int c = getchar();
    if (c != '\n' && c != EOF) {
        int nx;
        while ((nx = getchar()) != '\n' && nx != EOF);
    }
    return c;
}

/**
 * @brief Executa o loop do modo Debug (controle manual).
 *
 * @param fd  Descritor da porta serial.
 */
void debug_run(int fd)
{
    DbgCtx ctx;
    ctx.fd         = fd;
    ctx.running    = 1;
    ctx.sens_flags = 0;
    pthread_mutex_init(&ctx.lock, NULL);

    pthread_t tid;
    pthread_create(&tid, NULL, dbg_rx_thread, &ctx);

    log_print(LOG_DBG, "Modo DEBUG iniciado.");
    log_debug_menu();

    while (ctx.running) {
        if (g_interrupted) {
            log_print(LOG_SYS,
                "Interrupcao solicitada (Ctrl+C) -- encerrando modo DEBUG com seguranca...");
            break;
        }

        int c = read_char_timeout(200);
        if (c < 0) continue;

        switch (c) {
            case '1':
                log_print(LOG_DBG, "Flash ON");
                dbg_send(&ctx, LIGHT_EN);
                break;
            case '2':
                log_print(LOG_DBG, "Flash OFF");
                dbg_send(&ctx, LIGHT_DIS);
                break;
            case '3':
                log_print(LOG_DBG, "Cancela ABRIR");
                dbg_send(&ctx, GATE_OPEN);
                break;
            case '4':
                log_print(LOG_DBG, "Cancela FECHAR");
                dbg_send(&ctx, GATE_CLOSE);
                break;
            case '5':
                log_print(LOG_DBG, "Motor ENGAJAR");
                dbg_send(&ctx, STPR_EN);
                break;
            case '6':
                log_print(LOG_DBG, "Motor LIVRE");
                dbg_send(&ctx, STPR_DIS);
                break;
            case '7':
                log_print(LOG_DBG, "Direcao FRENTE");
                dbg_send(&ctx, STPR_FORWARD);
                break;
            case '8':
                log_print(LOG_DBG, "Direcao TRAS");
                dbg_send(&ctx, STPR_BACKWARD);
                break;
            case '9': {
                printf("Quantidade de passos (1-255): ");
                fflush(stdout);
                int steps = 0;
                if (scanf("%d", &steps) == 1) {
                    int nx;
                    while ((nx = getchar()) != '\n' && nx != EOF);
                    if (steps < 1)   steps = 1;
                    if (steps > 255) steps = 255;
                    log_print(LOG_DBG, "Enviando %d passos", steps);
                    dbg_send_data(&ctx, SET_STPR_TGT_STPS, (uint8_t)steps);
                }
                break;
            }
            case 't':
            case 'T':
                log_print(LOG_DBG, "Toggle DEBUG/FSM");
                dbg_send(&ctx, DEBUG_TOGGLE);
                break;
            case 'r':
            case 'R':
                printf("Confirma SW Reset? (s/n): ");
                fflush(stdout);
                {
                    int confirm = getchar();
                    int nx;
                    while ((nx = getchar()) != '\n' && nx != EOF);
                    if (confirm == 's' || confirm == 'S') {
                        log_print(LOG_DBG, "SW Reset enviado.");
                        dbg_send(&ctx, SW_RESET_MSG);
                    } else {
                        log_print(LOG_DBG, "Reset cancelado.");
                    }
                }
                break;
            case 'h':
            case 'H':
                log_print(LOG_DBG, "Executando Handshake sob comando do modo DEBUG.");
                do_handshake(fd);
                break;
            case 'a':
            case 'A':
                log_print(LOG_DBG, "Encaminhando para o caminho A");
                dbg_send(&ctx, ROUTE_A_SEND);
                break;
            case 'b':
            case 'B':
                log_print(LOG_DBG, "Encaminhando para o caminho B");
                dbg_send(&ctx, ROUTE_B_SEND);
                break;
            case 'q':
            case 'Q':
                log_print(LOG_DBG, "Encerrando modo DEBUG.");
                ctx.running = 0;
                break;
            default:
                log_debug_menu();
                break;
        }

        if (ctx.running)
            log_debug_menu();
    }

    ctx.running = 0;
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&ctx.lock);
    log_print(LOG_DBG, "Modo DEBUG encerrado.");
}
