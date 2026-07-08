#ifndef HMI_H
#define HMI_H

#include <stdint.h>

/* Modo HMI — operação via display LCD 2004A + botões GPIO + buzzer.
 *
 * O handshake com o STM32 não ocorre ao entrar neste modo: é disparado
 * apenas quando o operador pressiona START, permitindo que o sistema
 * fique em tela de espera por tempo indefinido.
 *
 * Ciclo de operação:
 *   1. Display exibe a tela de espera com os contadores de objetos
 *      entregues em cada rota.
 *   2. START pressionado → inicialização da câmera, seguida de
 *      handshake com o STM32.
 *   3. Operação autônoma (mesma máquina de estados do modo FSM, com
 *      saída no LCD em vez de log no terminal). START durante a
 *      operação zera os contadores sem interromper o ciclo.
 *   4. Falha na leitura do QR → alerta no display com backlight e
 *      buzzer piscando, instruindo o operador a liberar a esteira;
 *      START confirma o alerta e dispara reset do STM32 seguido de
 *      reenvio do handshake, aguardado sem timeout fixo até o STM32
 *      responder espontaneamente (ou até STOP cancelar a espera).
 *   5. STOP pressionado a qualquer momento durante a operação → reset
 *      do STM32, volta ao passo 1.
 *   6. Ctrl+C (g_interrupted) encerra o modo em qualquer estado e
 *      exibe uma mensagem final no display. */

/* Configuração de hardware — ajuste conforme a fiação real */
#define HMI_I2C_DEVICE       "/dev/i2c-1"
#define HMI_LCD_I2C_ADDR     0x27
#define HMI_GPIO_CHIP        "gpiochip0"
#define HMI_BTN_START_LINE   17   /* GPIO17 */
#define HMI_BTN_STOP_LINE    27   /* GPIO27 */
#define HMI_BUZZER_LINE      23   /* GPIO23 — saída para o buzzer de alerta */
#define HMI_BTN_DEBOUNCE_MS  30

/**
 * @brief Executa o loop do modo HMI (display + botões + buzzer).
 *
 * Inicializa o LCD, os botões GPIO e o buzzer, exibe a tela de espera
 * e aguarda START. Ao pressionar START, inicializa a câmera, executa o
 * handshake e entra na operação autônoma com saída no display. STOP
 * retorna à tela de espera; g_interrupted encerra definitivamente o
 * modo e limpa o display com uma mensagem final.
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera (0 = /dev/video0).
 * @param qr_timeout  Timeout para leitura do QR em ms (0 = sem limite).
 */
void hmi_run(int fd, int cam_index, int qr_timeout);

#endif /* HMI_H */
