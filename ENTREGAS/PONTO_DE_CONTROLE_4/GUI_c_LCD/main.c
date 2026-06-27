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
#include "debug.h"
#include "hmi.h"
#include "app_signal.h"

#define DEFAULT_PORT          "/dev/ttyS0"
#define DEFAULT_BAUD          115200
#define DEFAULT_CAM_INDEX     0
#define DEFAULT_QR_TIMEOUT_MS 30000

volatile int g_interrupted = 0;

/**
 * @brief Handler de SIGINT/SIGTERM.
 *
 * Apenas seta g_interrupted; o encerramento real acontece na
 * próxima iteração dos loops de fsm_run(), debug_run() ou hmi_run().
 *
 * @param sig  Número do sinal (não utilizado).
 */
static void sig_handler(int sig)
{
    (void)sig;
    g_interrupted = 1;
}

/**
 * @brief Exibe o menu de seleção de modo e lê a opção do usuário.
 *
 * @return  1 = FSM, 2 = Debug, 3 = HMI, -1 = opção inválida.
 */
static int select_mode(void)
{
    printf("\n");
    printf("Selecione o modo de operacao:\n");
    printf("  1  Modo FSM  (operacao autonoma com camera)\n");
    printf("  2  Modo DEBUG (controle manual via terminal)\n");
    printf("  3  Modo HMI  (display LCD + botoes START/STOP)\n");
    printf("Opcao: ");
    fflush(stdout);

    int c = getchar();
    int nx;
    while ((nx = getchar()) != '\n' && nx != EOF);

    if (c == '1') return 1;
    if (c == '2') return 2;
    if (c == '3') return 3;
    return -1;
}

/**
 * @brief Exibe as opções de linha de comando do programa.
 *
 * @param prog  Nome do executável (argv[0]).
 */
static void print_usage(const char *prog)
{
    printf("Uso: %s [opcoes]\n", prog);
    printf("  -p <porta>    Porta serial (padrao: %s)\n", DEFAULT_PORT);
    printf("  -b <baud>     Baud rate   (padrao: %d)\n",  DEFAULT_BAUD);
    printf("  -c <indice>   Indice da camera (padrao: %d)\n", DEFAULT_CAM_INDEX);
    printf("  -t <ms>       Timeout QR em ms (padrao: %d, 0=sem limite)\n",
           DEFAULT_QR_TIMEOUT_MS);
    printf("  -m <1|2|3>    Modo direto: 1=FSM, 2=DEBUG, 3=HMI (pula menu)\n");
    printf("  -h            Exibe esta ajuda\n");
}

/**
 * @brief Ponto de entrada do programa.
 *
 * Configura os handlers de sinal, abre a porta serial, seleciona o
 * modo de operação (via menu ou flag -m) e delega para fsm_run(),
 * debug_run() ou hmi_run(). O handshake com o STM32 é feito aqui
 * para os modos FSM e Debug; no modo HMI é feito internamente em
 * hmi_run(), somente quando o usuário pressiona START.
 *
 * @param argc  Quantidade de argumentos.
 * @param argv  Vetor de argumentos (-p, -b, -c, -t, -m, -h).
 * @return      0 em encerramento normal, 1 em erro.
 */
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

    int fd = serial_open(port, baud);
    if (fd < 0) {
        log_print(LOG_ERR,
            "Nao foi possivel abrir %s. Verifique a conexao.", port);
        return 1;
    }

    int mode = mode_arg;
    while (mode != 1 && mode != 2 && mode != 3 && !g_interrupted) {
        mode = select_mode();
        if (mode != 1 && mode != 2 && mode != 3)
            printf("Opcao invalida. Digite 1, 2 ou 3.\n");
    }

    if (g_interrupted) {
        log_print(LOG_SYS, "Interrompido antes de iniciar.");
        serial_close(fd);
        return 0;
    }

    if (mode == 1) {
        log_print(LOG_SYS, "Iniciando modo FSM...");
        if (do_handshake(fd) != 0) {
            log_print(LOG_ERR, "Handshake falhou. Encerrando.");
            serial_close(fd);
            return 1;
        }
        fsm_run(fd, cam_index, qr_timeout);

    } else if (mode == 2) {
        log_print(LOG_SYS, "Iniciando modo DEBUG...");
        if (do_handshake(fd) != 0) {
            log_print(LOG_ERR, "Handshake falhou. Encerrando.");
            serial_close(fd);
            return 1;
        }
        /* Coloca o STM32 em modo Debug logo após o handshake */
        uint8_t buf[2];
        int len = protocol_build_frame(DEBUG_TOGGLE, buf);
        serial_write(fd, buf, len);
        debug_run(fd);

    } else {
        /* Modo HMI: handshake ocorre só ao pressionar START */
        log_print(LOG_SYS, "Iniciando modo HMI...");
        hmi_run(fd, cam_index, qr_timeout);
    }

    log_print(LOG_SYS, "Encerrando programa.");
    deinit_stm32(fd);
    serial_close(fd);
    return 0;
}
