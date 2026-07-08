#include "protocol.h"
#include "serial.h"
#include "log.h"
#include <string.h>

/* Parser RX — máquina de 3 estados, espelhando o firmware */
typedef enum {
    PARSE_WAIT_STATUS = 0,
    PARSE_WAIT_CMD    = 1,
    PARSE_WAIT_DATA   = 2
} ParseState;

static ParseState s_parse_state = PARSE_WAIT_STATUS;
static uint8_t    s_status      = 0;
static uint8_t    s_cmd         = 0;

/**
 * @brief Monta um frame TX de 2 bytes em buf.
 *
 * @param cmd  Byte de comando a enviar.
 * @param buf  Buffer de destino (mínimo 2 bytes).
 * @return     Número de bytes escritos (sempre 2).
 */
int protocol_build_frame(uint8_t cmd, uint8_t *buf)
{
    buf[0] = START_FRAME;
    buf[1] = cmd;
    return 2;
}

/**
 * @brief Monta um frame TX de 3 bytes (com dado) em buf.
 *
 * @param cmd   Byte de comando a enviar.
 * @param data  Byte de dado associado.
 * @param buf   Buffer de destino (mínimo 3 bytes).
 * @return      Número de bytes escritos (sempre 3).
 */
int protocol_build_frame_data(uint8_t cmd, uint8_t data, uint8_t *buf)
{
    buf[0] = START_FRAME;
    buf[1] = cmd;
    buf[2] = data;
    return 3;
}

/**
 * @brief Alimenta o parser RX com um byte recebido da UART.
 *
 * Transições de estado:
 *   WAIT_STATUS: aguarda 0x90 (CMD_OK) ou 0x91 (CMD_ERR).
 *     0x91 entrega o frame imediatamente (1 byte).
 *   WAIT_CMD: guarda o comando; se for SENS_STATUS_MSG (0x55),
 *     avança para WAIT_DATA; caso contrário entrega o frame (2 bytes).
 *   WAIT_DATA: lê o byte extra e entrega o frame (3 bytes).
 *
 * Bytes fora de qualquer padrão esperado são descartados com log de erro.
 *
 * @param byte  Byte recebido da porta serial.
 * @param out   Preenchido quando um frame completo é detectado.
 * @return      1 se frame completo, 0 se ainda incompleto.
 */
int protocol_parse_byte(uint8_t byte, RxFrame *out)
{
    switch (s_parse_state) {

        case PARSE_WAIT_STATUS:
            if (byte == CMD_OK || byte == CMD_ERR) {
                s_status      = byte;
                s_parse_state = PARSE_WAIT_CMD;
                if (byte == CMD_ERR) {
                    /* CMD_ERR é frame de 1 byte — entrega na hora */
                    s_parse_state = PARSE_WAIT_STATUS;
                    out->status   = CMD_ERR;
                    out->cmd      = 0x00;
                    out->data     = 0x00;
                    out->has_data = 0;
                    return 1;
                }
            } else {
                log_print(LOG_ERR,
                    "Byte fora de frame descartado: 0x%02X (esperado 0x90 ou 0x91)",
                    byte);
            }
            break;

        case PARSE_WAIT_CMD:
            s_cmd = byte;
            if (byte == SENS_STATUS_MSG) {
                s_parse_state = PARSE_WAIT_DATA;
            } else {
                s_parse_state = PARSE_WAIT_STATUS;
                out->status   = s_status;
                out->cmd      = s_cmd;
                out->data     = 0x00;
                out->has_data = 0;
                return 1;
            }
            break;

        case PARSE_WAIT_DATA:
            s_parse_state = PARSE_WAIT_STATUS;
            out->status   = s_status;
            out->cmd      = s_cmd;
            out->data     = byte;
            out->has_data = 1;
            return 1;
    }

    return 0;
}

/**
 * @brief Retorna o nome descritivo de um byte de comando RX.
 *
 * @param cmd  Byte recebido do STM32.
 * @return     String estática com nome e valor hex, ou "DESCONHECIDO".
 */
const char *protocol_rx_name(uint8_t cmd)
{
    switch (cmd) {
        case SYS_INIT:          return "SYS_INIT        (0x01)";
        case OBJ_DETECTED:      return "OBJ_DETECTED    (0xA0)";
        case CLSS_REQUEST:      return "CLSS_REQUEST    (0xC0)";
        case ROUTE_A_FWD:       return "ROUTE_A_FWD     (0xFA)";
        case ROUTE_B_FWD:       return "ROUTE_B_FWD     (0xFB)";
        case ROUTE_A_OK:        return "ROUTE_A_OK      (0xBA)";
        case ROUTE_B_OK:        return "ROUTE_B_OK      (0xBB)";
        case MODE_FSM_MSG:      return "MODE_FSM        (0x11)";
        case MODE_DEBUG_MSG:    return "MODE_DEBUG      (0x22)";
        case SENS_STATUS_MSG:   return "SENS_STATUS     (0x55)";
        case LIGHT_EN:          return "LIGHT_EN  eco   (0xE1)";
        case LIGHT_DIS:         return "LIGHT_DIS eco   (0xD1)";
        case GATE_OPEN:         return "GATE_OPEN eco   (0xE2)";
        case GATE_CLOSE:        return "GATE_CLOSE eco  (0xD2)";
        case STPR_EN:           return "STPR_EN   eco   (0xE3)";
        case STPR_DIS:          return "STPR_DIS  eco   (0xD3)";
        case STPR_FORWARD:      return "STPR_FWD  eco   (0xE4)";
        case STPR_BACKWARD:     return "STPR_BWD  eco   (0xD4)";
        case SET_STPR_TGT_STPS: return "STPR_STPS eco   (0xE5)";
        case DEBUG_TOGGLE:      return "DBG_TOGGLE eco  (0xDD)";
        case ROUTE_A_SEND:      return "ROUTE_A   eco   (0xDA)";
        case ROUTE_B_SEND:      return "ROUTE_B   eco   (0xDB)";
        case SW_RESET_MSG:      return "SW_RESET  eco   (0x33)";
        default:                return "DESCONHECIDO";
    }
}

/**
 * @brief Retorna o nome descritivo de um byte de comando TX.
 *
 * @param cmd  Byte enviado ao STM32.
 * @return     String estática com nome e valor hex, ou "DESCONHECIDO".
 */
const char *protocol_tx_name(uint8_t cmd)
{
    switch (cmd) {
        case SYS_RDY:           return "SYS_RDY         (0x10)";
        case ROUTE_A_SEND:      return "ROUTE_A         (0xDA)";
        case ROUTE_B_SEND:      return "ROUTE_B         (0xDB)";
        case LIGHT_EN:          return "LIGHT_EN        (0xE1)";
        case LIGHT_DIS:         return "LIGHT_DIS       (0xD1)";
        case GATE_OPEN:         return "GATE_OPEN       (0xE2)";
        case GATE_CLOSE:        return "GATE_CLOSE      (0xD2)";
        case STPR_EN:           return "STPR_EN         (0xE3)";
        case STPR_DIS:          return "STPR_DIS        (0xD3)";
        case STPR_FORWARD:      return "STPR_FORWARD    (0xE4)";
        case STPR_BACKWARD:     return "STPR_BACKWARD   (0xD4)";
        case SET_STPR_TGT_STPS: return "STPR_TGT_STPS   (0xE5)";
        case DEBUG_TOGGLE:      return "DEBUG_TOGGLE    (0xDD)";
        case SW_RESET_MSG:      return "SW_RESET        (0x33)";
        default:                return "DESCONHECIDO";
    }
}

/**
 * @brief Envia SW_RESET ao STM32, forçando um reset por software.
 *
 * @param fd  Descritor da porta serial.
 */
void deinit_stm32(int fd)
{
    uint8_t buf[2];
    int len = protocol_build_frame(SW_RESET_MSG, buf);
    serial_write(fd, buf, len);
}

#define HANDSHAKE_TIMEOUT_MS 10000

/**
 * @brief Envia SYS_RDY ao STM32 sem aguardar resposta.
 *
 * @param fd  Descritor da porta serial.
 */
void handshake_send_ready(int fd)
{
    uint8_t buf[2];
    int len = protocol_build_frame(SYS_RDY, buf);
    serial_write(fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x10]  %s", protocol_tx_name(SYS_RDY));
}

/**
 * @brief Verifica, sem bloquear além do timeout interno de leitura, se o
 *        STM32 já respondeu ao handshake iniciado por handshake_send_ready().
 *
 * @param fd  Descritor da porta serial.
 * @return    1 se SYS_INIT chegou, -1 se CMD_ERR, 0 caso contrário.
 */
int handshake_poll(int fd)
{
    uint8_t byte;
    RxFrame frame;

    int r = serial_read_byte_timeout(fd, &byte, 100);
    if (r <= 0) return 0;

    if (protocol_parse_byte(byte, &frame)) {
        log_print(LOG_UART_RX,
            "<< RX [0x%02X][0x%02X]  %s",
            frame.status, frame.cmd, protocol_rx_name(frame.cmd));

        if (frame.status == CMD_OK && frame.cmd == SYS_INIT) {
            log_print(LOG_SYS, "Handshake OK -- sistema inicializado.");
            return 1;
        }
        if (frame.status == CMD_ERR) {
            log_print(LOG_ERR, "STM32 respondeu CMD_ERR durante handshake.");
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Executa o handshake de inicialização com o STM32.
 *
 * Envia SYS_RDY e aguarda SYS_INIT por até HANDSHAKE_TIMEOUT_MS ms.
 *
 * @param fd  Descritor da porta serial.
 * @return    0 em sucesso, -1 em timeout ou CMD_ERR recebido.
 */
int do_handshake(int fd)
{
    log_print(LOG_SYS,
        "Enviando SYS_RDY (0x10) -- aguardando SYS_INIT...");

    uint8_t buf[2];
    int len = protocol_build_frame(SYS_RDY, buf);
    serial_write(fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x10]  %s", protocol_tx_name(SYS_RDY));

    int elapsed      = 0;
    int hs_heartbeat = 0;
    uint8_t byte;
    RxFrame frame;

    while (elapsed < HANDSHAKE_TIMEOUT_MS) {
        int r = serial_read_byte_timeout(fd, &byte, 100);
        elapsed += 100;
        if (r <= 0) {
            if (++hs_heartbeat % 20 == 0)
                log_print(LOG_SYS,
                    "Aguardando SYS_INIT do STM32... (%d ms / %d ms)",
                    elapsed, HANDSHAKE_TIMEOUT_MS);
            continue;
        }

        if (protocol_parse_byte(byte, &frame)) {
            log_print(LOG_UART_RX,
                "<< RX [0x%02X][0x%02X]  %s",
                frame.status, frame.cmd,
                protocol_rx_name(frame.cmd));

            if (frame.status == CMD_OK && frame.cmd == SYS_INIT) {
                log_print(LOG_SYS, "Handshake OK -- sistema inicializado.");
                return 0;
            }
            if (frame.status == CMD_ERR) {
                log_print(LOG_ERR,
                    "STM32 respondeu CMD_ERR durante handshake.");
                return -1;
            }
        }
    }

    log_print(LOG_ERR, "Timeout no handshake (%d ms).", HANDSHAKE_TIMEOUT_MS);
    return -1;
}
