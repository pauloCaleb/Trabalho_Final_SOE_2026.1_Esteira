#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "serial.h"
#include "protocol.h"
#include "log.h"
#include "fsm.h"

#define DEFAULT_PORT          "/dev/ttyS0"
#define DEFAULT_BAUD          115200
#define DEFAULT_CAM_INDEX     0
#define DEFAULT_QR_TIMEOUT_MS 30000
#define HANDSHAKE_TIMEOUT_MS  10000

static volatile int g_interrupted = 0;

static void sig_handler(int sig)
{
    (void)sig;
    g_interrupted = 1;
}

static int do_handshake(int fd)
{
    log_print(LOG_SYS,
        "Enviando SYS_RDY (0x10) -- aguardando SYS_INIT...");

    uint8_t buf[2];
    int len = protocol_build_frame(SYS_RDY, buf);
    serial_write(fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x10]  %s", protocol_tx_name(SYS_RDY));

    int elapsed = 0;
    int hs_heartbeat = 0;
    uint8_t byte;
    RxFrame frame;

    while (elapsed < HANDSHAKE_TIMEOUT_MS) {
        int r = serial_read_byte_timeout(fd, &byte, 100);
        elapsed += 100;
        if (r <= 0) {
            if (++hs_heartbeat % 20 == 0)
                log_print(LOG_SYS,
                    "Aguardando SYS_INIT do STM32... (%d ms / %d ms)",
                    elapsed, HANDSHAKE_TIMEOUT_MS);
            continue;
        }

        if (protocol_parse_byte(byte, &frame)) {
            log_print(LOG_UART_RX,
                "<< RX [0x%02X][0x%02X]  %s",
                frame.status, frame.cmd,
                protocol_rx_name(frame.cmd));

            if (frame.status == CMD_OK && frame.cmd == SYS_INIT) {
                log_print(LOG_SYS, "Handshake OK -- sistema inicializado.");
                return 0;
            }
            if (frame.status == CMD_ERR) {
                log_print(LOG_ERR,
                    "STM32 respondeu CMD_ERR durante handshake.");
                return -1;
            }
        }
    }

    log_print(LOG_ERR, "Timeout no handshake (%d ms).", HANDSHAKE_TIMEOUT_MS);
    return -1;
}

static int select_mode(void)
{
    printf("\n");
    printf("Selecione o modo de operacao:\n");
    printf("  1  Modo FSM  (operacao autonoma com camera)\n");
    printf("  2  Modo DEBUG (controle manual via terminal)\n");
    printf("Opcao: ");
    fflush(stdout);

    int c = getchar();
    int nx;
    while ((nx = getchar()) != '\n' && nx != EOF);

    if (c == '1') return 1;
    if (c == '2') return 2;
    return -1;
}

static void print_usage(const char *prog)
{
    printf("Uso: %s [opcoes]\n", prog);
    printf("  -p <porta>    Porta serial (padrao: %s)\n", DEFAULT_PORT);
    printf("  -b <baud>     Baud rate   (padrao: %d)\n",  DEFAULT_BAUD);
    printf("  -c <indice>   Indice da camera (padrao: %d)\n", DEFAULT_CAM_INDEX);
    printf("  -t <ms>       Timeout QR em ms (padrao: %d, 0=sem limite)\n",
           DEFAULT_QR_TIMEOUT_MS);
    printf("  -m <1|2>      Modo direto: 1=FSM, 2=DEBUG (pula menu)\n");
    printf("  -h            Exibe esta ajuda\n");
}

int main(int argc, char *argv[])
{
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    const char *port       = DEFAULT_PORT;
    int         baud       = DEFAULT_BAUD;
    int         cam_index  = DEFAULT_CAM_INDEX;
    int         qr_timeout = DEFAULT_QR_TIMEOUT_MS;
    int         mode_arg   = 0;

    int opt;
    while ((opt = getopt(argc, argv, "p:b:c:t:m:h")) != -1) {
        switch (opt) {
            case 'p': port       = optarg;       break;
            case 'b': baud       = atoi(optarg); break;
            case 'c': cam_index  = atoi(optarg); break;
            case 't': qr_timeout = atoi(optarg); break;
            case 'm': mode_arg   = atoi(optarg); break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    log_banner();
    log_print(LOG_SYS,
        "Porta: %s  |  Baud: %d  |  Camera: %d  |  QR timeout: %d ms",
        port, baud, cam_index, qr_timeout);

    log_print(LOG_SYS, "Abrindo porta serial %s @ %d bps...", port, baud);
    int fd = serial_open(port, baud);
    if (fd < 0) {
        log_print(LOG_ERR,
            "Nao foi possivel abrir %s. Verifique a conexao.", port);
        return 1;
    }
    log_print(LOG_SYS, "Porta serial aberta: %s @ %d bps", port, baud);

    if (do_handshake(fd) != 0) {
        log_print(LOG_ERR, "Handshake falhou. Encerrando.");
        serial_close(fd);
        return 1;
    }

    int mode = mode_arg;
    while (mode != 1 && mode != 2 && !g_interrupted) {
        mode = select_mode();
        if (mode != 1 && mode != 2)
            printf("Opcao invalida. Digite 1 ou 2.\n");
    }

    if (g_interrupted) {
        log_print(LOG_SYS, "Interrompido antes de iniciar.");
        serial_close(fd);
        return 0;
    }

    if (mode == 1) {
        log_print(LOG_SYS, "Iniciando modo FSM...");
        fsm_run(fd, cam_index, qr_timeout);
    } else {
        log_print(LOG_SYS, "Iniciando modo DEBUG...");
        debug_run(fd);
    }

    log_print(LOG_SYS, "Encerrando programa.");
    serial_close(fd);
    return 0;
}
