#ifndef CAMERA_H
#ifdef __cplusplus
extern "C" {
#endif
#define CAMERA_H

#include <stdint.h>

/* =========================================================
 *  Módulo de câmera e leitura de QR code
 *
 *  Usa a API legada C do OpenCV (cv.h / highgui.h) para
 *  captura de frames, e libzbar para decodificação do QR.
 *
 *  Dependências:
 *    - OpenCV  (pkg: libopencv-dev)
 *    - ZBar    (pkg: libzbar-dev)
 *
 *  O módulo opera de forma síncrona: quando chamado, captura
 *  frames continuamente até ler um QR válido com destino
 *  ROUTE_A (0xDA) ou ROUTE_B (0xDB), ou até o timeout.
 * ========================================================= */

/* Resultado da leitura */
typedef enum {
    CAM_ROUTE_A  =  1,   /* QR lido: destino ROTA A (0xDA) */
    CAM_ROUTE_B  =  2,   /* QR lido: destino ROTA B (0xDB) */
    CAM_TIMEOUT  = -1,   /* Nenhum QR válido dentro do timeout */
    CAM_ERROR    = -2    /* Erro de hardware ou inicialização */
} CamResult;

/**
 * Inicializa a câmera (abre o dispositivo de captura).
 * @param device_index  Índice da câmera (0 para /dev/video0)
 * @return  0 em sucesso, -1 em erro.
 */
int camera_init(int device_index);

/**
 * Libera os recursos da câmera.
 */
void camera_release(void);

/**
 * Captura frames e tenta ler um QR code com destino de rota válido.
 * Bloqueia até encontrar um QR válido ou atingir o timeout.
 *
 * @param timeout_ms  Timeout em milissegundos (0 = sem timeout)
 * @return  CAM_ROUTE_A, CAM_ROUTE_B, CAM_TIMEOUT ou CAM_ERROR
 */
CamResult camera_read_qr(int timeout_ms);

#endif /* CAMERA_H */

#ifdef __cplusplus
}
#endif
