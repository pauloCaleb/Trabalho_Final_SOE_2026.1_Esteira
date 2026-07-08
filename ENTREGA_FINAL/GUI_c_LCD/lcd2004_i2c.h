#ifndef LCD2004_I2C_H
#define LCD2004_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Driver para display LCD 2004A (20x4) via backpack I2C PCF8574.
 *
 * Controlador: HD44780, operado em modo 4 bits (nibble alto primeiro).
 * Mapeamento padrão dos pinos do PCF8574:
 *   P0=RS  P1=RW  P2=E  P3=Backlight  P4-P7=D4-D7
 *
 * Endereço I2C padrão com A0/A1/A2 abertos: 0x27.
 *
 * Atenção: use apenas ASCII puro nas strings passadas às funções de
 * escrita — o HD44780 não entende UTF-8, e bytes multibyte de acentos
 * latinos resultam em caracteres errados na tela. */

#define LCD2004_COLS 20
#define LCD2004_ROWS 4

/**
 * @brief Inicializa o display LCD via I2C.
 *
 * Abre o barramento I2C, configura o endereço do escravo e executa a
 * sequência de inicialização do HD44780 em modo 4 bits (2 linhas lógicas,
 * cursor desligado, display limpo).
 *
 * @param i2c_dev   Caminho do dispositivo I2C (ex: "/dev/i2c-1").
 * @param i2c_addr  Endereço do PCF8574 (ex: 0x27).
 * @return          0 em sucesso, -1 em erro.
 */
int lcd_init(const char *i2c_dev, uint8_t i2c_addr);

/**
 * @brief Fecha o barramento I2C usado pelo display.
 */
void lcd_close(void);

/**
 * @brief Limpa o display e volta o cursor para (0, 0).
 */
void lcd_clear(void);

/**
 * @brief Move o cursor para a coluna e linha indicadas.
 *
 * Valores fora da faixa são saturados para a borda válida mais próxima.
 *
 * @param col  Coluna (0 a LCD2004_COLS - 1).
 * @param row  Linha  (0 a LCD2004_ROWS - 1).
 */
void lcd_set_cursor(uint8_t col, uint8_t row);

/**
 * @brief Escreve uma string a partir da posição atual do cursor.
 *
 * Não quebra linha automaticamente; caracteres além da borda da linha
 * são descartados.
 *
 * @param str  String ASCII terminada em NUL.
 */
void lcd_print(const char *str);

/**
 * @brief Liga ou desliga o backlight do display imediatamente.
 *
 * Escreve apenas o bit de backlight no PCF8574 (mantendo E em nível
 * baixo), sem afetar o conteúdo já exibido nem a posição do cursor.
 * Útil para piscar o display como alerta visual.
 *
 * @param on  1 para ligar o backlight, 0 para desligar.
 */
void lcd_set_backlight(int on);

/**
 * @brief Sobrescreve uma linha inteira do display com a string fornecida.
 *
 * Posiciona o cursor no início da linha, escreve o texto truncado em
 * LCD2004_COLS caracteres e preenche o restante com espaços — sem precisar
 * chamar lcd_clear() a cada atualização, o que evitaria flicker.
 *
 * @param row  Linha de destino (0 a LCD2004_ROWS - 1).
 * @param str  String ASCII a exibir (truncada se exceder LCD2004_COLS bytes).
 */
void lcd_print_line(uint8_t row, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* LCD2004_I2C_H */
