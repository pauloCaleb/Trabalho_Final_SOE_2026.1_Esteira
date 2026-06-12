#define _POSIX_C_SOURCE 200809L

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <zbar.h>

#include "camera.h"
#include "protocol.h"
#include "log.h"

#include <string.h>
#include <time.h>
#include <stdint.h>

using namespace zbar;

static cv::VideoCapture      s_capture;
static zbar_image_scanner_t *s_scanner = NULL;

static uint8_t parse_qr_text(const char *text)
{
    if (!text || text[0] == '\0') return 0;

    const char *dest = strstr(text, "Destino:");
    if (!dest) dest  = strstr(text, "destino:");
    if (dest) {
        dest += 8;
        while (*dest == ' ' || *dest == '\t') dest++;
        unsigned int val = 0;
        if (sscanf(dest, "0x%X", &val) == 1 || sscanf(dest, "0X%X", &val) == 1) {
            if ((val >> 8) == START_FRAME) {
                uint8_t route = val & 0xFF;
                if (route == ROUTE_A_SEND || route == ROUTE_B_SEND) return route;
            }
            if (val == ROUTE_A_SEND || val == ROUTE_B_SEND) return (uint8_t)val;
        }
    }

    const char *p = text;
    while (*p) {
        if ((p[0] == '0') && (p[1] == 'x' || p[1] == 'X')) {
            unsigned int val = 0;
            if (sscanf(p, "0x%X", &val) == 1 || sscanf(p, "0X%X", &val) == 1) {
                if ((val >> 8) == START_FRAME) {
                    uint8_t route = val & 0xFF;
                    if (route == ROUTE_A_SEND || route == ROUTE_B_SEND) return route;
                }
                if (val == ROUTE_A_SEND || val == ROUTE_B_SEND) return (uint8_t)val;
            }
        }
        p++;
    }
    return 0;
}

extern "C" int camera_init(int device_index)
{
    s_capture.open(device_index);
    if (!s_capture.isOpened()) {
        log_print(LOG_ERR, "Nao foi possivel abrir camera (indice %d)", device_index);
        return -1;
    }
    s_capture.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    s_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    s_scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(s_scanner, ZBAR_NONE,   ZBAR_CFG_ENABLE, 0);
    zbar_image_scanner_set_config(s_scanner, ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);

    log_print(LOG_CAM, "Camera inicializada (indice %d, 640x480)", device_index);
    return 0;
}

extern "C" void camera_release(void)
{
    if (s_capture.isOpened()) s_capture.release();
    if (s_scanner) {
        zbar_image_scanner_destroy(s_scanner);
        s_scanner = NULL;
    }
    log_print(LOG_CAM, "Camera liberada.");
}

extern "C" CamResult camera_read_qr(int timeout_ms)
{
    if (!s_capture.isOpened() || !s_scanner) {
        log_print(LOG_ERR, "Camera nao inicializada.");
        return CAM_ERROR;
    }

    struct timespec t_start, t_now;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    int attempt     = 0;
    int first_frame = 1;

    log_print(LOG_CAM, "Iniciando captura (timeout: %d ms)...",
              timeout_ms > 0 ? timeout_ms : 0);

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        long elapsed_ms =
            (t_now.tv_sec  - t_start.tv_sec)  * 1000 +
            (t_now.tv_nsec - t_start.tv_nsec) / 1000000L;

        if (timeout_ms > 0 && elapsed_ms >= timeout_ms) {
            log_print(LOG_CAM, "Timeout (%d ms) -- %d tentativas.", timeout_ms, attempt);
            return CAM_TIMEOUT;
        }

        cv::Mat frame;
        s_capture >> frame;
        if (frame.empty()) {
            log_print(LOG_ERR, "Falha ao capturar frame.");
            return CAM_ERROR;
        }

        if (first_frame) {
            first_frame = 0;
            log_print(LOG_CAM, "Primeiro frame: %dx%d -- escaneando QR...",
                      frame.cols, frame.rows);
        }

        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        zbar_image_t *zimg = zbar_image_create();
        zbar_image_set_format(zimg, zbar_fourcc('Y','8','0','0'));
        zbar_image_set_size(zimg, (unsigned)gray.cols, (unsigned)gray.rows);
        zbar_image_set_data(zimg, gray.data,
            (unsigned long)(gray.cols * gray.rows), NULL);

        int n = zbar_scan_image(s_scanner, zimg);
        if (n > 0) {
            const zbar_symbol_t *sym = zbar_image_first_symbol(zimg);
            if (sym) {
                const char *data = zbar_symbol_get_data(sym);
                log_print(LOG_CAM, "QR lido (tent. %d): '%s'", attempt + 1, data);
                uint8_t route = parse_qr_text(data);
                if (route == ROUTE_A_SEND) {
                    log_print(LOG_CAM, "Rota: A (0x%02X)", route);
                    zbar_image_destroy(zimg);
                    return CAM_ROUTE_A;
                }
                if (route == ROUTE_B_SEND) {
                    log_print(LOG_CAM, "Rota: B (0x%02X)", route);
                    zbar_image_destroy(zimg);
                    return CAM_ROUTE_B;
                }
                log_print(LOG_CAM, "QR sem rota valida.");
            }
        }

        zbar_image_destroy(zimg);
        attempt++;
        if (attempt % 10 == 0)
            log_print(LOG_CAM, "Aguardando QR... (%d frames, %ld ms)", attempt, elapsed_ms);
    }
}
