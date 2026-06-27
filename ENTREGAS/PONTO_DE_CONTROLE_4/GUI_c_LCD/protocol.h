#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Protocolo proprietário binário — STM32G070 <-> Raspberry Pi 3B
 *
 * Frames TX (RPi -> STM32):
 *   [0xAA][CMD]          — 2 bytes (maioria dos comandos)
 *   [0xAA][0xE5][DATA]   — 3 bytes (SET_STPR_TGT_STPS)
 *
 * Frames RX (STM32 -> RPi):
 *   [0x90][CMD]          — confirmação ou evento (2 bytes)
 *   [0x91]               — erro (1 byte)
 *   [0x90][0x55][FLAGS]  — telemetria dos sensores (3 bytes) */

/* Bytes de enquadramento */
#define START_FRAME             0xAA
#define CMD_OK                  0x90
#define CMD_ERR                 0x91

/* Handshake */
#define SYS_RDY                 0x10
#define SYS_INIT                0x01

/* Rotas — enviadas pelo RPi ao STM32 */
#define ROUTE_A_SEND            0xDA
#define ROUTE_B_SEND            0xDB

/* Eventos — enviados pelo STM32 ao RPi */
#define OBJ_DETECTED            0xA0
#define CLSS_REQUEST            0xC0
#define ROUTE_A_FWD             0xFA
#define ROUTE_B_FWD             0xFB
#define ROUTE_A_OK              0xBA
#define ROUTE_B_OK              0xBB

/* Comandos assíncronos do modo Debug (RPi -> STM32) */
#define LIGHT_EN                0xE1
#define LIGHT_DIS               0xD1
#define GATE_OPEN               0xE2
#define GATE_CLOSE              0xD2
#define STPR_EN                 0xE3
#define STPR_DIS                0xD3
#define STPR_FORWARD            0xE4
#define STPR_BACKWARD           0xD4
#define SET_STPR_TGT_STPS       0xE5

/* Controle de modo */
#define DEBUG_TOGGLE            0xDD
#define MODE_FSM_MSG            0x11
#define MODE_DEBUG_MSG          0x22
#define SW_RESET_MSG            0x33

/* Telemetria de sensores */
#define SENS_STATUS_MSG         0x55

/* Frame recebido do STM32, já montado pelo parser */
typedef struct {
    uint8_t status;   /* CMD_OK (0x90) ou CMD_ERR (0x91) */
    uint8_t cmd;      /* byte de comando / payload        */
    uint8_t data;     /* byte extra (só SENS_STATUS_MSG)  */
    int     has_data; /* 1 se data é válido               */
} RxFrame;

/**
 * @brief Monta um frame TX de 2 bytes em buf.
 *
 * Formato: [START_FRAME][cmd].
 *
 * @param cmd  Byte de comando a enviar.
 * @param buf  Buffer de destino (mínimo 2 bytes).
 * @return     Número de bytes escritos (sempre 2).
 */
int protocol_build_frame(uint8_t cmd, uint8_t *buf);

/**
 * @brief Monta um frame TX de 3 bytes (com dado) em buf.
 *
 * Formato: [START_FRAME][cmd][data]. Usado para SET_STPR_TGT_STPS.
 *
 * @param cmd   Byte de comando a enviar.
 * @param data  Byte de dado associado.
 * @param buf   Buffer de destino (mínimo 3 bytes).
 * @return      Número de bytes escritos (sempre 3).
 */
int protocol_build_frame_data(uint8_t cmd, uint8_t data, uint8_t *buf);

/**
 * @brief Alimenta o parser RX com um byte recebido da UART.
 *
 * Máquina de 3 estados (WAIT_STATUS → WAIT_CMD → WAIT_DATA) que
 * espelha o parser do firmware. O estado é mantido entre chamadas
 * em variáveis estáticas internas — não é reentrante.
 *
 * @param byte  Byte recebido da porta serial.
 * @param out   Preenchido quando um frame completo é detectado.
 * @return      1 quando o frame está completo e *out foi preenchido;
 *              0 enquanto ainda aguarda mais bytes.
 */
int protocol_parse_byte(uint8_t byte, RxFrame *out);

/**
 * @brief Retorna o nome descritivo de um byte de comando RX.
 *
 * @param cmd  Byte de comando recebido do STM32.
 * @return     String estática com nome e valor hex, ou "DESCONHECIDO".
 */
const char *protocol_rx_name(uint8_t cmd);

/**
 * @brief Retorna o nome descritivo de um byte de comando TX.
 *
 * @param cmd  Byte de comando a enviar ao STM32.
 * @return     String estática com nome e valor hex, ou "DESCONHECIDO".
 */
const char *protocol_tx_name(uint8_t cmd);

/**
 * @brief Envia SW_RESET ao STM32, forçando um reset por software.
 *
 * Um novo handshake (do_handshake()) é necessário após o reset.
 *
 * @param fd  Descritor da porta serial.
 */
void deinit_stm32(int fd);

/**
 * @brief Executa o handshake de inicialização com o STM32.
 *
 * Envia SYS_RDY (0x10) e aguarda SYS_INIT (0x01) por até
 * HANDSHAKE_TIMEOUT_MS ms, emitindo heartbeats de espera via log.
 *
 * @param fd  Descritor da porta serial.
 * @return    0 em sucesso, -1 em timeout ou CMD_ERR recebido.
 */
int do_handshake(int fd);

#endif /* PROTOCOL_H */
