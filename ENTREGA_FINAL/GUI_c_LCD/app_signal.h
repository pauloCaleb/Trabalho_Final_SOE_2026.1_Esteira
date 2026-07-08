#ifndef APP_SIGNAL_H
#define APP_SIGNAL_H

/* Flag global de interrupção da aplicação.
 *
 * Setada para 1 pelo handler de SIGINT/SIGTERM em main.c.
 * Os loops de fsm_run(), debug_run() e hmi_run() verificam
 * esta flag periodicamente para encerrar de forma controlada,
 * garantindo que deinit_stm32() seja chamado antes de sair. */
extern volatile int g_interrupted;

#endif /* APP_SIGNAL_H */
