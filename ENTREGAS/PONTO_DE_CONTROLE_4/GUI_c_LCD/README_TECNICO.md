# Esteira Separadora — Descrição Técnica dos Algoritmos

---

## Visão Geral da Arquitetura

```
┌─────────────────────────────────────────────────────┐
│                  Raspberry Pi 3B                    │
│                                                     │
│  main.c ──► serial_open()  ──► /dev/ttyS0           │
│      │                               │              │
│      ▼                            UART 8N1          │
│  handshake()                      115200            │
│      │                               │              │
│      ▼                        ┌──────┴──────┐       │
│  select_mode()                │  STM32G070  │       │
│      │                        └─────────────┘       │
│    ┌─┴─────────┐                                    │
│    │           │                                    │
│  fsm_run()  debug_run()                             │
│    │           │                                    │
│  Thread RX  Thread RX   ◄── protocol_parse_byte()   │
│  (pthread)  (pthread)                               │
│    │                                                │
│  camera_read_qr()  ◄── OpenCV (C++) + ZBar          │
└─────────────────────────────────────────────────────┘
```

---

## Protocolo de Comunicação (proprietário, binário, UART 8N1)

### Características

| Parâmetro | Valor |
|---|---|
| Camada física | UART 115200 bps, 8 bits, sem paridade, 1 stop bit |
| Sincronização | Byte de status fixo como delimitador de início |
| Tamanho de frame | 1, 2 ou 3 bytes (fixo por comando) |
| CRC / checksum | **Nenhum** |
| Campo de comprimento | **Nenhum** |
| Endereçamento | Ponto a ponto (sem múltiplos nós) |
| ACK implícito | Echo do comando pelo STM32 |

### Estrutura dos Frames

**TX — Raspberry Pi → STM32**

```
Frame padrão (2 bytes):
┌────────┬────────┐
│  0xAA  │  CMD   │
└────────┴────────┘

Frame com dado (3 bytes) — somente SET_STPR_TGT_STPS:
┌────────┬────────┬────────┐
│  0xAA  │  0xE5  │  DATA  │
└────────┴────────┴────────┘
  DATA = número de passos (1–255)
```

**RX — STM32 → Raspberry Pi**

```
Erro (1 byte):
┌────────┐
│  0x91  │   CMD_ERR
└────────┘

Evento / confirmação (2 bytes):
┌────────┬────────┐
│  0x90  │  CMD   │   CMD_OK + payload
└────────┴────────┘

Telemetria de sensores (3 bytes):
┌────────┬────────┬────────┐
│  0x90  │  0x55  │ FLAGS  │
└────────┴────────┴────────┘
  FLAGS: bits 0-3 = sensores S1-S4 (1=objeto presente, 0=livre)
```

### Tabela de Comandos

| Direção | Byte | Nome | Significado |
|---|---|---|---|
| TX | `0x10` | `SYS_RDY` | Handshake — RPi pronto |
| RX | `0x01` | `SYS_INIT` | STM32 inicializado |
| RX | `0xA0` | `OBJ_DETECTED` | Sensor detectou objeto |
| RX | `0xC0` | `CLSS_REQUEST` | Objeto sob câmera, classifique |
| TX | `0xDA` | `ROUTE_A_SEND` | Enviar para Rota A |
| TX | `0xDB` | `ROUTE_B_SEND` | Enviar para Rota B |
| RX | `0xFA` | `ROUTE_A_FWD` | STM32 encaminhou para A |
| RX | `0xFB` | `ROUTE_B_FWD` | STM32 encaminhou para B |
| RX | `0xBA` | `ROUTE_A_OK` | Entrega em A confirmada |
| RX | `0xBB` | `ROUTE_B_OK` | Entrega em B confirmada |
| TX | `0xE1` | `LIGHT_EN` | Flash ligado |
| TX | `0xD1` | `LIGHT_DIS` | Flash desligado |
| TX | `0xE2` | `GATE_OPEN` | Cancela abrir |
| TX | `0xD2` | `GATE_CLOSE` | Cancela fechar |
| TX | `0xE3` | `STPR_EN` | Motor passo engajar |
| TX | `0xD3` | `STPR_DIS` | Motor passo livre |
| TX | `0xE4` | `STPR_FORWARD` | Motor passo — frente |
| TX | `0xD4` | `STPR_BACKWARD` | Motor passo — trás |
| TX | `0xE5` + N | `SET_STPR_TGT_STPS` | Motor passo — N passos |
| TX | `0xDD` | `DEBUG_TOGGLE` | Alterna modo DEBUG/FSM |
| TX/RX | `0x33` | `SW_RESET_MSG` | Reset por software |
| RX | `0x55` + FLAGS | `SENS_STATUS_MSG` | Telemetria sensores |

---

## Módulos

### `serial.c` — Camada Física

Abre `/dev/ttyS0` com `O_RDWR | O_NOCTTY | O_NONBLOCK` e configura
via `termios` para modo raw 8N1. A leitura com timeout usa `select()`
sobre o file descriptor — não bloqueia threads concorrentes.

Funções principais:

| Função | Comportamento |
|---|---|
| `serial_open()` | Abre, configura termios, faz flush do buffer RX |
| `serial_write()` | Escrita completa em loop (`EINTR`-safe) |
| `serial_read_byte_timeout()` | Lê 1 byte com timeout via `select()` |
| `serial_flush()` | `tcflush(TCIFLUSH)` — descarta bytes pendentes no RX |

---

### `protocol.c` — Parser de Frames

Implementa uma **máquina de estados de 3 fases** alimentada byte a byte.
O parser é stateful (variáveis estáticas internas), portanto não é
reentrante — deve ser chamado de uma única thread.

```
      byte 0x90 ou 0x91
WAIT_STATUS ──────────────────► WAIT_CMD
    │                               │
    │ outro byte → descartado       │ byte == 0x55
    │ (log_print ERR)               ▼
    │                           WAIT_DATA ──► frame completo (3 bytes)
    │                               │
    │                          outro byte
    │                               ▼
    └───────────────────────── frame completo (2 bytes)

    0x91 em WAIT_STATUS → frame de 1 byte (CMD_ERR) entregue imediatamente
```

Bytes que chegam no estado `WAIT_STATUS` e não são `0x90` nem `0x91`
são descartados com log de erro — indica ruído na linha ou
desalinhamento de frame.

---

### `fsm.c` — Máquina de Estados (Modo Autônomo)

#### Diagrama de Estados

```
         ┌──────────────────────────────────────────────────┐
         │                                                  │
         ▼                                                  │
      [IDLE] ──── OBJ_DETECTED ────► [OBJ_DETECTED]         │
         ▲                                │                 │
         │                         CLSS_REQUEST             │
         │                                │                 │
         │                                ▼                 │
         │                         [CLASSIFYING]            │
         │                        /            \            │
         │               CAM_ROUTE_A      CAM_ROUTE_B       │
         │                    │                 │           │
         │              ROUTE_A_SEND      ROUTE_B_SEND      │
         │                    │                 │           │
         │                    ▼                 ▼           │
         │               [ROUTE_A]         [ROUTE_B]        │
         │                    │                 │           │
         │              ROUTE_A_OK        ROUTE_B_OK        │
         │                    │                 │           │
         │             [DELIVERED_A]    [DELIVERED_B]       │
         │                    │                 │           │
         └────────────────────┴─────────────────┘           │
                         (sleep 2s)                         │
                                                            │
         Qualquer estado: SW_RESET_MSG ─────────────────────┘
```

#### Arquitetura de Threads

A thread RX é criada **antes** da inicialização da câmera. Isso é
intencional — o OpenCV leva ~2 s para abrir o dispositivo de vídeo,
e durante esse tempo o STM32 pode enviar eventos (ex: `OBJ_DETECTED`,
`CLSS_REQUEST`) que seriam perdidos se a thread ainda não estivesse
ativa. Com essa ordem, todos os eventos são capturados em
`ctx.rx_event` e processados pelo loop principal após a câmera estar
pronta.

```
main thread (fsm_run)          rx_thread
      │                             │
      │  pthread_create() ─────────►│  (sobe ANTES da camera)
      │                             │ serial_read_byte_timeout(100 ms)
      │  camera_init()              │ protocol_parse_byte()
      │  (~2s abertura OpenCV)      │ mutex_lock → ctx.rx_event = cmd
      │                             │ mutex_unlock
      │                             │
      │ fsm_consume_event()         │
      │  mutex_lock                 │
      │  ev = ctx.rx_event          │
      │  ctx.rx_event = 0           │
      │  mutex_unlock               │
      │                             │
      │ switch(state) + usleep(10ms)│
      │                             │
      │  ctx.running = 0 ──────────►│ (sai do loop)
      │  pthread_join() ◄───────────┘
```

A leitura da câmera (`camera_read_qr`) ocorre **na main thread**, de
forma síncrona, enquanto a thread RX continua recebendo bytes em paralelo.
Eventos que chegam durante a classificação são enfileirados em
`ctx.rx_event` e processados após o retorno da câmera.

---

### `camera.cpp` — Captura e Leitura de QR

Implementado em **C++** usando a API moderna do OpenCV (`cv::VideoCapture`,
`cv::Mat`) por exigência do OpenCV 4.x, que removeu suporte à API legada C.
As funções exportadas (`camera_init`, `camera_release`, `camera_read_qr`)
são declaradas com `extern "C"` para compatibilidade com os módulos C do
projeto. O ZBar recebe frames em escala de cinza (`Y800`).

#### Fluxo de captura

```
s_capture >> frame  (cv::VideoCapture)
    │
    ▼
cv::cvtColor(BGR → GRAY)
    │
    ▼
zbar_scan_image()
    │
    ├── n == 0 → nenhum QR → próximo frame
    │
    └── n > 0  → zbar_symbol_get_data()
                     │
                     ▼
               parse_qr_text()
                     │
                     ├── campo "Destino: 0xAADA" → extrai byte baixo
                     ├── campo "Destino: 0xDA"   → usa diretamente
                     └── varredura de tokens 0xXX no texto inteiro
```

#### Formato esperado do QR

```
Nome da Peca: <texto livre>
Destino: 0xAADA
```

`0xDA` → `ROUTE_A_SEND` (Rota A)
`0xDB` → `ROUTE_B_SEND` (Rota B)

O parser tenta duas estratégias em sequência: campo `Destino:` primeiro,
depois varredura linear do texto. Isso garante compatibilidade com QRs
que contenham apenas o token hexadecimal sem label.

---

### `debug.c` — Modo Interativo

Usa `select()` sobre `STDIN_FILENO` com timeout de 200 ms para ler
input do teclado sem bloquear a thread RX. A thread RX exibe
telemetria (`SENS_STATUS_MSG`) em tempo real enquanto o operador
digita comandos.

---

### `hmi.c` — Modo Display + Botões

Terceiro modo de operação, com mini-FSM própria (`HmiOpState`,
independente da `PcFsmState` de `fsm.c`) espelhando os mesmos
eventos de protocolo do Modo FSM, porém com saída em LCD em vez
de log no terminal.

```
[HMI_WAITING_START]
        │  button_was_pressed(btn_start)
        ▼
   do_handshake(fd)
        │  sucesso
        ▼
[hmi_operate()]  ── mesma lógica de fsm_run(), porém:
        │            • checa button_was_pressed(btn_stop) a cada
        │              iteração do loop (não só Ctrl+C)
        │            • hmi_display_state() em vez de log_fsm_state()
        │
        ├─ STOP pressionado ──► deinit_stm32(fd) ──► [HMI_WAITING_START]
        └─ g_interrupted     ──► encerra o modo HMI por completo
```

Dependências de hardware, isoladas em módulos próprios:

- `lcd2004_i2c.c` — driver HD44780 4-bit via backpack I2C PCF8574
  (`/dev/i2c-X` + `ioctl(I2C_SLAVE)`). Mapeamento de pinos do
  backpack: `P0=RS P1=RW P2=E P3=Backlight P4-P7=D4-D7`. O
  endereçamento de DDRAM das 4 linhas do 2004A não é contíguo
  (linhas 0/2 começam em `0x00`/`0x14`, linhas 1/3 em `0x40`/`0x54`)
  -- peculiaridade conhecida do HD44780 em displays 20x4.
  `lcd_print_line()` sobrescreve a linha inteira com padding de
  espaços para evitar flicker (sem `lcd_clear()` a cada
  atualização). O controlador não entende UTF-8: strings devem
  ser ASCII puro, sem acentuação.

- `gpio_button.c` — leitura por polling via libgpiod (API v1:
  `gpiod_chip_open_by_name` / `gpiod_line_request_input_flags` /
  `gpiod_line_get_value`), sem bias interno (pull-up é externo ao
  circuito do botão, nível 0 = pressionado). Debounce por
  software configurável (`HMI_BTN_DEBOUNCE_MS` em `hmi.h`): uma
  borda de descida só é reportada como press válido se o nível
  baixo persistir pelo tempo configurado desde a primeira
  amostra; cada press é entregue uma única vez por pressão
  (consumido até o botão ser solto e pressionado novamente).

---

### `log.c` — Sistema de Log

Todas as mensagens imprimem `HH:MM:SS.mmm` via `clock_gettime(CLOCK_REALTIME)`
seguido de prefixo ANSI colorido e texto formatado (`vprintf`).
O `fflush(stdout)` ao final garante saída imediata mesmo com redirecionamento.

A função `log_fsm_state()` imprime adicionalmente um **pipeline visual**
da FSM em linha, indicando o estado ativo com destaque bold:

```
          [IDLE]  >  OBJ  >  CLASS  >  ROTA A  >  ROTA B
```

---

## Sequência de Inicialização (Handshake)

```
RPi                            STM32
 │                               │
 │── [0xAA][0x10]  SYS_RDY ────► │
 │                               │
 │◄─ [0x90][0x01]  SYS_INIT ──── │  (dentro de 10 s)
 │                               │
 │   Handshake OK                │
 │                               │
 │── [0xAA][0xDA ou 0xDB] ──────►│  (após classificação)
```

Se `SYS_INIT` não chegar em 10 000 ms o programa encerra com erro.
Durante a espera, o log emite heartbeats a cada ~2 s para confirmar
que o processo está ativo.

---

## Dependências e Versões

| Biblioteca | Pacote apt | Uso |
|---|---|---|
| OpenCV 4.x | `libopencv-dev` | Captura de câmera (API C++ moderna) |
| ZBar | `libzbar-dev` | Decodificação de QR / barcode |
| libgpiod | `libgpiod-dev` | Leitura por polling dos botões START/STOP (API v1) |
| pthreads | glibc (nativo) | Thread RX em background |
| termios | glibc (nativo) | Configuração da UART |
| ioctl I2C (`linux/i2c-dev.h`) | glibc/kernel headers (nativo) | Comunicação com o backpack PCF8574 do LCD |