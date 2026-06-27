#ifndef CAMERA_H
#define CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Módulo de câmera e leitura de QR code.
 *
 * Implementado em camera.cpp com a API C++ do OpenCV 4.x
 * (cv::VideoCapture, cv::Mat) + libzbar para decodificação.
 * As funções exportadas são declaradas com extern "C" para
 * interoperar com os módulos C do projeto.
 *
 * Dependências:
 *   sudo apt install libopencv-dev libzbar-dev */

/** Resultados possíveis de camera_read_qr() */
typedef enum {
    CAM_ROUTE_A  =  1,   /* QR com destino Rota A (0xDA) */
    CAM_ROUTE_B  =  2,   /* QR com destino Rota B (0xDB) */
    CAM_TIMEOUT  = -1,   /* Timeout sem QR válido         */
    CAM_ERROR    = -2    /* Erro de hardware ou init       */
} CamResult;

/**
 * @brief Abre o dispositivo de câmera e cria o scanner ZBar.
 *
 * Configura a câmera para 640x480 e habilita apenas QR code no ZBar.
 *
 * @param device_index  Índice da câmera (0 = /dev/video0).
 * @return              0 em sucesso, -1 se o dispositivo não abriu.
 */
int camera_init(int device_index);

/**
 * @brief Libera o dispositivo de câmera e o scanner ZBar.
 */
void camera_release(void);

/**
 * @brief Captura frames em loop tentando ler um QR code com rota válida.
 *
 * Converte cada frame para escala de cinza e passa ao ZBar.
 * Ao detectar um símbolo, extrai o byte de destino do texto
 * decodificado (campo "Destino:" ou token hex avulso).
 *
 * @param timeout_ms  Tempo máximo em ms (0 = aguarda indefinidamente).
 * @return            CAM_ROUTE_A, CAM_ROUTE_B, CAM_TIMEOUT ou CAM_ERROR.
 */
CamResult camera_read_qr(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_H */
