# Tabela de Mensagens — Firmware STM32G070 v3
> **Protocolo de quadro:**
> - **RX** (Raspberry Pi → STM32): `[0xAA][CMD]` ou `[0xAA][CMD][DATA]`
> - **TX** (STM32 → Raspberry Pi): `[0x90][PAYLOAD]` (sucesso), `[0x90][CMD][DATA]` (telemetria), ou `[0x91]` (erro)

---

## Tabela 1 — Dicionário Completo de Mensagens

| Símbolo | Byte | Direção | Tipo | Descrição |
|---|---|---|---|---|
| `START_FRAME_MSG` | `0xAA` | RX | Estrutura | Byte de início de quadro. Todo frame recebido deve começar com este byte; bytes fora de quadro são ignorados silenciosamente. |
| `CMD_OK_MSG` | `0x90` | TX | Estrutura | Byte de status positivo. Enviado como primeiro byte de qualquer resposta bem-sucedida, sempre seguido de um payload. |
| `CMD_ERR_MSG` | `0x91` | TX | Estrutura | Byte de status negativo. Enviado sozinho (1 byte) quando o comando recebido não é reconhecido. |
| `SYS_RDY_MSG` | `0x10` | RX | Handshake | Mensagem enviada pelo Raspberry Pi para iniciar o handshake. Só é aceita após todos os quatro sensores estarem operando na frequência esperada (200 Hz). |
| `SYS_INIT_MSG` | `0x01` | TX | Handshake | Payload de resposta ao handshake. Indica que o STM32 está inicializado. Enviado no quadro `[0x90][0x01]`, sempre seguido de `[0x90][0x11]` informando o modo inicial. |
| `MODE_FSM_MSG` | `0x11` | TX | Modo | Payload que informa que o modo ativo é `OP_MODE_FSM`. Enviado após handshake e após cada transição de modo. |
| `MODE_DEBUG_MSG` | `0x22` | TX | Modo | Payload que informa que o modo ativo é `OP_MODE_DEBUG`. Enviado após cada transição de modo. |
| `SW_RESET_MSG` | `0x33` | RX | Controle | Comando de reset por software. Processado com prioridade máxima (antes do toggle de modo e da FSM). O STM32 envia `[0x90][0x33]` e executa `NVIC_SystemReset()` após ~50 ms. |
| `ROUTE_A_RECEIVE_MSG` | `0xDA` | RX | FSM | Comando enviado pelo Raspberry Pi ordenando o encaminhamento do objeto para a Rota A. Válido apenas no estado `STATE_WAIT_CLASSIFICATION`. |
| `ROUTE_B_RECEIVE_MSG` | `0xDB` | RX | FSM | Comando enviado pelo Raspberry Pi ordenando o encaminhamento do objeto para a Rota B. Válido apenas no estado `STATE_WAIT_CLASSIFICATION`. |
| `OBJ_DETECTED_MSG` | `0xA0` | TX | FSM | Notificação espontânea de que o Sensor 1 detectou um objeto na entrada da esteira (transição `IDLE → OBJECT_DETECTED`). |
| `CLSS_REQUEST_MSG` | `0xC0` | TX | FSM | Solicitação de classificação enviada ao Raspberry Pi quando o objeto alcança o Sensor 2 (transição `OBJECT_DETECTED → WAIT_CLASSIFICATION`). O STM32 aguarda `0xDA` ou `0xDB` em resposta. |
| `ROUTE_A_FWRDNG_MSG` | `0xFA` | TX | FSM | Confirmação de que o objeto está sendo encaminhado para a Rota A. Enviado ao receber `0xDA`. |
| `ROUTE_B_FWRDNG_MSG` | `0xFB` | TX | FSM | Confirmação de que o objeto está sendo encaminhado para a Rota B. Enviado ao receber `0xDB`. |
| `ROUTE_A_SCCSS_DLVRY_MSG` | `0xBA` | TX | FSM | Notificação de entrega bem-sucedida na Rota A. Disparado quando o Sensor 3 detecta a passagem do objeto. FSM retorna a `STATE_IDLE`. |
| `ROUTE_B_SCCSS_DLVRY_MSG` | `0xBB` | TX | FSM | Notificação de entrega bem-sucedida na Rota B. Disparado quando o Sensor 4 detecta a passagem do objeto. FSM retorna a `STATE_IDLE`. |
| `DEBUG_MODE_TOGGLE_MSG` | `0xDD` | RX | Modo | Alterna entre `OP_MODE_FSM` e `OP_MODE_DEBUG`. Processado com prioridade máxima (após `SW_RESET_MSG`, antes da FSM) em qualquer modo de operação. |
| `SENS_STATUS_MSG` | `0x55` | TX | Telemetria | Mensagem espontânea de telemetria dos sensores. Enviada apenas em `OP_MODE_DEBUG`, somente quando o STATUS_BYTE muda. Frame de 3 bytes: `[0x90][0x55][STATUS_BYTE]`. Bits 0–3 do STATUS_BYTE indicam o estado de SENS1–SENS4 respectivamente (1 = objeto detectado, 0 = feixe livre). Bits 4–7 reservados (0). |
| `LIGHT_EN_MSG` | `0xE1` | RX | Assíncrono (Debug) | Liga o flash/luminária (`FLASH_PIN` → HIGH). Válido apenas em `OP_MODE_DEBUG`. |
| `LIGHT_DISABLE_MSG` | `0xD1` | RX | Assíncrono (Debug) | Desliga o flash/luminária (`FLASH_PIN` → LOW). Válido apenas em `OP_MODE_DEBUG`. |
| `GATE_OPEN_MSG` | `0xE2` | RX | Assíncrono (Debug) | Abre a cancela movendo o servo para `openAngle` (45°). Válido apenas em `OP_MODE_DEBUG`. |
| `GATE_CLOSE_MSG` | `0xD2` | RX | Assíncrono (Debug) | Fecha a cancela movendo o servo para `closeAngle` (0°). Válido apenas em `OP_MODE_DEBUG`. |
| `STPR_EN_MSG` | `0xE3` | RX | Assíncrono (Debug) | Ativa o motor de passo no modo *follow steps* (eixo engajado, TIM6 habilitado). Válido apenas em `OP_MODE_DEBUG`. |
| `STPR_DISABLE_MSG` | `0xD3` | RX | Assíncrono (Debug) | Desativa o motor de passo com eixo livre (driver A4988 em SLEEP). Válido apenas em `OP_MODE_DEBUG`. |
| `SET_STPR_FORWARD_MSG` | `0xE4` | RX | Assíncrono (Debug) | Define o sentido de rotação do motor de passo como **frente** (`stepperDirInst = 0`). Válido apenas em `OP_MODE_DEBUG`. |
| `SET_STPR_BACKWARD_MSG` | `0xD4` | RX | Assíncrono (Debug) | Define o sentido de rotação do motor de passo como **trás** (`stepperDirInst = 1`). Válido apenas em `OP_MODE_DEBUG`. |
| `SET_STPR_TGT_STPS_MSG` | `0xE5` | RX | Assíncrono (Debug) | Inicia um quadro de 3 bytes. O byte `DATA` subsequente define a quantidade de passos alvo (`targetStepps`). É o único frame RX de 3 bytes (`[0xAA][0xE5][DATA]`). Válido apenas em `OP_MODE_DEBUG`. |

---

## Tabela 2 — Fluxo de Mensagens por Evento

> **Legenda de quadros completos:**
> - Quadro TX de sucesso (2 bytes): `[0x90][PAYLOAD]`
> - Quadro TX de telemetria (3 bytes): `[0x90][0x55][STATUS_BYTE]`
> - Quadro TX de erro (1 byte): `[0x91]`
> - Quadro RX padrão (2 bytes): `[0xAA][CMD]`
> - Quadro RX com dado (3 bytes): `[0xAA][0xE5][DATA]`

---

### Modo Normal — `OP_MODE_FSM`

#### Sequência de Inicialização (Handshake)

| # | Evento | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | Sensores prontos + RPi inicia handshake | RPi → STM32 | `[START][SYS_RDY]` | `AA 10` | Raspberry Pi sinaliza que está pronto. STM32 só aceita após todos os sensores medirem 200 Hz. |
| 2 | STM32 confirma inicialização | STM32 → RPi | `[CMD_OK][SYS_INIT]` | `90 01` | STM32 confirma sistema inicializado. |
| 3 | STM32 informa modo inicial | STM32 → RPi | `[CMD_OK][MODE_FSM]` | `90 11` | STM32 informa que inicia em `OP_MODE_FSM`. Loop de boot encerra. FSM inicia em `STATE_IDLE`. |

---

#### Ciclo Completo — Objeto encaminhado para Rota A

| # | Evento / Estado FSM | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | `IDLE` → Sensor 1 detecta objeto | STM32 → RPi | `[CMD_OK][OBJ_DETECTED]` | `90 A0` | STM32 notifica detecção. FSM avança para `STATE_OBJECT_DETECTED`. |
| 2 | `OBJECT_DETECTED` → Sensor 2 detecta objeto | STM32 → RPi | `[CMD_OK][CLSS_REQUEST]` | `90 C0` | STM32 para o motor e solicita classificação. FSM avança para `STATE_WAIT_CLASSIFICATION`. |
| 3 | RPi define destino: Rota A | RPi → STM32 | `[START][ROUTE_A]` | `AA DA` | Raspberry Pi envia o resultado da classificação por visão computacional. |
| 4 | STM32 confirma e encaminha para Rota A | STM32 → RPi | `[CMD_OK][ROUTE_A_FWD]` | `90 FA` | STM32 confirma, aciona servo (`closeAngle`), religa motor. FSM avança para `STATE_ROUTE_A`. |
| 5 | `ROUTE_A` → Sensor 3 detecta objeto | STM32 → RPi | `[CMD_OK][ROUTE_A_OK]` | `90 BA` | Entrega confirmada na Rota A. FSM retorna para `STATE_IDLE`. |

---

#### Ciclo Completo — Objeto encaminhado para Rota B

| # | Evento / Estado FSM | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | `IDLE` → Sensor 1 detecta objeto | STM32 → RPi | `[CMD_OK][OBJ_DETECTED]` | `90 A0` | STM32 notifica detecção. FSM avança para `STATE_OBJECT_DETECTED`. |
| 2 | `OBJECT_DETECTED` → Sensor 2 detecta objeto | STM32 → RPi | `[CMD_OK][CLSS_REQUEST]` | `90 C0` | STM32 para o motor e solicita classificação. FSM avança para `STATE_WAIT_CLASSIFICATION`. |
| 3 | RPi define destino: Rota B | RPi → STM32 | `[START][ROUTE_B]` | `AA DB` | Raspberry Pi envia o resultado da classificação por visão computacional. |
| 4 | STM32 confirma e encaminha para Rota B | STM32 → RPi | `[CMD_OK][ROUTE_B_FWD]` | `90 FB` | STM32 confirma, aciona servo (`openAngle`), religa motor. FSM avança para `STATE_ROUTE_B`. |
| 5 | `ROUTE_B` → Sensor 4 detecta objeto | STM32 → RPi | `[CMD_OK][ROUTE_B_OK]` | `90 BB` | Entrega confirmada na Rota B. FSM retorna para `STATE_IDLE`. |

---

#### Troca de Modo (qualquer estado)

| # | Evento | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | RPi solicita entrada em modo debug | RPi → STM32 | `[START][DEBUG_TOGGLE]` | `AA DD` | Pode ser enviado em qualquer ponto, em qualquer estado da FSM. |
| 2 | STM32 ecoa o comando recebido | STM32 → RPi | `[CMD_OK][DEBUG_TOGGLE]` | `90 DD` | Confirmação imediata do recebimento do frame. |
| 3 | STM32 informa modo ativo | STM32 → RPi | `[CMD_OK][MODE_DEBUG]` | `90 22` | Motor travado (`stepperStopEngaged`), flash apagado. LED pisca 6× rápido. |
| 4 | RPi solicita retorno ao modo FSM | RPi → STM32 | `[START][DEBUG_TOGGLE]` | `AA DD` | Mesmo comando, reenviado para alternar de volta. |
| 5 | STM32 ecoa o comando recebido | STM32 → RPi | `[CMD_OK][DEBUG_TOGGLE]` | `90 DD` | Confirmação imediata do recebimento do frame. |
| 6 | STM32 informa modo ativo | STM32 → RPi | `[CMD_OK][MODE_FSM]` | `90 11` | FSM reinicia em `STATE_IDLE`. LED acende 400 ms e apaga. |

---

#### Reset por Software (qualquer estado, qualquer modo)

| # | Evento | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | RPi solicita reset | RPi → STM32 | `[START][SW_RESET]` | `AA 33` | Processado com prioridade máxima, antes do toggle de modo e da FSM. |
| 2 | STM32 confirma e reinicia | STM32 → RPi | `[CMD_OK][SW_RESET]` | `90 33` | STM32 envia confirmação, aguarda ~50 ms para o frame sair pelo TX e executa `NVIC_SystemReset()`. |

---

### Modo Debug — `OP_MODE_DEBUG`

> Em modo debug a FSM fica suspensa. O STM32 responde exclusivamente a comandos assíncronos e envia telemetria de sensores espontaneamente.

#### Telemetria de Sensores (espontânea)

| # | Evento | Agente | Quadro Completo | Bytes (hex) | Descrição |
|---|---|---|---|---|---|
| 1 | STATUS_BYTE dos sensores muda | STM32 → RPi | `[CMD_OK][SENS_STATUS][STATUS_BYTE]` | `90 55 NN` | Enviado apenas quando o valor do STATUS_BYTE muda em relação ao anterior. `NN`: bit 0 = SENS1, bit 1 = SENS2, bit 2 = SENS3, bit 3 = SENS4 (1 = objeto detectado, 0 = feixe livre). Bits 4–7 reservados (0). |

#### Comandos Assíncronos

| # | Comando enviado | Agente | Quadro Completo | Bytes (hex) | Ação executada pelo STM32 | Resposta STM32 | Bytes (hex) |
|---|---|---|---|---|---|---|---|
| 1 | Liga luminária | RPi → STM32 | `[START][LIGHT_EN]` | `AA E1` | `FLASH_PIN` → HIGH | `[CMD_OK][LIGHT_EN]` | `90 E1` |
| 2 | Desliga luminária | RPi → STM32 | `[START][LIGHT_DIS]` | `AA D1` | `FLASH_PIN` → LOW | `[CMD_OK][LIGHT_DIS]` | `90 D1` |
| 3 | Abre cancela | RPi → STM32 | `[START][GATE_OPEN]` | `AA E2` | Servo → `openAngle` (45°) | `[CMD_OK][GATE_OPEN]` | `90 E2` |
| 4 | Fecha cancela | RPi → STM32 | `[START][GATE_CLOSE]` | `AA D2` | Servo → `closeAngle` (0°) | `[CMD_OK][GATE_CLOSE]` | `90 D2` |
| 5 | Ativa motor (eixo engajado) | RPi → STM32 | `[START][STPR_EN]` | `AA E3` | `stepperFollowSteps()` | `[CMD_OK][STPR_EN]` | `90 E3` |
| 6 | Desativa motor (eixo livre) | RPi → STM32 | `[START][STPR_DIS]` | `AA D3` | `stepperStopDisengaged()` | `[CMD_OK][STPR_DIS]` | `90 D3` |
| 7 | Define direção: frente | RPi → STM32 | `[START][STPR_FWD]` | `AA E4` | `stepperDirInst = 0` | `[CMD_OK][STPR_FWD]` | `90 E4` |
| 8 | Define direção: trás | RPi → STM32 | `[START][STPR_BCK]` | `AA D4` | `stepperDirInst = 1` | `[CMD_OK][STPR_BCK]` | `90 D4` |
| 9 | Define quantidade de passos | RPi → STM32 | `[START][STPR_TGT][DATA]` | `AA E5 NN` | `targetStepps = DATA` (`NN` = valor de 0–255) | `[CMD_OK][STPR_TGT]` | `90 E5` |
| 10 | Comando desconhecido | RPi → STM32 | `[START][???]` | `AA ??` | Nenhuma ação | `[CMD_ERR]` | `91` |