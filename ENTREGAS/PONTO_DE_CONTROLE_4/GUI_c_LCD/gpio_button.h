#ifndef GPIO_BUTTON_H
#define GPIO_BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Módulo de botão GPIO — polling com debounce em software.
 *
 * Usa libgpiod (API v1) para ler o nível de uma linha GPIO
 * configurada como entrada sem bias interno (pull-up é externo).
 *
 * Convenção de fiação: botão liga o pino ao GND quando pressionado
 * (nível 0 = pressionado, nível 1 = solto, pull-up externo). */

typedef struct GpioButton GpioButton;

/**
 * @brief Abre e configura uma linha GPIO como entrada de botão.
 *
 * Reserva a linha no chip indicado como entrada (sem bias interno)
 * e inicializa o estado de debounce.
 *
 * @param chip_name     Nome do chip GPIO (ex: "gpiochip0").
 * @param line_offset   Número do pino GPIO (ex: 17 para GPIO17).
 * @param debounce_ms   Tempo mínimo de estabilidade em nível baixo
 *                      para aceitar um pressionamento (ms).
 * @return              Ponteiro para o contexto em sucesso, NULL em erro.
 */
GpioButton *button_open(const char *chip_name, unsigned int line_offset,
                         int debounce_ms);

/**
 * @brief Libera a linha GPIO e o contexto do botão.
 *
 * Seguro chamar com NULL.
 *
 * @param btn  Contexto retornado por button_open().
 */
void button_close(GpioButton *btn);

/**
 * @brief Verifica se houve um pressionamento válido desde a última chamada.
 *
 * Deve ser chamada periodicamente (polling). Detecta borda de descida
 * (1→0) e só reporta o evento após 'debounce_ms' estáveis em nível baixo.
 * Cada pressionamento é reportado uma única vez, independentemente de
 * quanto tempo o botão permaneça pressionado.
 *
 * @param btn  Contexto retornado por button_open().
 * @return     1 se um pressionamento válido foi detectado, 0 caso contrário.
 */
int button_was_pressed(GpioButton *btn);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_BUTTON_H */
