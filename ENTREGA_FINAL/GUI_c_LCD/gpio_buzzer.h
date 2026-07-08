#ifndef GPIO_BUZZER_H
#define GPIO_BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Módulo de buzzer GPIO — saída digital simples (liga/desliga).
 *
 * Usa libgpiod (API v1) para acionar uma linha GPIO como saída.
 *
 * Convenção de fiação: nível 1 = buzzer ligado, nível 0 = desligado.
 * Se o módulo de buzzer usado for ativo em nível baixo, inverta a
 * lógica dentro de buzzer_set() (gpio_buzzer.c). */

typedef struct GpioBuzzer GpioBuzzer;

/**
 * @brief Abre e configura uma linha GPIO como saída para o buzzer.
 *
 * Reserva a linha no chip indicado como saída, já iniciando em nível
 * baixo (buzzer desligado).
 *
 * @param chip_name    Nome do chip GPIO (ex: "gpiochip0").
 * @param line_offset  Número do pino GPIO (ex: 23 para GPIO23).
 * @return             Ponteiro para o contexto em sucesso, NULL em erro.
 */
GpioBuzzer *buzzer_open(const char *chip_name, unsigned int line_offset);

/**
 * @brief Desliga o buzzer e libera a linha GPIO e o contexto.
 *
 * Seguro chamar com NULL.
 *
 * @param buz  Contexto retornado por buzzer_open().
 */
void buzzer_close(GpioBuzzer *buz);

/**
 * @brief Liga ou desliga o buzzer.
 *
 * Seguro chamar com buz == NULL (não faz nada) — permite que o
 * chamador funcione normalmente mesmo sem o hardware do buzzer
 * conectado/aberto.
 *
 * @param buz  Contexto retornado por buzzer_open().
 * @param on   1 para ligar, 0 para desligar.
 */
void buzzer_set(GpioBuzzer *buz, int on);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_BUZZER_H */
