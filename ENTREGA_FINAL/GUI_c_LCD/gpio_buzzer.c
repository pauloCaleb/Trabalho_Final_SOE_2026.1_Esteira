#define _POSIX_C_SOURCE 200809L

#include "gpio_buzzer.h"
#include "log.h"

#include <gpiod.h>
#include <stdlib.h>

/* Estado interno da linha GPIO de saída do buzzer */
struct GpioBuzzer {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
};

/**
 * @brief Abre e configura uma linha GPIO como saída para o buzzer.
 *
 * @param chip_name    Nome do chip GPIO (ex: "gpiochip0").
 * @param line_offset  Número do pino GPIO.
 * @return             Ponteiro para o contexto em sucesso, NULL em erro.
 */
GpioBuzzer *buzzer_open(const char *chip_name, unsigned int line_offset)
{
    GpioBuzzer *buz = calloc(1, sizeof(GpioBuzzer));
    if (!buz) {
        log_print(LOG_ERR, "buzzer_open: falha ao alocar contexto.");
        return NULL;
    }

    buz->chip = gpiod_chip_open_by_name(chip_name);
    if (!buz->chip) {
        log_print(LOG_ERR, "buzzer_open: falha ao abrir chip '%s'.", chip_name);
        free(buz);
        return NULL;
    }

    buz->line = gpiod_chip_get_line(buz->chip, line_offset);
    if (!buz->line) {
        log_print(LOG_ERR,
            "buzzer_open: falha ao obter linha %u do chip '%s'.",
            line_offset, chip_name);
        gpiod_chip_close(buz->chip);
        free(buz);
        return NULL;
    }

    /* Já inicia em nível baixo (buzzer desligado) */
    if (gpiod_line_request_output(buz->line, "esteira-hmi", 0) != 0) {
        log_print(LOG_ERR,
            "buzzer_open: falha ao reservar linha %u como saida.",
            line_offset);
        gpiod_chip_close(buz->chip);
        free(buz);
        return NULL;
    }

    log_print(LOG_SYS,
        "Buzzer GPIO aberto: chip=%s linha=%u", chip_name, line_offset);
    return buz;
}

/**
 * @brief Desliga o buzzer e libera a linha GPIO e o contexto.
 *
 * @param buz  Contexto retornado por buzzer_open(). Seguro com NULL.
 */
void buzzer_close(GpioBuzzer *buz)
{
    if (!buz) return;
    if (buz->line) {
        gpiod_line_set_value(buz->line, 0); /* garante desligado ao sair */
        gpiod_line_release(buz->line);
    }
    if (buz->chip) gpiod_chip_close(buz->chip);
    free(buz);
}

/**
 * @brief Liga ou desliga o buzzer.
 *
 * @param buz  Contexto retornado por buzzer_open(). Seguro com NULL.
 * @param on   1 para ligar, 0 para desligar.
 */
void buzzer_set(GpioBuzzer *buz, int on)
{
    if (!buz || !buz->line) return;
    gpiod_line_set_value(buz->line, on ? 1 : 0);
}
