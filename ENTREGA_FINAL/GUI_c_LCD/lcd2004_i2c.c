#define _POSIX_C_SOURCE 200809L

#include "lcd2004_i2c.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* Bits do PCF8574 (mapeamento padrão dos backpacks LCD I2C) */
#define LCD_BIT_RS  0x01  /* P0 — Register Select */
#define LCD_BIT_RW  0x02  /* P1 — sempre em escrita (0) */
#define LCD_BIT_E   0x04  /* P2 — Enable */
#define LCD_BIT_BL  0x08  /* P3 — Backlight */
#define LCD_DATA_SHIFT 4  /* dados em P4-P7 */

/* Comandos do HD44780 */
#define LCD_CMD_CLEAR        0x01
#define LCD_CMD_HOME         0x02
#define LCD_CMD_ENTRY_MODE   0x04
#define LCD_CMD_DISPLAY_CTRL 0x08
#define LCD_CMD_FUNCTION_SET 0x20
#define LCD_CMD_SET_DDRAM    0x80

/* Endereços de início de cada linha no HD44780.
 * O controlador trata o 2004A como 2 blocos de 40 colunas:
 * linhas 0 e 2 compartilham o bloco 1 (0x00/0x14),
 * linhas 1 e 3 compartilham o bloco 2 (0x40/0x54). */
static const uint8_t LCD_ROW_OFFSETS[LCD2004_ROWS] = {0x00, 0x40, 0x14, 0x54};

/* Estado interno do driver */
static int     s_fd           = -1;
static uint8_t s_backlight_on = LCD_BIT_BL;
static uint8_t s_cur_col      = 0;
static uint8_t s_cur_row      = 0;

/**
 * @brief Aguarda um intervalo curto em microssegundos.
 *
 * @param us  Tempo em microssegundos.
 */
static void delay_us(long us)
{
    struct timespec ts;
    ts.tv_sec  = us / 1000000L;
    ts.tv_nsec = (us % 1000000L) * 1000L;
    nanosleep(&ts, NULL);
}

/**
 * @brief Escreve um byte bruto no PCF8574 via I2C.
 *
 * @param value  Byte completo (RS/RW/E/BL + nibble de dados).
 * @return       0 em sucesso, -1 em erro de escrita.
 */
static int pcf_write(uint8_t value)
{
    if (s_fd < 0) return -1;
    if (write(s_fd, &value, 1) != 1) {
        log_print(LOG_ERR, "lcd: falha na escrita I2C: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief Envia um nibble ao HD44780 com pulso de Enable.
 *
 * Gera a sequência E=0 → E=1 → E=0 com os tempos mínimos
 * exigidos pelo datasheet (borda >= 450 ns; aqui usamos 1 us
 * e 50 us de margem, seguros para o barramento I2C do RPi).
 *
 * @param nibble  Nibble de 4 bits (bits 0-3) a enviar.
 * @param rs      0 para comando, 1 para dado de caractere.
 */
static void lcd_write_nibble(uint8_t nibble, int rs)
{
    uint8_t data = (uint8_t)((nibble << LCD_DATA_SHIFT) & 0xF0);
    data |= s_backlight_on;
    if (rs) data |= LCD_BIT_RS;

    pcf_write(data);               /* E = 0 */
    pcf_write(data | LCD_BIT_E);   /* E = 1 */
    delay_us(1);
    pcf_write(data);               /* E = 0 — captura na descida */
    delay_us(50);
}

/**
 * @brief Envia um byte completo ao HD44780 em dois nibbles.
 *
 * @param value  Byte a enviar.
 * @param rs     0 para comando, 1 para dado.
 */
static void lcd_write_byte(uint8_t value, int rs)
{
    lcd_write_nibble((uint8_t)(value >> 4),   rs);
    lcd_write_nibble((uint8_t)(value & 0x0F), rs);
}

/**
 * @brief Envia um byte de comando ao HD44780 (RS = 0).
 *
 * @param cmd  Código do comando.
 */
static void lcd_command(uint8_t cmd)
{
    lcd_write_byte(cmd, 0);
}

/**
 * @brief Inicializa o display LCD via I2C.
 *
 * @param i2c_dev   Caminho do dispositivo I2C (ex: "/dev/i2c-1").
 * @param i2c_addr  Endereço do PCF8574 (ex: 0x27).
 * @return          0 em sucesso, -1 em erro.
 */
int lcd_init(const char *i2c_dev, uint8_t i2c_addr)
{
    s_fd = open(i2c_dev, O_RDWR);
    if (s_fd < 0) {
        log_print(LOG_ERR, "lcd_init: falha ao abrir %s: %s",
            i2c_dev, strerror(errno));
        return -1;
    }

    if (ioctl(s_fd, I2C_SLAVE, i2c_addr) < 0) {
        log_print(LOG_ERR, "lcd_init: ioctl I2C_SLAVE (0x%02X) falhou: %s",
            i2c_addr, strerror(errno));
        close(s_fd);
        s_fd = -1;
        return -1;
    }

    /* Sequência de inicialização do HD44780 em modo 4 bits.
     * O controlador pode estar em estado desconhecido ao ligar;
     * a sequência clássica de 3x nibble 0x3 + nibble 0x2 garante
     * que ele entre em 4 bits independentemente do estado inicial. */
    delay_us(50000);

    lcd_write_nibble(0x03, 0); delay_us(4500);
    lcd_write_nibble(0x03, 0); delay_us(4500);
    lcd_write_nibble(0x03, 0); delay_us(150);
    lcd_write_nibble(0x02, 0); delay_us(150);

    /* A partir daqui usa lcd_command() normalmente (2 nibbles cada) */
    lcd_command(LCD_CMD_FUNCTION_SET | 0x08); /* 4-bit, 2 linhas lógicas */
    delay_us(50);
    lcd_command(LCD_CMD_DISPLAY_CTRL);        /* display off (config inicial) */
    delay_us(50);
    lcd_command(LCD_CMD_CLEAR);
    delay_us(2000);                           /* clear exige ~1.6 ms */
    lcd_command(LCD_CMD_ENTRY_MODE | 0x02);   /* cursor incrementa, sem shift */
    delay_us(50);
    lcd_command(LCD_CMD_DISPLAY_CTRL | 0x04); /* display on, cursor/blink off */
    delay_us(50);

    s_cur_col = 0;
    s_cur_row = 0;

    log_print(LOG_SYS, "LCD inicializado: %s addr=0x%02X (%dx%d)",
        i2c_dev, i2c_addr, LCD2004_COLS, LCD2004_ROWS);
    return 0;
}

/**
 * @brief Fecha o barramento I2C usado pelo display.
 */
void lcd_close(void)
{
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
}

/**
 * @brief Limpa o display e volta o cursor para (0, 0).
 */
void lcd_clear(void)
{
    lcd_command(LCD_CMD_CLEAR);
    delay_us(2000);
    s_cur_col = 0;
    s_cur_row = 0;
}

/**
 * @brief Move o cursor para a coluna e linha indicadas.
 *
 * @param col  Coluna (0 a LCD2004_COLS - 1).
 * @param row  Linha  (0 a LCD2004_ROWS - 1).
 */
void lcd_set_cursor(uint8_t col, uint8_t row)
{
    if (row >= LCD2004_ROWS) row = LCD2004_ROWS - 1;
    if (col >= LCD2004_COLS) col = LCD2004_COLS - 1;

    uint8_t addr = (uint8_t)(LCD_ROW_OFFSETS[row] + col);
    lcd_command((uint8_t)(LCD_CMD_SET_DDRAM | addr));

    s_cur_col = col;
    s_cur_row = row;
}

/**
 * @brief Liga ou desliga o backlight do display imediatamente.
 *
 * @param on  1 para ligar o backlight, 0 para desligar.
 */
void lcd_set_backlight(int on)
{
    s_backlight_on = on ? LCD_BIT_BL : 0;

    /* Escreve o byte com E e RS em zero: nenhum pulso de Enable ocorre,
     * então nenhum comando/dado é interpretado pelo HD44780 -- apenas o
     * bit de backlight do PCF8574 muda, sem afetar cursor ou conteúdo. */
    pcf_write(s_backlight_on);
}

/**
 * @brief Escreve uma string a partir da posição atual do cursor.
 *
 * @param str  String ASCII terminada em NUL.
 */
void lcd_print(const char *str)
{
    if (!str) return;
    while (*str && s_cur_col < LCD2004_COLS) {
        lcd_write_byte((uint8_t)*str, 1);
        str++;
        s_cur_col++;
    }
}

/**
 * @brief Sobrescreve uma linha inteira do display com a string fornecida.
 *
 * @param row  Linha de destino (0 a LCD2004_ROWS - 1).
 * @param str  String ASCII (truncada se exceder LCD2004_COLS bytes).
 */
void lcd_print_line(uint8_t row, const char *str)
{
    char buf[LCD2004_COLS + 1];
    size_t len = str ? strlen(str) : 0;
    if (len > LCD2004_COLS) len = LCD2004_COLS;

    memset(buf, ' ', LCD2004_COLS);
    if (str) memcpy(buf, str, len);
    buf[LCD2004_COLS] = '\0';

    lcd_set_cursor(0, row);
    lcd_print(buf);
}
