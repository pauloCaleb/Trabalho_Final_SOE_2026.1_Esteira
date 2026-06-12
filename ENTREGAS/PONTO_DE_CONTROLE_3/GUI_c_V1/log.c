#define _POSIX_C_SOURCE 200809L

#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_RED     "\033[31m"

static void print_timestamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);
    printf("%02d:%02d:%02d.%03ld  ",
           tm_info->tm_hour,
           tm_info->tm_min,
           tm_info->tm_sec,
           ts.tv_nsec / 1000000L);
}

static void print_prefix(LogCategory cat)
{
    switch (cat) {
        case LOG_UART_TX:
            printf(ANSI_CYAN    ANSI_BOLD "[UART TX] " ANSI_RESET); break;
        case LOG_UART_RX:
            printf(ANSI_GREEN   ANSI_BOLD "[UART RX] " ANSI_RESET); break;
        case LOG_FSM:
            printf(ANSI_BLUE    ANSI_BOLD "[FSM    ] " ANSI_RESET); break;
        case LOG_CAM:
            printf(ANSI_MAGENTA ANSI_BOLD "[CAM    ] " ANSI_RESET); break;
        case LOG_DBG:
            printf(ANSI_YELLOW  ANSI_BOLD "[DBG    ] " ANSI_RESET); break;
        case LOG_SYS:
            printf(ANSI_WHITE   ANSI_BOLD "[SYS    ] " ANSI_RESET); break;
        case LOG_ERR:
            printf(ANSI_RED     ANSI_BOLD "[ERR    ] " ANSI_RESET); break;
    }
}

void log_print(LogCategory cat, const char *fmt, ...)
{
    print_timestamp();
    print_prefix(cat);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void log_sensors(uint8_t flags)
{
    print_timestamp();
    printf(ANSI_YELLOW ANSI_BOLD "[SENSORES] " ANSI_RESET);
    for (int i = 0; i < 4; i++) {
        int ativo = (flags >> i) & 0x01;
        if (ativo)
            printf(ANSI_RED   "S%d:[OBJETO] " ANSI_RESET, i + 1);
        else
            printf(ANSI_GREEN "S%d:[LIVRE ] " ANSI_RESET, i + 1);
    }
    printf("\n");
    fflush(stdout);
}

void log_fsm_state(int state_id)
{
    const char *name;
    const char *color;

    switch (state_id) {
        case 0:  name = "IDLE";              color = ANSI_WHITE;   break;
        case 1:  name = "OBJETO DETECTADO";  color = ANSI_YELLOW;  break;
        case 2:  name = "CLASSIFICANDO";     color = ANSI_CYAN;    break;
        case 3:  name = "ROTA A";            color = ANSI_GREEN;   break;
        case 4:  name = "ROTA B";            color = ANSI_MAGENTA; break;
        case 5:  name = "ENTREGUE - ROTA A"; color = ANSI_GREEN;   break;
        case 6:  name = "ENTREGUE - ROTA B"; color = ANSI_MAGENTA; break;
        default: name = "--";                color = ANSI_WHITE;   break;
    }

    print_timestamp();
    printf(ANSI_BOLD "[FSM    ] " ANSI_RESET);
    printf("Estado atual: %s%s%s\n", color, name, ANSI_RESET);

    printf("          ");
    const char *states[] = {"IDLE", "OBJ", "CLASS", "ROTA A", "ROTA B"};
    int ids[]            = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++) {
        if (ids[i] == state_id)
            printf(ANSI_BOLD "[%s]" ANSI_RESET, states[i]);
        else
            printf(" %s ", states[i]);
        if (i < 4) printf(" > ");
    }
    printf("\n");
    fflush(stdout);
}

void log_debug_menu(void)
{
    printf("\n");
    printf(ANSI_YELLOW ANSI_BOLD "========== MODO DEBUG ==========\n" ANSI_RESET);
    printf("  1  Flash ON\n");
    printf("  2  Flash OFF\n");
    printf("  3  Cancela ABRIR\n");
    printf("  4  Cancela FECHAR\n");
    printf("  5  Motor ENGAJAR\n");
    printf("  6  Motor LIVRE\n");
    printf("  7  Direcao FRENTE\n");
    printf("  8  Direcao TRAS\n");
    printf("  9  Enviar passos (solicita quantidade)\n");
    printf("  t  Toggle modo DEBUG/FSM\n");
    printf("  r  SW Reset STM32\n");
    printf("  q  Sair do programa\n");
    printf(ANSI_YELLOW "================================\n" ANSI_RESET);
    printf("Comando: ");
    fflush(stdout);
}

void log_banner(void)
{
    printf(ANSI_CYAN ANSI_BOLD);
    printf("+=======================================================+\n");
    printf("|   ESTEIRA SEPARADORA -- Sistema de Controle          |\n");
    printf("|   STM32G070 <-> Raspberry Pi 3B  |  Firmware v3      |\n");
    printf("|   SOE 2026  --  Interface CLI em C                   |\n");
    printf("+=======================================================+\n");
    printf(ANSI_RESET);
    fflush(stdout);
}
