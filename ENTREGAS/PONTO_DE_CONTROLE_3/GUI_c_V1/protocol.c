#include "protocol.h"
#include "log.h"
#include <string.h>

/* =========================================================
 *  Estado interno do parser RX
 *  Máquina de estados de 3 estados, espelhando o firmware
 * ========================================================= */
typedef enum {
    PARSE_WAIT_STATUS = 0,
    PARSE_WAIT_CMD    = 1,
    PARSE_WAIT_DATA   = 2
} ParseState;

static ParseState s_parse_state = PARSE_WAIT_STATUS;
static uint8_t    s_status      = 0;
static uint8_t    s_cmd         = 0;

/* =========================================================
 *  Montagem de frames TX
 * ========================================================= */
int protocol_build_frame(uint8_t cmd, uint8_t *buf)
{
    buf[0] = START_FRAME;
    buf[1] = cmd;
    return 2;
}

int protocol_build_frame_data(uint8_t cmd, uint8_t data, uint8_t *buf)
{
    buf[0] = START_FRAME;
    buf[1] = cmd;
    buf[2] = data;
    return 3;
}

/* =========================================================
 *  Parser RX — alimentado byte a byte
 * ========================================================= */
int protocol_parse_byte(uint8_t byte, RxFrame *out)
{
    switch (s_parse_state) {

        case PARSE_WAIT_STATUS:
            if (byte == CMD_OK || byte == CMD_ERR) {
                s_status      = byte;
                s_parse_state = PARSE_WAIT_CMD;
                /* CMD_ERR é single-byte: entrega imediatamente */
                if (byte == CMD_ERR) {
                    s_parse_state  = PARSE_WAIT_STATUS;
                    out->status    = CMD_ERR;
                    out->cmd       = 0x00;
                    out->data      = 0x00;
                    out->has_data  = 0;
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
                /* frame de 3 bytes: aguarda DATA */
                s_parse_state = PARSE_WAIT_DATA;
            } else {
                /* frame de 2 bytes completo */
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

    return 0; /* frame ainda incompleto */
}

/* =========================================================
 *  Nomes descritivos para log
 * ========================================================= */
const char *protocol_rx_name(uint8_t cmd)
{
    switch (cmd) {
        case SYS_INIT:        return "SYS_INIT        (0x01)";
        case OBJ_DETECTED:    return "OBJ_DETECTED    (0xA0)";
        case CLSS_REQUEST:    return "CLSS_REQUEST    (0xC0)";
        case ROUTE_A_FWD:     return "ROUTE_A_FWD     (0xFA)";
        case ROUTE_B_FWD:     return "ROUTE_B_FWD     (0xFB)";
        case ROUTE_A_OK:      return "ROUTE_A_OK      (0xBA)";
        case ROUTE_B_OK:      return "ROUTE_B_OK      (0xBB)";
        case MODE_FSM_MSG:    return "MODE_FSM        (0x11)";
        case MODE_DEBUG_MSG:  return "MODE_DEBUG      (0x22)";
        case SENS_STATUS_MSG: return "SENS_STATUS     (0x55)";
        case LIGHT_EN:        return "LIGHT_EN  eco   (0xE1)";
        case LIGHT_DIS:       return "LIGHT_DIS eco   (0xD1)";
        case GATE_OPEN:       return "GATE_OPEN eco   (0xE2)";
        case GATE_CLOSE:      return "GATE_CLOSE eco  (0xD2)";
        case STPR_EN:         return "STPR_EN   eco   (0xE3)";
        case STPR_DIS:        return "STPR_DIS  eco   (0xD3)";
        case STPR_FORWARD:    return "STPR_FWD  eco   (0xE4)";
        case STPR_BACKWARD:   return "STPR_BWD  eco   (0xD4)";
        case SET_STPR_TGT_STPS: return "STPR_STPS eco (0xE5)";
        case DEBUG_TOGGLE:    return "DBG_TOGGLE eco  (0xDD)";
        case ROUTE_A_SEND:    return "ROUTE_A   eco   (0xDA)";
        case ROUTE_B_SEND:    return "ROUTE_B   eco   (0xDB)";
        case SW_RESET_MSG:    return "SW_RESET  eco   (0x33)";
        default:              return "DESCONHECIDO";
    }
}

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
