#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================
 *  Módulo Serial — POSIX (Linux / Raspberry Pi)
 *  Configura a porta serial para 115200 8N1 sem controle
 *  de fluxo, compatível com a USART2 do STM32G070.
 * ========================================================= */

/**
 * Abre e configura a porta serial.
 * @param device  Caminho do dispositivo, ex: "/dev/ttyUSB0"
 * @param baud    Taxa em bps (ex: 115200)
 * @return        Descritor de arquivo >= 0 em sucesso, -1 em erro.
 */
int serial_open(const char *device, int baud);

/**
 * Fecha a porta serial.
 */
void serial_close(int fd);

/**
 * Envia exatamente 'len' bytes.
 * @return  Número de bytes enviados, ou -1 em erro.
 */
int serial_write(int fd, const uint8_t *buf, size_t len);

/**
 * Lê até 'len' bytes disponíveis (não bloqueante).
 * @return  Número de bytes lidos (0 se nenhum disponível), -1 em erro.
 */
int serial_read(int fd, uint8_t *buf, size_t len);

/**
 * Lê exatamente 1 byte com timeout em milissegundos.
 * @return  1 se leu, 0 se timeout, -1 em erro.
 */
int serial_read_byte_timeout(int fd, uint8_t *byte, int timeout_ms);

/**
 * Descarta todos os bytes pendentes no buffer de RX.
 */
void serial_flush(int fd);

#endif /* SERIAL_H */
