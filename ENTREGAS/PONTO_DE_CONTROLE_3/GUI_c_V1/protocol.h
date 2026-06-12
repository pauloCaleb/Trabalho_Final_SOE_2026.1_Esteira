#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* =========================================================
 *  QUADRO DE COMUNICAÇÃO
 *  Firmware STM32G070 v3 <-> Raspberry Pi 3B
 *
 *  Formato TX (PC -> STM32):
 *    [0xAA][CMD]          — frame de 2 bytes
 *    [0xAA][0xE5][DATA]   — frame de 3 bytes (SET_STPR_TGT_STPS)
 *
 *  Formato RX (STM32 -> PC):
 *    [0x90][CMD]          — CMD_OK  com payload
 *    [0x91]               — CMD_ERR single byte
 *    [0x90][0x55][STATUS] — telemetria de sensores (3 bytes)
 * ========================================================= */

/* --- Bytes de enquadramento --- */
#define START_FRAME             0xAA
#define CMD_OK                  0x90
#define CMD_ERR                 0x91

/* --- Handshake --- */
#define SYS_RDY                 0x10
#define SYS_INIT                0x01

/* --- FSM RX (PC -> STM32) --- */
#define ROUTE_A_SEND            0xDA
#define ROUTE_B_SEND            0xDB

/* --- FSM TX (STM32 -> PC) --- */
#define OBJ_DETECTED            0xA0
#define CLSS_REQUEST            0xC0
#define ROUTE_A_FWD             0xFA
#define ROUTE_B_FWD             0xFB
#define ROUTE_A_OK              0xBA
#define ROUTE_B_OK              0xBB

/* --- Controle assíncrono RX (PC -> STM32) --- */
#define LIGHT_EN                0xE1
#define LIGHT_DIS               0xD1
#define GATE_OPEN               0xE2
#define GATE_CLOSE              0xD2
#define STPR_EN                 0xE3
#define STPR_DIS                0xD3
#define STPR_FORWARD            0xE4
#define STPR_BACKWARD           0xD4
#define SET_STPR_TGT_STPS       0xE5

/* --- Modo de operação --- */
#define DEBUG_TOGGLE            0xDD
#define MODE_FSM_MSG            0x11
#define MODE_DEBUG_MSG          0x22
#define SW_RESET_MSG            0x33

/* --- Telemetria --- */
#define SENS_STATUS_MSG         0x55

/* =========================================================
 *  Estruturas de frame
 * ========================================================= */

/* Frame recebido do STM32 (já parseado) */
typedef struct {
    uint8_t status;   /* CMD_OK (0x90) ou CMD_ERR (0x91) */
    uint8_t cmd;      /* byte de comando / payload        */
    uint8_t data;     /* byte extra (só SENS_STATUS_MSG)  */
    int     has_data; /* 1 se data é válido               */
} RxFrame;

/* =========================================================
 *  Funções
 * ========================================================= */

/**
 * Monta um frame TX de 2 bytes em buf.
 * Retorna o número de bytes escritos (sempre 2).
 */
int protocol_build_frame(uint8_t cmd, uint8_t *buf);

/**
 * Monta um frame TX de 3 bytes (com data) em buf.
 * Retorna o número de bytes escritos (sempre 3).
 */
int protocol_build_frame_data(uint8_t cmd, uint8_t data, uint8_t *buf);

/**
 * Alimenta o parser com um byte.
 * Quando um frame completo é detectado, preenche *out e retorna 1.
 * Retorna 0 enquanto o frame ainda não está completo.
 */
int protocol_parse_byte(uint8_t byte, RxFrame *out);

/**
 * Retorna uma string descritiva para um byte de comando RX.
 * Útil para o log no terminal.
 */
const char *protocol_rx_name(uint8_t cmd);

/**
 * Retorna uma string descritiva para um byte de comando TX.
 */
const char *protocol_tx_name(uint8_t cmd);

#endif /* PROTOCOL_H */
