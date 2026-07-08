#ifndef FSM_H
#define FSM_H

#include <stdint.h>

/* Modo FSM — operação autônoma sem intervenção do usuário.
 *
 * Fluxo:
 *   1. Aguarda OBJ_DETECTED do STM32.
 *   2. Ao receber CLSS_REQUEST, aciona a câmera para leitura do QR.
 *   3. Envia ROUTE_A_SEND ou ROUTE_B_SEND conforme o resultado.
 *   4. Aguarda confirmação de entrega e volta ao IDLE. */

/**
 * @brief Executa o loop do modo FSM (operação autônoma).
 *
 * Sobe a thread RX antes de inicializar a câmera (evita perder
 * eventos do STM32 durante a abertura do dispositivo de vídeo, que
 * pode levar ~2 s). Em seguida percorre a máquina de estados até
 * g_interrupted ser setado por SIGINT/SIGTERM.
 *
 * A desinicialização do STM32 (SW_RESET) não é feita aqui;
 * main.c chama deinit_stm32() após o retorno desta função.
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera (0 = /dev/video0).
 * @param qr_timeout  Timeout para leitura do QR em ms (0 = sem limite).
 */
void fsm_run(int fd, int cam_index, int qr_timeout);

#endif /* FSM_H */
