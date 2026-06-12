#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_UART_TX = 0,
    LOG_UART_RX,
    LOG_FSM,
    LOG_CAM,
    LOG_DBG,
    LOG_SYS,
    LOG_ERR
} LogCategory;

void log_print(LogCategory cat, const char *fmt, ...);
void log_sensors(uint8_t flags);
void log_fsm_state(int state_id);
void log_debug_menu(void);
void log_banner(void);

#endif /* LOG_H */

#ifdef __cplusplus
}
#endif
