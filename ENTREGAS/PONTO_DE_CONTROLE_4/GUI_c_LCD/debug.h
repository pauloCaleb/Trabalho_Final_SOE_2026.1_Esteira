#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdint.h>

/* Modo Debug — controle manual da esteira via terminal.
 *
 * Apresenta um menu de comandos e aplica os comandos assíncronos
 * ao STM32 via UART. Também exibe telemetria dos sensores em
 * tempo real enquanto o operador digita. */

/**
 * @brief Executa o loop do modo Debug (controle manual).
 *
 * Sobe uma thread RX para monitorar eventos e telemetria em paralelo,
 * e apresenta um menu interativo no terminal. Bloqueia até 'q' ser
 * pressionado ou g_interrupted ser setado por SIGINT/SIGTERM.
 *
 * A desinicialização do STM32 (SW_RESET) não é feita aqui;
 * main.c chama deinit_stm32() após o retorno desta função.
 *
 * @param fd  Descritor da porta serial.
 */
void debug_run(int fd);

#endif /* DEBUG_MODE_H */
