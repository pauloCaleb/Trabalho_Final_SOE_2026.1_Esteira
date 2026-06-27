#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* Módulo serial — POSIX (Linux / Raspberry Pi)
 * Camada de acesso à porta serial em modo raw 8N1,
 * compatível com a USART2 do STM32G070 a 115200 bps. */

/**
 * @brief Abre e configura a porta serial em modo raw 8N1.
 *
 * Abre o dispositivo com O_NONBLOCK, configura os atributos
 * termios (8 bits, sem paridade, 1 stop bit, sem controle de
 * fluxo) e limpa o buffer de RX antes de retornar.
 *
 * @param device  Caminho do dispositivo, ex: "/dev/ttyS0".
 * @param baud    Taxa em bps (ex: 115200). Valores não suportados
 *                caem automaticamente em 115200.
 * @return        Descritor de arquivo >= 0 em sucesso, -1 em erro.
 */
int serial_open(const char *device, int baud);

/**
 * @brief Fecha a porta serial.
 *
 * @param fd  Descritor retornado por serial_open(). Ignorado se < 0.
 */
void serial_close(int fd);

/**
 * @brief Envia exatamente 'len' bytes pela porta serial.
 *
 * Chama write() em loop até enviar tudo, recuperando interrupções
 * (EINTR) automaticamente.
 *
 * @param fd   Descritor da porta serial.
 * @param buf  Buffer com os bytes a transmitir.
 * @param len  Quantidade de bytes a enviar.
 * @return     Bytes enviados (igual a 'len' em sucesso), -1 em erro.
 */
int serial_write(int fd, const uint8_t *buf, size_t len);

/**
 * @brief Lê até 'len' bytes disponíveis (não bloqueante).
 *
 * @param fd   Descritor da porta serial.
 * @param buf  Buffer de destino.
 * @param len  Capacidade do buffer.
 * @return     Bytes lidos; 0 se nenhum disponível no momento; -1 em erro.
 */
int serial_read(int fd, uint8_t *buf, size_t len);

/**
 * @brief Lê exatamente 1 byte, aguardando até o timeout indicado.
 *
 * Usa select() para não bloquear indefinidamente a thread chamadora.
 *
 * @param fd          Descritor da porta serial.
 * @param byte        Ponteiro onde o byte lido será armazenado.
 * @param timeout_ms  Tempo máximo de espera em milissegundos.
 * @return            1 se leu, 0 em timeout, -1 em erro.
 */
int serial_read_byte_timeout(int fd, uint8_t *byte, int timeout_ms);

/**
 * @brief Descarta os bytes pendentes no buffer de RX.
 *
 * @param fd  Descritor da porta serial.
 */
void serial_flush(int fd);

#endif /* SERIAL_H */
