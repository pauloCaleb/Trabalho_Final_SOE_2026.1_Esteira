#ifndef FSM_H
#define FSM_H

#include <stdint.h>

/* =========================================================
 *  Módulo FSM — modo de operação autônomo
 *
 *  Espelha a lógica da aba "APLICACAO" da GUI Python v4.
 *  Opera sem intervenção do usuário:
 *    1. Aguarda OBJ_DETECTED do STM32
 *    2. Ao receber CLSS_REQUEST, aciona câmera em background
 *    3. Lê QR e envia ROUTE_A ou ROUTE_B automaticamente
 *    4. Aguarda confirmação de entrega e volta ao início
 * ========================================================= */

/**
 * Executa o loop do modo FSM.
 * Bloqueia até o usuário encerrar (Ctrl+C ou sinal de saída).
 *
 * @param fd          Descritor da porta serial
 * @param cam_index   Índice da câmera (0 = /dev/video0)
 * @param qr_timeout  Timeout para leitura do QR em ms (0 = sem limite)
 */
void fsm_run(int fd, int cam_index, int qr_timeout);

#endif /* FSM_H */


#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdint.h>

/* =========================================================
 *  Módulo Debug — modo de operação interativo
 *
 *  Espelha a aba "DEBUG" da GUI Python v4.
 *  Apresenta um menu de comandos no terminal e aplica
 *  os comandos assíncronos ao STM32 via UART.
 *  Também monitora e exibe a telemetria dos sensores.
 * ========================================================= */

/**
 * Executa o loop do modo Debug.
 * Bloqueia até o usuário digitar 'q' para sair.
 *
 * @param fd  Descritor da porta serial
 */
void debug_run(int fd);

#endif /* DEBUG_MODE_H */
