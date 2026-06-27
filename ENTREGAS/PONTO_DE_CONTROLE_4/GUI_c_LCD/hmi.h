#ifndef HMI_H
#define HMI_H

#include <stdint.h>

/* Modo HMI — operação via display LCD 2004A + botões GPIO.
 *
 * Diferente do modo FSM, o handshake com o STM32 NÃO ocorre ao entrar
 * neste modo — só é disparado quando o usuário pressiona START. Isso
 * permite que o display fique em tela de espera por tempo indefinido.
 *
 * Ciclo de operação:
 *   1. Display exibe "Aperte START".
 *   2. START pressionado → handshake com o STM32.
 *   3. Operação autônoma (mesma lógica do modo FSM, mas com saída no LCD).
 *   4. STOP pressionado → reset do STM32, volta ao passo 1.
 *   5. Ctrl+C (g_interrupted) encerra o modo em qualquer estado. */

/* Configuração de hardware — ajuste conforme a fiação real */
#define HMI_I2C_DEVICE       "/dev/i2c-1"
#define HMI_LCD_I2C_ADDR     0x27
#define HMI_GPIO_CHIP        "gpiochip0"
#define HMI_BTN_START_LINE   17   /* GPIO17 */
#define HMI_BTN_STOP_LINE    27   /* GPIO27 */
#define HMI_BTN_DEBOUNCE_MS  30

/**
 * @brief Executa o loop do modo HMI (display + botões).
 *
 * Inicializa o LCD e os botões GPIO, exibe a tela de espera e aguarda
 * START. Ao pressionar START, executa o handshake e entra na operação
 * autônoma com saída no display. STOP retorna à tela de espera;
 * g_interrupted encerra definitivamente o modo.
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera (0 = /dev/video0).
 * @param qr_timeout  Timeout para leitura do QR em ms (0 = sem limite).
 */
void hmi_run(int fd, int cam_index, int qr_timeout);

#endif /* HMI_H */
