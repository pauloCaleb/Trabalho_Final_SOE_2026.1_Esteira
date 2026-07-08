#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "hmi.h"
#include "protocol.h"
#include "serial.h"
#include "camera.h"
#include "log.h"
#include "lcd2004_i2c.h"
#include "gpio_button.h"
#include "gpio_buzzer.h"
#include "app_signal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Estados internos da operação autônoma (espelham a FSM do firmware) */
typedef enum {
    HMI_OP_IDLE         = 0,
    HMI_OP_OBJ_DETECTED = 1,
    HMI_OP_CLASSIFYING  = 2,
    HMI_OP_ROUTE_A      = 3,
    HMI_OP_ROUTE_B      = 4,
    HMI_OP_DELIVERED_A  = 5,
    HMI_OP_DELIVERED_B  = 6
} HmiOpState;

/* Contadores de objetos entregues em cada rota, expostos no display.
 * Persistem entre ciclos de operação; só são zerados quando o usuário
 * pressiona START enquanto o sistema está em execução (ver hmi_operate). */
static unsigned long s_count_a = 0;
static unsigned long s_count_b = 0;

/* Parâmetros do alerta de "objeto não identificado": o backlight do LCD
 * e o buzzer (GPIO23, ver HMI_BUZZER_LINE em hmi.h) piscam/apitam juntos,
 * alternando a cada HMI_ALERT_BLINK_MS, indefinidamente, até o operador
 * confirmar com START (o que dispara reset + handshake do STM32) ou
 * encerrar com STOP. Não há timeout automático: o objetivo é que o
 * alerta seja impossível de ignorar. */
#define HMI_ALERT_BLINK_MS      300

/* Retorno de hmi_wait_for_operator_ack(): o que encerrou a espera. */
#define HMI_ALERT_ACK           0  /* START pressionado -- operador confirmou */
#define HMI_ALERT_STOPPED       1  /* STOP pressionado durante o alerta */
#define HMI_ALERT_INTERRUPTED   2  /* Ctrl+C (g_interrupted) no alerta  */

/* Contexto compartilhado com a thread RX */
typedef struct {
    int             fd;
    volatile int    running;
    volatile int    rx_event;
    pthread_mutex_t lock;
} HmiCtx;

/**
 * @brief Thread de recepção UART do modo HMI.
 *
 * Mesma estrutura da thread RX do modo FSM: alimenta o parser byte a
 * byte e armazena o comando recebido em ctx->rx_event (sob mutex).
 *
 * @param arg  Ponteiro para HmiCtx.
 * @return     NULL (API pthread).
 */
static void *hmi_rx_thread(void *arg)
{
    HmiCtx *ctx = (HmiCtx *)arg;
    uint8_t byte;
    RxFrame frame;

    log_print(LOG_SYS, "Thread RX (HMI) iniciada.");

    while (ctx->running) {
        int r = serial_read_byte_timeout(ctx->fd, &byte, 100);
        if (r <= 0) continue;

        if (protocol_parse_byte(byte, &frame)) {
            if (frame.status == CMD_ERR) {
                log_print(LOG_ERR, "CMD_ERR recebido do STM32");
                continue;
            }
            log_print(LOG_UART_RX,
                "<< RX [0x%02X][0x%02X]  %s",
                frame.status, frame.cmd,
                protocol_rx_name(frame.cmd));

            pthread_mutex_lock(&ctx->lock);
            ctx->rx_event = frame.cmd;
            pthread_mutex_unlock(&ctx->lock);
        }
    }

    log_print(LOG_SYS, "Thread RX (HMI) encerrada.");
    return NULL;
}

/**
 * @brief Monta e envia um frame TX de 2 bytes ao STM32, com log.
 *
 * @param ctx  Contexto HMI (fornece o fd serial).
 * @param cmd  Byte de comando a enviar.
 */
static void hmi_send(HmiCtx *ctx, uint8_t cmd)
{
    uint8_t buf[2];
    int len = protocol_build_frame(cmd, buf);
    serial_write(ctx->fd, buf, len);
    log_print(LOG_UART_TX,
        ">> TX [0xAA][0x%02X]  %s",
        cmd, protocol_tx_name(cmd));
}

/**
 * @brief Consome o último evento RX pendente, zerando o campo.
 *
 * @param ctx  Contexto HMI.
 * @return     Byte do comando recebido (0 se nenhum evento novo).
 */
static int hmi_consume_event(HmiCtx *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    int ev = ctx->rx_event;
    ctx->rx_event = 0;
    pthread_mutex_unlock(&ctx->lock);
    return ev;
}

/**
 * @brief Formata a linha de contadores "A:<n> B:<n>" em buf.
 *
 * @param buf      Buffer de saída (mínimo LCD2004_COLS + 1 bytes).
 * @param buf_len  Tamanho de buf.
 */
static void hmi_format_counts(char *buf, size_t buf_len)
{
    snprintf(buf, buf_len, "A:%-4lu B:%-4lu", s_count_a, s_count_b);
}

/**
 * @brief Zera os contadores de objetos entregues em cada rota.
 *
 * Chamada quando o usuário pressiona START durante a operação
 * (ver hmi_operate()) -- não afeta o ciclo de operação em andamento.
 */
static void hmi_reset_counts(void)
{
    s_count_a = 0;
    s_count_b = 0;
    log_print(LOG_SYS,
        "HMI: contadores de objetos zerados (START pressionado em operacao).");
}

/**
 * @brief Atualiza as 4 linhas do display com o estado atual da operação.
 *
 * Usa lcd_print_line() para sobrescrever cada linha sem lcd_clear(),
 * evitando flicker.
 *
 * @param state  Sub-estado atual da operação autônoma.
 */
static void hmi_display_state(HmiOpState state)
{
    const char *txt;
    switch (state) {
        case HMI_OP_IDLE:         txt = "Aguardando objeto"; break;
        case HMI_OP_OBJ_DETECTED: txt = "Objeto detectado";  break;
        case HMI_OP_CLASSIFYING:  txt = "Classificando...";  break;
        case HMI_OP_ROUTE_A:      txt = "Rota A";             break;
        case HMI_OP_ROUTE_B:      txt = "Rota B";             break;
        case HMI_OP_DELIVERED_A:  txt = "Entregue - Rota A";  break;
        case HMI_OP_DELIVERED_B:  txt = "Entregue - Rota B";  break;
        default:                  txt = "--";                 break;
    }

    char counts[LCD2004_COLS + 1];
    hmi_format_counts(counts, sizeof(counts));

    lcd_print_line(0, "ESTEIRA - OPERANDO");
    lcd_print_line(1, txt);
    lcd_print_line(2, "STOP:sai  START:zera");
    lcd_print_line(3, counts);
}

/**
 * @brief Exibe a tela de espera (antes do handshake/START).
 */
static void hmi_display_waiting(void)
{
    char counts[LCD2004_COLS + 1];
    hmi_format_counts(counts, sizeof(counts));

    lcd_print_line(0, "ESTEIRA SEPARADORA");
    lcd_print_line(1, "SOE 2026.1");
    lcd_print_line(2, counts);
    lcd_print_line(3, "Aperte START");
}

/**
 * @brief Exibe uma mensagem transitória de 2 linhas no display.
 *
 * @param l0  Texto da linha 0.
 * @param l1  Texto da linha 1.
 */
static void hmi_display_msg(const char *l0, const char *l1)
{
    lcd_print_line(0, l0);
    lcd_print_line(1, l1);
    lcd_print_line(2, "");
    lcd_print_line(3, "");
}

/**
 * @brief Exibe um alerta de 3 linhas e aguarda confirmação do operador.
 *
 * Limpa a tela, mostra "ESTEIRA - ALERTA" na primeira linha seguida das
 * 3 linhas fornecidas, e alterna backlight + buzzer em sincronia a cada
 * HMI_ALERT_BLINK_MS, indefinidamente, até que o operador pressione
 * START (confirma o alerta) ou STOP (encerra a operação). Ctrl+C também
 * é verificado a cada iteração. Reaproveitada tanto para o alerta de
 * objeto não identificado quanto para o de handshake sem resposta.
 *
 * @param msg_l1     Texto da linha 1 (logo abaixo do título).
 * @param msg_l2     Texto da linha 2.
 * @param msg_l3     Texto da linha 3 (normalmente a instrução de ação).
 * @param btn_start  Botão START já aberto.
 * @param btn_stop   Botão STOP já aberto.
 * @param buzzer     Contexto do buzzer (pode ser NULL — o alerta segue
 *                   funcionando apenas com o backlight piscando).
 * @return  HMI_ALERT_ACK, HMI_ALERT_STOPPED ou HMI_ALERT_INTERRUPTED.
 */
static int hmi_wait_for_operator_ack(const char *msg_l1, const char *msg_l2,
                                      const char *msg_l3,
                                      GpioButton *btn_start, GpioButton *btn_stop,
                                      GpioBuzzer *buzzer)
{
    lcd_clear();
    lcd_print_line(0, "ESTEIRA - ALERTA");
    lcd_print_line(1, msg_l1);
    lcd_print_line(2, msg_l2);
    lcd_print_line(3, msg_l3);

    int blink_on = 0;
    int result;

    for (;;) {
        if (g_interrupted) {
            result = HMI_ALERT_INTERRUPTED;
            break;
        }
        if (button_was_pressed(btn_stop)) {
            log_print(LOG_SYS,
                "HMI: STOP pressionado durante alerta -- encerrando operacao.");
            result = HMI_ALERT_STOPPED;
            break;
        }
        if (button_was_pressed(btn_start)) {
            log_print(LOG_SYS, "HMI: START pressionado durante alerta.");
            result = HMI_ALERT_ACK;
            break;
        }

        blink_on = !blink_on;
        lcd_set_backlight(blink_on);
        buzzer_set(buzzer, blink_on);
        usleep(HMI_ALERT_BLINK_MS * 1000);
    }

    lcd_set_backlight(1);  /* garante backlight aceso ao sair do alerta */
    buzzer_set(buzzer, 0); /* garante buzzer desligado ao sair do alerta */
    return result;
}

/**
 * @brief Executa o ciclo de operação autônoma (pós-handshake).
 *
 * A câmera já deve estar inicializada por hmi_run() antes de chamar esta
 * função (e é liberada por hmi_run() depois que ela retorna). Sobe a
 * thread RX e espelha no display os eventos da FSM do firmware.
 *
 * A cada iteração verifica: STOP (encerra a operação e volta à tela de
 * espera) e START (zera os contadores de objetos entregues, sem afetar
 * o ciclo em andamento). Em caso de falha na leitura do QR, exibe um
 * alerta (backlight + buzzer piscando indefinidamente) instruindo o
 * operador a liberar a esteira, e aguarda START para confirmar. A
 * confirmação dispara reset do STM32 e reenvio do SYS_RDY; como o
 * firmware só responde SYS_INIT espontaneamente quando os sensores da
 * esteira ficam livres, o Raspberry apenas espera essa resposta chegar
 * (sem timeout fixo, sem exigir novos START) enquanto o alerta continua
 * piscando -- STOP cancela a espera a qualquer momento. Os contadores
 * nunca são afetados por esse fluxo de falha/recuperação.
 *
 * @param fd          Descritor da porta serial.
 * @param qr_timeout  Timeout de leitura do QR em ms.
 * @param btn_start   Botão START já aberto (zera contagem em operação
 *                    normal; confirma o alerta de QR não identificado).
 * @param btn_stop    Botão STOP já aberto.
 * @param buzzer      Contexto do buzzer já aberto (pode ser NULL).
 * @return  1 se deve retornar à tela de espera (STOP pressionado no
 *          alerta ou na espera do sensor),  0 se g_interrupted foi
 *          setado durante a operação (inclusive durante um alerta/espera).
 */
static int hmi_operate(int fd, int qr_timeout, GpioButton *btn_start,
                        GpioButton *btn_stop, GpioBuzzer *buzzer)
{
    HmiCtx ctx;
    ctx.fd       = fd;
    ctx.running  = 1;
    ctx.rx_event = 0;
    pthread_mutex_init(&ctx.lock, NULL);

    pthread_t tid;
    pthread_create(&tid, NULL, hmi_rx_thread, &ctx);

    HmiOpState state = HMI_OP_IDLE;
    hmi_display_state(state);
    log_print(LOG_FSM, "HMI: operacao iniciada. Aguardando objetos...");

    int interrupted_during_op = 0;
    int stop_operation        = 0;

    while (ctx.running) {
        if (g_interrupted) {
            log_print(LOG_SYS,
                "HMI: interrupcao solicitada (Ctrl+C) -- encerrando com seguranca...");
            interrupted_during_op = 1;
            break;
        }

        if (button_was_pressed(btn_stop)) {
            log_print(LOG_SYS, "HMI: STOP pressionado -- encerrando operacao.");
            break;
        }

        if (button_was_pressed(btn_start)) {
            hmi_reset_counts();
            hmi_display_state(state);
        }

        int ev = hmi_consume_event(&ctx);

        switch (state) {

            case HMI_OP_IDLE:
                if (ev == OBJ_DETECTED) {
                    state = HMI_OP_OBJ_DETECTED;
                    log_print(LOG_FSM, "HMI: objeto detectado.");
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_OBJ_DETECTED:
                if (ev == CLSS_REQUEST) {
                    state = HMI_OP_CLASSIFYING;
                    log_print(LOG_FSM, "HMI: classificando QR...");
                    hmi_display_state(state);

                    CamResult res = camera_read_qr(qr_timeout);

                    if (res == CAM_ROUTE_A) {
                        state = HMI_OP_ROUTE_A;
                        hmi_display_state(state);
                        hmi_send(&ctx, ROUTE_A_SEND);
                    } else if (res == CAM_ROUTE_B) {
                        state = HMI_OP_ROUTE_B;
                        hmi_display_state(state);
                        hmi_send(&ctx, ROUTE_B_SEND);
                    } else {
                        log_print(LOG_ERR,
                            "HMI: objeto nao identificado -- alertando operador.");

                        int alert = hmi_wait_for_operator_ack(
                            "QR nao identificado",
                            "Libere a esteira!",
                            "START p/ reiniciar",
                            btn_start, btn_stop, buzzer);

                        if (alert == HMI_ALERT_INTERRUPTED) {
                            interrupted_during_op = 1;
                            stop_operation        = 1;
                        } else if (alert == HMI_ALERT_STOPPED) {
                            stop_operation = 1;
                        } else {
                            /* handshake_send_ready() e deinit_stm32() acessam
                             * a UART diretamente (fora do mecanismo de
                             * ctx.rx_event) e usam o parser estático de
                             * protocol.c, que não é reentrante. A thread RX
                             * precisa ser pausada durante esse acesso para
                             * evitar disputa pelos mesmos bytes e corrupção
                             * do estado do parser.
                             *
                             * O firmware só emite SYS_INIT espontaneamente
                             * quando os sensores da esteira ficam livres; não
                             * há outro meio do Raspberry Pi observar esse
                             * estado. Por isso a espera pela resposta não usa
                             * timeout fixo: handshake_send_ready() é chamado
                             * uma única vez e handshake_poll() é consultado
                             * em loop até a resposta chegar. */
                            ctx.running = 0;
                            pthread_join(tid, NULL);

                            hmi_display_msg("Reiniciando STM32", "aguarde...");
                            deinit_stm32(fd);
                            usleep(300000); /* folga para o STM32 reiniciar */

                            handshake_send_ready(fd);

                            lcd_clear();
                            lcd_print_line(0, "ESTEIRA - ALERTA");
                            lcd_print_line(1, "Aguardando sensor");
                            lcd_print_line(2, "ficar livre...");
                            lcd_print_line(3, "STOP p/ cancelar");

                            int hs_result   = 0; /* 0=aguardando 1=ok -1=erro */
                            int blink_on    = 0;
                            int elapsed_ms  = 0;
                            int aborted     = 0;

                            while (hs_result == 0) {
                                if (g_interrupted) {
                                    interrupted_during_op = 1;
                                    stop_operation        = 1;
                                    aborted = 1;
                                    break;
                                }
                                if (button_was_pressed(btn_stop)) {
                                    log_print(LOG_SYS,
                                        "HMI: STOP pressionado aguardando "
                                        "sensor livre -- encerrando operacao.");
                                    stop_operation = 1;
                                    aborted = 1;
                                    break;
                                }

                                hs_result = handshake_poll(fd);
                                elapsed_ms += 100; /* handshake_poll() bloqueia ~100ms */

                                if (hs_result == 0 &&
                                    elapsed_ms >= HMI_ALERT_BLINK_MS) {
                                    blink_on = !blink_on;
                                    lcd_set_backlight(blink_on);
                                    buzzer_set(buzzer, blink_on);
                                    elapsed_ms = 0;
                                }
                            }

                            lcd_set_backlight(1);
                            buzzer_set(buzzer, 0);

                            /* Religa a thread RX (necessária tanto para
                             * retomar a operação quanto para o join final). */
                            ctx.running  = 1;
                            ctx.rx_event = 0;
                            pthread_create(&tid, NULL, hmi_rx_thread, &ctx);

                            if (!aborted) {
                                if (hs_result == 1) {
                                    log_print(LOG_SYS,
                                        "HMI: sensor livre, STM32 reiniciado e "
                                        "handshake concluido -- retomando "
                                        "operacao.");
                                } else { /* hs_result == -1 (CMD_ERR) */
                                    log_print(LOG_ERR,
                                        "HMI: STM32 respondeu CMD_ERR no "
                                        "handshake apos alerta.");
                                    hmi_display_msg("ERRO: handshake",
                                                    "retornando ao menu");
                                    sleep(2);
                                    stop_operation = 1;
                                }
                            }
                        }

                        state = HMI_OP_IDLE;
                        if (!stop_operation) hmi_display_state(state);
                    }
                }
                break;

            case HMI_OP_CLASSIFYING:
                /* Inalcançável como entrada de switch: a transição para este
                 * estado e sua resolução (camera_read_qr, bloqueante) ocorrem
                 * por completo dentro do case HMI_OP_OBJ_DETECTED. Mantido
                 * aqui para que todos os valores do enum sejam cobertos
                 * explicitamente, evitando warnings do compilador. */
                break;

            case HMI_OP_ROUTE_A:
                if (ev == ROUTE_A_OK) {
                    s_count_a++;
                    state = HMI_OP_DELIVERED_A;
                    hmi_display_state(state);
                    sleep(2);
                    state = HMI_OP_IDLE;
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_ROUTE_B:
                if (ev == ROUTE_B_OK) {
                    s_count_b++;
                    state = HMI_OP_DELIVERED_B;
                    hmi_display_state(state);
                    sleep(2);
                    state = HMI_OP_IDLE;
                    hmi_display_state(state);
                }
                break;

            case HMI_OP_DELIVERED_A:
            case HMI_OP_DELIVERED_B:
                break;
        }

        if (stop_operation) break;

        if (ev == SW_RESET_MSG) {
            log_print(LOG_SYS, "HMI: STM32 reiniciando por SW Reset.");
            state = HMI_OP_IDLE;
            hmi_display_state(state);
        }

        usleep(10000);
    }

    ctx.running = 0;
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&ctx.lock);
    log_print(LOG_FSM, "HMI: operacao encerrada.");

    return interrupted_during_op ? 0 : 1;
}

/**
 * @brief Executa o loop do modo HMI (display + botões).
 *
 * Ao pressionar START na tela de espera, a câmera é inicializada
 * primeiro; só depois dela estar pronta o handshake com o STM32 é
 * disparado. Isso garante que a operação autônoma só comece quando
 * todo o hardware necessário -- câmera incluída -- já estiver de pé.
 *
 * @param fd          Descritor da porta serial.
 * @param cam_index   Índice da câmera.
 * @param qr_timeout  Timeout para leitura do QR em ms.
 */
void hmi_run(int fd, int cam_index, int qr_timeout)
{
    if (lcd_init(HMI_I2C_DEVICE, HMI_LCD_I2C_ADDR) != 0) {
        log_print(LOG_ERR, "HMI: falha ao inicializar o display LCD.");
        return;
    }

    GpioButton *btn_start = button_open(HMI_GPIO_CHIP, HMI_BTN_START_LINE,
                                         HMI_BTN_DEBOUNCE_MS);
    GpioButton *btn_stop  = button_open(HMI_GPIO_CHIP, HMI_BTN_STOP_LINE,
                                         HMI_BTN_DEBOUNCE_MS);

    if (!btn_start || !btn_stop) {
        log_print(LOG_ERR, "HMI: falha ao abrir os botoes GPIO.");
        hmi_display_msg("ERRO: botoes GPIO", "verifique fiacao");
        button_close(btn_start);
        button_close(btn_stop);
        lcd_close();
        return;
    }

    /* Buzzer é tratado como opcional: se a linha não abrir (ex.: hardware
     * ainda não cabeado), o modo HMI segue funcionando normalmente, só
     * que o alerta de objeto não identificado fica sem o apito. */
    GpioBuzzer *buzzer = buzzer_open(HMI_GPIO_CHIP, HMI_BUZZER_LINE);
    if (!buzzer) {
        log_print(LOG_ERR,
            "HMI: buzzer indisponivel (GPIO23) -- alerta seguira so com backlight.");
    }

    log_print(LOG_SYS, "Modo HMI iniciado. Aguardando START...");
    hmi_display_waiting();

    while (!g_interrupted) {
        if (button_was_pressed(btn_start)) {
            log_print(LOG_SYS,
                "HMI: START pressionado -- inicializando camera...");
            hmi_display_msg("Iniciando camera", "aguarde...");

            if (camera_init(cam_index) != 0) {
                log_print(LOG_ERR, "HMI: falha ao inicializar camera.");
                hmi_display_msg("ERRO: camera", "nao inicializou");
                sleep(2);
                hmi_display_waiting();
                continue;
            }

            log_print(LOG_SYS,
                "HMI: camera pronta -- iniciando handshake...");
            hmi_display_msg("Conectando ao", "STM32...");

            if (do_handshake(fd) != 0) {
                log_print(LOG_ERR, "HMI: handshake falhou.");
                hmi_display_msg("ERRO: handshake", "tente novamente");
                camera_release();
                sleep(2);
                hmi_display_waiting();
                continue;
            }

            int return_to_waiting = hmi_operate(fd, qr_timeout,
                                                 btn_start, btn_stop, buzzer);

            camera_release();

            if (!return_to_waiting)
                break; /* g_interrupted durante a operação — sai do modo HMI */

            /* STOP pressionado: reset do STM32 antes de voltar à tela de
             * espera, para devolvê-lo ao estado inicial. */
            deinit_stm32(fd);
            hmi_display_msg("Parando...", "");
            sleep(1);
            hmi_display_waiting();
            continue;
        }

        if (g_interrupted) break;
        usleep(20000);
    }

    if (g_interrupted) {
        log_print(LOG_SYS, "HMI: interrompido (Ctrl+C) -- encerrando.");
        lcd_clear();
        lcd_print_line(0, "PROGRAMA ENCERRADO");
        lcd_print_line(1, "Reinicie pelo");
        lcd_print_line(2, "terminal");
        lcd_print_line(3, "");
    }

    button_close(btn_start);
    button_close(btn_stop);
    buzzer_close(buzzer);
    lcd_close();
    log_print(LOG_SYS, "Modo HMI encerrado.");
}
