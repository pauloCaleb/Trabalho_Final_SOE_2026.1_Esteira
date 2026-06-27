#define _POSIX_C_SOURCE 200809L

#include "gpio_button.h"
#include "log.h"

#include <gpiod.h>
#include <stdlib.h>
#include <time.h>

/* Estado interno do debounce por linha GPIO */
struct GpioButton {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int                debounce_ms;

    int             last_level;      /* último nível lido (0/1) */
    int             falling_pending; /* 1 = viu borda de descida, aguardando debounce */
    struct timespec falling_ts;      /* instante da borda detectada */
    int             press_delivered; /* 1 = press já entregue nesta pressão */
};

/**
 * @brief Calcula a diferença em milissegundos entre dois timespecs.
 *
 * @param a  Instante mais recente.
 * @param b  Referência (mais antigo).
 * @return   (a - b) em milissegundos.
 */
static long ms_diff(const struct timespec *a, const struct timespec *b)
{
    long sec_diff  = a->tv_sec  - b->tv_sec;
    long nsec_diff = a->tv_nsec - b->tv_nsec;
    return sec_diff * 1000L + nsec_diff / 1000000L;
}

/**
 * @brief Abre e configura uma linha GPIO como entrada de botão.
 *
 * @param chip_name     Nome do chip GPIO (ex: "gpiochip0").
 * @param line_offset   Número do pino GPIO.
 * @param debounce_ms   Tempo mínimo de estabilidade para aceitar um press (ms).
 * @return              Ponteiro para o contexto em sucesso, NULL em erro.
 */
GpioButton *button_open(const char *chip_name, unsigned int line_offset,
                         int debounce_ms)
{
    GpioButton *btn = calloc(1, sizeof(GpioButton));
    if (!btn) {
        log_print(LOG_ERR, "button_open: falha ao alocar contexto.");
        return NULL;
    }

    btn->chip = gpiod_chip_open_by_name(chip_name);
    if (!btn->chip) {
        log_print(LOG_ERR, "button_open: falha ao abrir chip '%s'.", chip_name);
        free(btn);
        return NULL;
    }

    btn->line = gpiod_chip_get_line(btn->chip, line_offset);
    if (!btn->line) {
        log_print(LOG_ERR,
            "button_open: falha ao obter linha %u do chip '%s'.",
            line_offset, chip_name);
        gpiod_chip_close(btn->chip);
        free(btn);
        return NULL;
    }

    /* Sem flag de bias: pull-up é externo, não queremos interferência
     * do bias interno do RPi nessa linha. */
    if (gpiod_line_request_input_flags(btn->line, "esteira-hmi", 0) != 0) {
        log_print(LOG_ERR,
            "button_open: falha ao reservar linha %u como entrada.",
            line_offset);
        gpiod_chip_close(btn->chip);
        free(btn);
        return NULL;
    }

    btn->debounce_ms    = debounce_ms > 0 ? debounce_ms : 0;
    btn->last_level      = 1; /* repouso = alto (pull-up) */
    btn->falling_pending = 0;
    btn->press_delivered = 0;

    log_print(LOG_SYS,
        "Botao GPIO aberto: chip=%s linha=%u debounce=%d ms",
        chip_name, line_offset, btn->debounce_ms);
    return btn;
}

/**
 * @brief Libera a linha GPIO e o contexto do botão.
 *
 * @param btn  Contexto retornado por button_open(). Seguro com NULL.
 */
void button_close(GpioButton *btn)
{
    if (!btn) return;
    if (btn->line) gpiod_line_release(btn->line);
    if (btn->chip) gpiod_chip_close(btn->chip);
    free(btn);
}

/**
 * @brief Verifica se houve um pressionamento válido desde a última chamada.
 *
 * @param btn  Contexto retornado por button_open().
 * @return     1 se um press com debounce foi detectado, 0 caso contrário.
 */
int button_was_pressed(GpioButton *btn)
{
    if (!btn || !btn->line) return 0;

    int level = gpiod_line_get_value(btn->line);
    if (level < 0) return 0; /* erro de leitura — não trava o chamador */

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Borda de descida: início de um possível pressionamento */
    if (btn->last_level == 1 && level == 0) {
        btn->falling_pending = 1;
        btn->falling_ts      = now;
        btn->press_delivered = 0;
    }

    /* Nível voltou a alto: pressão encerrada, reseta o rastreamento */
    if (level == 1) {
        btn->falling_pending = 0;
        btn->press_delivered = 0;
    }

    int reported = 0;

    /* Nível baixo estável por mais de debounce_ms: reporta uma vez */
    if (level == 0 && btn->falling_pending && !btn->press_delivered) {
        if (ms_diff(&now, &btn->falling_ts) >= btn->debounce_ms) {
            btn->press_delivered = 1;
            reported = 1;
        }
    }

    btn->last_level = level;
    return reported;
}
