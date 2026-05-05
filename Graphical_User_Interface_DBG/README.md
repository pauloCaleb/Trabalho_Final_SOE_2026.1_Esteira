# Esteira Separadora — Painel de Controle UART
### GUI de Diagnóstico e Controle · STM32G070 ↔ Raspberry Pi 3B
**Trabalho Final — Sistemas Operacionais Embarcados (SOE) · 2026.1**
Autores: Paulo Caleb Fernandes da Silva · Felipe de Castro

---

## Sumário

1. [Visão Geral](#1-visão-geral)
2. [Arquitetura do Sistema](#2-arquitetura-do-sistema)
3. [Requisitos](#3-requisitos)
4. [Instalação](#4-instalação)
5. [Execução](#5-execução)
6. [Interface — Painéis e Controles](#6-interface--painéis-e-controles)
7. [Protocolo de Comunicação UART](#7-protocolo-de-comunicação-uart)
8. [Tabela de Bytes do Protocolo](#8-tabela-de-bytes-do-protocolo)
9. [Arquitetura Interna do Software](#9-arquitetura-interna-do-software)
10. [Modos de Operação](#10-modos-de-operação)
11. [Máquina de Estados Finitos (FSM)](#11-máquina-de-estados-finitos-fsm)
12. [Conexão com o STM32G070](#12-conexão-com-o-stm32g070)
13. [Solução de Problemas](#13-solução-de-problemas)
14. [Changelog](#14-changelog)
15. [Estrutura de Arquivos](#15-estrutura-de-arquivos)

---

## 1. Visão Geral

Este software é o painel de controle gráfico (GUI) da esteira separadora desenvolvida como trabalho final da disciplina de Sistemas Operacionais Embarcados. Ele roda no **Raspberry Pi 3B** e se comunica por **UART serial** com o firmware embarcado no **STM32G070**, permitindo:

- Monitorar em tempo real o estado da máquina de estados finitos (FSM) do firmware
- Enviar comandos de roteamento de objetos (Rota A / Rota B)
- Controlar individualmente todos os atuadores no modo de depuração (Debug):
  - Flash de iluminação
  - Cancela (servo motor)
  - Motor de passo (direção, passos, modo loop)
- Visualizar estado dos 4 sensores laser em tempo real (modo Debug)
- Visualizar o log completo de comunicação TX/RX com timestamp e exportar para .txt
- Reconectar automaticamente em caso de queda de comunicação
- Resetar o STM32 por software via comando UART
- Alternar entre tema escuro e claro

O software **não** realiza processamento de imagem nem tomada de decisão sobre rotas — essa lógica reside no firmware do STM32. A GUI apenas comanda e observa.

---

## 2. Arquitetura do Sistema

```
┌─────────────────────────────────────┐
│         Raspberry Pi 3B             │
│                                     │
│   ┌─────────────────────────────┐   │
│   │  esteira_control.py  (GUI)  │   │
│   │  · tkinter (interface)      │   │
│   │  · SerialManager            │   │
│   │    ├─ Thread TX (queue)     │   │
│   │    └─ Thread RX (parser)    │   │
│   └──────────────┬──────────────┘   │
│                  │ UART 115200 8N1  │
│            USB / GPIO14-15          │
└─────────────────┬───────────────────┘
                  │
┌─────────────────┴───────────────────┐
│         STM32G070  — FW v3.0        │
│                                     │
│   · FSM (5 estados)                 │
│   · 4 sensores laser (TIM3 DMA)     │
│   · Motor de passo A4988 (TIM6)     │
│   · Servo motor ES08MA (TIM16)      │
│   · Flash LED (GPIO)                │
│   · Telemetria de sensores (0x55)   │
│   · Reset por software (NVIC)       │
│   · USART2 · PA2(TX) · PA3(RX)     │
└─────────────────────────────────────┘
```

---

## 3. Requisitos

### Hardware
| Componente | Especificação |
|---|---|
| SBC | Raspberry Pi 3B (Raspbian Bookworm ou Bullseye) |
| Microcontrolador | STM32G070 com firmware v3.0 gravado |
| Conexão atual | Cabo USB (ST-Link / USB-Serial) |
| Conexão futura | UART direta: GPIO14 (TX) ↔ PA3 (RX) / GPIO15 (RX) ↔ PA2 (TX) + GND comum |

### Software
| Pacote | Versão mínima | Origem |
|---|---|---|
| Python | 3.9+ | Incluso no Raspbian |
| tkinter | qualquer | Incluso no Python |
| pyserial | 3.5+ | `pip install pyserial` |

---

## 4. Instalação

### Linux / Raspberry Pi — `install.sh`

```bash
chmod +x install.sh
./install.sh
```

O script executa automaticamente:
1. Verifica se Python 3 está disponível
2. Instala `python3-full` via apt (necessário para criar venv no Bookworm)
3. Cria o ambiente virtual `venv/` dentro da pasta do projeto
4. Instala `pyserial` dentro do venv
5. Verifica se `python3-tk` está presente no sistema
6. Adiciona o usuário ao grupo `dialout` (acesso à porta serial)

### Windows — `install.bat`

```bat
install.bat
```

---

## 5. Execução

### Linux / Raspberry Pi

```bash
source venv/bin/activate
python3 esteira_control.py
```

### Windows

```bat
venv\Scripts\activate
python esteira_control.py
```

---

## 6. Interface — Painéis e Controles

### Barra Superior — Conexão Serial

| Controle | Função |
|---|---|
| **PORTA** | Seletor da porta serial detectada |
| **BAUD** | Taxa de transmissão (padrão: 115200) |
| **⟳** | Atualiza lista de portas |
| **CONECTAR / DESCONECTAR** | Abre ou fecha a conexão serial |
| **◆** | Indicador de atividade serial — pisca (azul) a cada frame TX ou RX |
| **●** | Status da conexão: cinza = desconectado, verde = conectado, vermelho = erro |
| **HANDSHAKE** | Envia `SYS_RDY (0x10)` para iniciar o handshake com o STM32 |
| **SW RESET** | Envia `SW_RESET (0x33)` para reset por software do STM32 (pede confirmação) |

### Coluna Esquerda

**Painel STATUS DO HARDWARE**

| Campo | Valores |
|---|---|
| MODO | `FSM` (azul) / `DEBUG` (laranja) — sincronizado via mensagem do firmware |
| CANCELA | `ABERTA` (verde) / `FECHADA` (vermelho) |
| MOTOR | `GIRANDO` (verde) / `LIVRE` / `PARADO` (cinza) |
| DIREÇÃO | `FRENTE` / `TRÁS` |
| FLASH | `ON` (amarelo) / `OFF` (cinza) |

**Painel MÁQUINA DE ESTADOS**
Pipeline visual dos 5 estados com o estado ativo iluminado. Atualizado pelas mensagens espontâneas do firmware.

**Painel COMANDOS FSM**

| Botão | Comando | Descrição |
|---|---|---|
| → ROTA A | `0xAA 0xDA` | Encaminha objeto para Rota A |
| → ROTA B | `0xAA 0xDB` | Encaminha objeto para Rota B |
| ⇄ TOGGLE DEBUG | `0xAA 0xDD` | Alterna FSM ↔ Debug |

**Painel SENSORES LASER**
Exibe 4 indicadores retangulares (S1–S4). Ativo apenas no **modo Debug** — o firmware envia a telemetria `SENS_STATUS_MSG (0x55)` somente quando o modo Debug está ativo e o status dos sensores muda.

| Estado | Visual |
|---|---|
| Feixe livre | Caixa cinza / texto "LIVRE" |
| Objeto detectado | Caixa verde (escuro: vermelha) / texto "OBJETO" |

### Coluna Direita

**Painel CONTROLE ASSÍNCRONO (MODO DEBUG)**
Controles de flash, cancela, motor e passos. Idêntico à versão anterior. O modo loop envia `STPR_TGT_STPS` continuamente a 16 cmd/s (60 ms/ciclo).

**Painel LOG DE COMUNICAÇÃO**

| Cor | Tipo | Significado |
|---|---|---|
| Azul | `TX` | Frame enviado pelo RPi |
| Verde | `RX` | Frame recebido do STM32 |
| Amarelo | `···` | Mensagem de sistema |
| Vermelho | `RX` | CMD_ERR recebido |

- **ERR: N** — contador de erros de comunicação. Reseta ao limpar o log.
- **EXPORTAR LOG** — salva o log atual em arquivo `.txt` com timestamp no nome.
- **LIMPAR LOG** — apaga o log e zera o contador de erros.

### Botão de Tema
O botão ☀/◑ no canto superior direito alterna entre tema escuro e claro em tempo real. O tema é aplicado completamente desde a inicialização — sem artefatos visuais na abertura do programa.

---

## 7. Protocolo de Comunicação UART

### Configuração da porta
```
Baud rate : 115200
Bits      : 8
Paridade  : Nenhuma
Stop bits : 1
Flow ctrl : Nenhum
```

### Estrutura dos frames

**RPi → STM32 (TX):**
```
Frame padrão (2 bytes):   [0xAA] [CMD]
Frame com dado (3 bytes):  [0xAA] [0xE5] [steps]
```

**STM32 → RPi (RX):**
```
Confirmação (2 bytes):    [0x90] [ECO_DO_CMD]
Telemetria (3 bytes):     [0x90] [0x55] [STATUS_BYTE]
Erro (1 byte):            [0x91]
```

### Handshake de inicialização
```
RPi   → STM32 : [0xAA][0x10]          SYS_RDY
STM32 → RPi   : [0x90][0x01]          CMD_OK + SYS_INIT
STM32 → RPi   : [0x90][0x11]          CMD_OK + MODE_FSM  ← informa modo inicial
```

### Reset por software
```
RPi   → STM32 : [0xAA][0x33]          SW_RESET
STM32 → RPi   : [0x90][0x33]          CMD_OK + eco  (antes de resetar)
[STM32 executa NVIC_SystemReset()]
[STM32 reinicia e aguarda novo handshake]
```

### Telemetria de sensores (STATUS_BYTE)
```
STM32 → RPi   : [0x90][0x55][STATUS_BYTE]

STATUS_BYTE:
  bit 0 → SENS1_flag  (1 = objeto / 0 = livre)
  bit 1 → SENS2_flag
  bit 2 → SENS3_flag
  bit 3 → SENS4_flag
  bits 4-7 → reservados (0)

Exemplos:
  0x00 → todos livres
  0x01 → S1 com objeto
  0x05 → S1 e S3 com objeto (0b00000101)
  0x0F → todos com objeto
```

Enviada **somente no modo Debug** e **somente quando o STATUS_BYTE mudar** em relação ao último valor transmitido.

---

## 8. Tabela de Bytes do Protocolo

### Bytes de frame
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0xAA` | `START_FRAME` | TX | Marcador de início |
| `0x90` | `CMD_OK` | RX | Confirmação positiva |
| `0x91` | `CMD_ERR` | RX | Erro de reconhecimento |

### Handshake
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0x10` | `SYS_RDY` | TX | Inicia handshake |
| `0x01` | `SYS_INIT` | RX | Completa handshake |

### Modo de operação (espontâneas — v3.0)
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0x11` | `MODE_FSM` | RX | STM32 confirmou modo FSM |
| `0x22` | `MODE_DEBUG` | RX | STM32 confirmou modo Debug |

### Telemetria (espontânea — v3.0)
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0x55` | `SENS_STATUS` | RX | STATUS_BYTE dos 4 sensores (frame de 3 bytes) |

### Reset por software (v3.0)
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0x33` | `SW_RESET` | TX | Solicita reset por software |

### FSM — Roteamento
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0xDA` | `ROUTE_A` | TX | Define destino Rota A |
| `0xDB` | `ROUTE_B` | TX | Define destino Rota B |
| `0xA0` | `OBJ_DETECTED` | RX | Objeto no sensor 1 |
| `0xC0` | `CLSS_REQUEST` | RX | Aguardando classificação |
| `0xFA` | `ROUTE_A_FWD` | RX | Encaminhamento Rota A |
| `0xFB` | `ROUTE_B_FWD` | RX | Encaminhamento Rota B |
| `0xBA` | `ROUTE_A_OK` | RX | Entrega confirmada Rota A |
| `0xBB` | `ROUTE_B_OK` | RX | Entrega confirmada Rota B |

### Comandos assíncronos
| Byte | Nome | Dir | Descrição |
|---|---|---|---|
| `0xE1` | `LIGHT_EN` | TX | Liga luminária |
| `0xD1` | `LIGHT_DIS` | TX | Desliga luminária |
| `0xE2` | `GATE_OPEN` | TX | Abre cancela |
| `0xD2` | `GATE_CLOSE` | TX | Fecha cancela |
| `0xE3` | `STPR_EN` | TX | Habilita motor de passo |
| `0xD3` | `STPR_DIS` | TX | Desabilita motor de passo |
| `0xE4` | `STPR_FWD` | TX | Direção: frente |
| `0xD4` | `STPR_BWD` | TX | Direção: trás |
| `0xE5` | `STPR_TGT` | TX | Define passos (frame 3 bytes) |
| `0xDD` | `DBG_TOGGLE` | TX | Alterna FSM ↔ Debug |

---

## 9. Arquitetura Interna do Software

### Classe `SerialManager`

**Thread TX (`serial-tx`):** drena `queue.Queue` sem disputar lock com RX — enfileirar é sempre instantâneo independente do estado do barramento.

**Thread RX (`serial-rx`):** parser de estados com suporte a frames de 2 e 3 bytes:
```
IDLE → WAIT_CMD → (payload normal: notifica GUI)
                → WAIT_DATA → (telemetria 0x55: notifica GUI com data)
```

### Classe `EsteiraApp`

**Inicialização do tema:** o dicionário `C` é populado antes de qualquer widget ser criado e `_recolor_all()` é chamado explicitamente logo após `_build_ui()`. Isso garante que o tema seja consistente desde o primeiro frame renderizado, eliminando os artefatos visuais da versão anterior.

**Sincronização de modo:** o campo MODO na GUI só é atualizado ao receber `MODE_FSM_MSG (0x11)` ou `MODE_DEBUG_MSG (0x22)` do firmware — nunca por toggle local. Isso torna a exibição do modo imune a dessincronismos por perda de frame ou reset do STM32.

**Reconexão automática:** ao detectar queda de conexão (`_on_disconnected`), uma thread separada tenta reconectar à última porta/baud usada a cada `RECONNECT_DELAY` (3 s). A reconexão é cancelada se o usuário clicar em DESCONECTAR ou fechar a janela.

**Indicador de atividade:** `_flash_activity()` acende o LED ◆ por `ACTIVITY_ON_MS` (120 ms) a cada frame TX ou RX, usando `after()` para cancelar acendimentos anteriores e evitar acúmulo de callbacks.

**Error counter:** incrementado a cada `CMD_ERR (0x91)` recebido. Exibido em vermelho ao lado do log. Reseta junto com o log.

**Exportação de log:** cada linha escrita no widget Text é também armazenada em `_log_lines`. O botão EXPORTAR abre um diálogo de salvamento e escreve o buffer com cabeçalho (data, total de erros).

---

## 10. Modos de Operação

### Modo FSM (`OP_MODE_FSM`)
Modo normal. O firmware executa a lógica da esteira autonomamente. A GUI acompanha via mensagens espontâneas e intervém apenas ao enviar ROTA A ou ROTA B. A telemetria de sensores **não** é enviada neste modo para manter o barramento limpo para o algoritmo de controle automático externo.

### Modo Debug (`OP_MODE_DEBUG`)
Ativado pelo comando `DEBUG_TOGGLE (0xDD)`. A FSM é pausada, todos os comandos assíncronos são processados e a telemetria de sensores (`SENS_STATUS_MSG`) passa a ser enviada sempre que o status mudar. O firmware confirma a troca de modo com `MODE_DEBUG_MSG (0x22)` ou `MODE_FSM_MSG (0x11)`.

---

## 11. Máquina de Estados Finitos (FSM)

```
    ┌──────────────────────────────────────────────────────────────┐
    ▼                                                              │
 STATE_IDLE ──SENS1──▶ STATE_OBJ_DETECTED ──SENS2──▶ STATE_WAIT_CLSS
    ▲                                                     │
    │                                          ROUTE_A │ ROUTE_B
    │                                              ▼         ▼
    │                                        STATE_ROUTE_A  STATE_ROUTE_B
    │                                              │              │
    └──────────────────────────SENS3 / SENS4───────┘──────────────┘
```

| Estado | ID | Cor | Condição |
|---|---|---|---|
| `STATE_IDLE` | 0 | Cinza | Inicialização ou entrega concluída |
| `STATE_OBJECT_DETECTED` | 1 | Amarelo | Sensor 1 interrompido |
| `STATE_WAIT_CLASSIFICATION` | 2 | Azul | Sensor 2 interrompido |
| `STATE_ROUTE_A` | 3 | Verde | Recebido `ROUTE_A` |
| `STATE_ROUTE_B` | 4 | Laranja | Recebido `ROUTE_B` |

---

## 12. Conexão com o STM32G070

### Conexão atual — USB (ST-Link)
Porta virtual criada pelo ST-Link: `/dev/ttyACM0` ou `/dev/ttyUSB0` no Linux, `COM_N` no Windows.

### Conexão futura — UART direta (GPIO)

| RPi 3B | Pino físico | STM32G070 |
|---|---|---|
| GPIO14 (TXD) | 8 | PA3 (RX da USART2) |
| GPIO15 (RXD) | 10 | PA2 (TX da USART2) |
| GND | 6 | GND |

Ambos operam em 3,3 V — sem conversor de nível necessário.

**Habilitando UART no Raspbian:**
```bash
sudo raspi-config
# Interface Options → Serial Port → shell: No → hardware: Yes
sudo reboot
```

---

## 13. Solução de Problemas

**Porta não aparece no seletor**
```bash
ls /dev/tty*      # identifica a porta
sudo usermod -aG dialout $USER   # adiciona ao grupo (requer logout)
```

**Handshake não completa**
- LED do STM32 pisca lento (500 ms) = sensores OK, aguardando `SYS_RDY`
- LED pisca rápido (100 ms) = algum sensor não detectou portadora (200 Hz)
- Verifique se o firmware v3.0 está gravado (v2.x não responde `MODE_FSM_MSG`)

**MODO mostra valor errado após reset do STM32**
- Com firmware v3.0, o handshake sempre envia `MODE_FSM_MSG` → a GUI se autorrecalibra
- Se usar firmware v2.x, o MODO pode dessincronizar — atualize o firmware

**`ModuleNotFoundError: No module named 'serial'`**
```bash
source venv/bin/activate
```

**`No module named 'tkinter'`**
```bash
sudo apt install python3-tk
```

---

## 14. Changelog

### v3 (GUI) / v3.0 (Firmware) — atual
- **[FIX] Tema na inicialização:** `_recolor_all()` chamado explicitamente após `_build_ui()` — sem artefatos visuais na abertura
- **[NEW] Sincronização de modo:** campo MODO atualizado via `MODE_FSM_MSG (0x11)` e `MODE_DEBUG_MSG (0x22)` — sem risco de dessincronismo
- **[NEW] Reconexão automática:** detecta queda e tenta reconectar a cada 3 s
- **[NEW] Indicador de atividade serial:** LED ◆ pisca a cada frame TX/RX
- **[NEW] Exportação de log:** botão salva log em `.txt` com cabeçalho e timestamp
- **[NEW] Error counter:** conta `CMD_ERR` recebidos; reseta com o log
- **[NEW] Painel de sensores:** 4 indicadores S1–S4 atualizados pela telemetria `SENS_STATUS_MSG (0x55)`
- **[NEW] SW RESET:** botão envia `0x33` para `NVIC_SystemReset()` no STM32
- **[FW] `sendFrame3()`:** nova função para frames TX de 3 bytes
- **[FW] `sendTelemetryData()`:** telemetria de sensores compactada, só no modo Debug, só quando muda
- **[FW] `MODE_FSM_MSG / MODE_DEBUG_MSG`:** anunciados após toggle de modo e no handshake
- **[FW] `SW_RESET_MSG`:** processado em `handleModeToggle()` antes do toggle normal

### v2 (GUI) / v2.1 (Firmware)
- Fila TX dedicada (threads TX e RX independentes)
- Loop de passos a 60 ms (≈16 cmd/s)
- Tema claro/escuro alternável em tempo real
- Parser UART com suporte a frame de 3 bytes (`SET_STPR_TGT_STPS`)
- `sendFrame()` padronizado no firmware

### v1 (GUI) / v1.0 (Firmware)
- Versão inicial com tkinter e pyserial
- FSM de 5 estados, 4 sensores laser, servo, motor de passo
- Comunicação UART com handshake

---

## 15. Estrutura de Arquivos

```
Graphical_User_Interface_DBG/
├── esteira_control.py   # Aplicação principal (GUI v3 + SerialManager)
├── install.sh           # Instalação Linux / Raspberry Pi
├── install.bat          # Instalação Windows
├── README.md            # Este documento
└── venv/                # Ambiente virtual Python (criado pelo install)
```

---

*Projeto desenvolvido para a disciplina de Sistemas Operacionais Embarcados — 2026.1*
*Hardware: STM32G070 @ 64 MHz · Raspberry Pi 3B · Driver A4988 · Servo ES08MA*
*Firmware v3.0 · GUI v3*
