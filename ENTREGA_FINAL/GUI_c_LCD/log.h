#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Categorias disponíveis para log_print() */
typedef enum {
    LOG_UART_TX = 0,
    LOG_UART_RX,
    LOG_FSM,
    LOG_CAM,
    LOG_DBG,
    LOG_SYS,
    LOG_ERR
} LogCategory;

/**
 * @brief Imprime uma linha de log com timestamp, categoria colorida e mensagem.
 *
 * Formato: HH:MM:SS.mmm  [CATEGORIA]  mensagem
 *
 * @param cat  Categoria (define prefixo e cor ANSI).
 * @param fmt  String de formato estilo printf.
 * @param ...  Argumentos variádicos correspondentes.
 */
void log_print(LogCategory cat, const char *fmt, ...);

/**
 * @brief Exibe o estado dos 4 sensores laser em formato visual.
 *
 * Cada sensor é mostrado como [OBJETO] (vermelho) ou [LIVRE] (verde)
 * conforme os bits 0-3 do byte de telemetria.
 *
 * @param flags  Byte de status: bit i = sensor (i+1); 1=objeto, 0=livre.
 */
void log_sensors(uint8_t flags);

/**
 * @brief Exibe o estado atual da FSM e o pipeline de estados no terminal.
 *
 * Imprime o nome colorido do estado ativo e uma linha visual do tipo:
 *   [IDLE]  >  OBJ  >  CLASS  >  ROTA A  >  ROTA B
 *
 * @param state_id  0=IDLE, 1=OBJ, 2=CLASS, 3=ROTA A, 4=ROTA B,
 *                  5=ENTREGUE A, 6=ENTREGUE B.
 */
void log_fsm_state(int state_id);

/**
 * @brief Imprime o menu de comandos do modo Debug.
 */
void log_debug_menu(void);

/**
 * @brief Imprime o banner de inicialização do programa.
 */
void log_banner(void);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
