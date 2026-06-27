#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "serial.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

/**
 * @brief Converte baud rate em inteiro para a constante speed_t do termios.
 *
 * Cobre os valores mais comuns. Qualquer coisa fora da lista
 * produz um aviso em log e retorna B115200 como padrão seguro.
 *
 * @param baud  Taxa em bps (9600, 19200, 38400, 57600, 115200, 230400).
 * @return      Constante speed_t correspondente, ou B115200 se não suportado.
 */
static speed_t baud_to_speed(int baud)
{
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default:
            log_print(LOG_ERR,
                "Baud rate %d nao suportado -- usando 115200.", baud);
            return B115200;
    }
}

/**
 * @brief Abre e configura a porta serial em modo raw 8N1.
 *
 * @param device  Caminho do dispositivo, ex: "/dev/ttyS0".
 * @param baud    Taxa em bps desejada.
 * @return        Descritor de arquivo >= 0 em sucesso, -1 em erro.
 */
int serial_open(const char *device, int baud)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        log_print(LOG_ERR,
            "Falha ao abrir %s: %s", device, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        log_print(LOG_ERR,
            "tcgetattr em %s: %s", device, strerror(errno));
        close(fd);
        return -1;
    }

    speed_t spd = baud_to_speed(baud);
    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);

    /* 8N1 sem controle de fluxo */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |=  CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |=  CREAD | CLOCAL;

    /* Modo raw: sem processamento de entrada ou saída */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        log_print(LOG_ERR,
            "tcsetattr em %s: %s", device, strerror(errno));
        close(fd);
        return -1;
    }

    serial_flush(fd);
    log_print(LOG_SYS,
        "Serial OK: %s | fd=%d | %d bps | 8N1 | sem flow control",
        device, fd, baud);
    return fd;
}

/**
 * @brief Fecha a porta serial.
 *
 * @param fd  Descritor da porta. Ignorado se < 0.
 */
void serial_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

/**
 * @brief Envia exatamente 'len' bytes pela porta serial.
 *
 * @param fd   Descritor da porta serial.
 * @param buf  Buffer com os bytes a transmitir.
 * @param len  Quantidade de bytes a enviar.
 * @return     Bytes enviados (igual a 'len' em sucesso), -1 em erro.
 */
int serial_write(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            log_print(LOG_ERR,
                "serial_write: %s (%zu/%zu bytes enviados)",
                strerror(errno), sent, len);
            return -1;
        }
        sent += (size_t)n;
    }
    return (int)sent;
}

/**
 * @brief Lê até 'len' bytes disponíveis (não bloqueante).
 *
 * @param fd   Descritor da porta serial.
 * @param buf  Buffer de destino.
 * @param len  Capacidade do buffer.
 * @return     Bytes lidos; 0 se nenhum disponível; -1 em erro.
 */
int serial_read(int fd, uint8_t *buf, size_t len)
{
    ssize_t n = read(fd, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        perror("[serial] read");
        return -1;
    }
    return (int)n;
}

/**
 * @brief Lê exatamente 1 byte, aguardando até o timeout indicado.
 *
 * @param fd          Descritor da porta serial.
 * @param byte        Onde o byte lido será armazenado.
 * @param timeout_ms  Tempo máximo de espera em milissegundos.
 * @return            1 se leu, 0 em timeout, -1 em erro.
 */
int serial_read_byte_timeout(int fd, uint8_t *byte, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret < 0) {
        if (errno == EINTR) return 0;
        log_print(LOG_ERR, "serial select: %s", strerror(errno));
        return -1;
    }
    if (ret == 0)
        return 0;

    ssize_t n = read(fd, byte, 1);
    if (n <= 0) return -1;
    return 1;
}

/**
 * @brief Descarta os bytes pendentes no buffer de RX.
 *
 * @param fd  Descritor da porta serial.
 */
void serial_flush(int fd)
{
    tcflush(fd, TCIFLUSH);
}
