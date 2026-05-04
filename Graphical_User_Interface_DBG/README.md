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
14. [Estrutura de Arquivos](#14-estrutura-de-arquivos)

---

## 1. Visão Geral

Este software é o painel de controle gráfico (GUI) da esteira separadora desenvolvida como trabalho final da disciplina de Sistemas Operacionais Embarcados. Ele roda no **Raspberry Pi 3B** e se comunica por **UART serial** com o firmware embarcado no **STM32G070**, permitindo:

- Monitorar em tempo real o estado da máquina de estados finitos (FSM) do firmware
- Enviar comandos de roteamento de objetos (Rota A / Rota B)
- Controlar individualmente todos os atuadores no modo de depuração (Debug):
  - Flash de iluminação
  - Cancela (servo motor)
  - Motor de passo (direção, passos, modo loop)
- Visualizar o log completo de comunicação TX/RX com timestamp
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
│         STM32G070                   │
│                                     │
│   · Firmware v2.1                   │
│   · FSM (5 estados)                 │
│   · 4 sensores laser (TIM3 DMA)     │
│   · Motor de passo A4988 (TIM6)     │
│   · Servo motor ES08MA (TIM16)      │
│   · Flash LED (GPIO)                │
│   · USART2 · PA2(TX) · PA3(RX)     │
└─────────────────────────────────────┘
```

---

## 3. Requisitos

### Hardware
| Componente | Especificação |
|---|---|
| SBC | Raspberry Pi 3B (Raspbian Bookworm ou Bullseye) |
| Microcontrolador | STM32G070 com firmware v2.1 gravado |
| Conexão atual | Cabo USB (ST-Link / USB-Serial) |
| Conexão futura | UART direta: GPIO14 (TX) ↔ PA3 (RX) / GPIO15 (RX) ↔ PA2 (TX) + GND comum |

### Software
| Pacote | Versão mínima | Origem |
|---|---|---|
| Python | 3.9+ | Incluso no Raspbian |
| tkinter | qualquer | Incluso no Python |
| pyserial | 3.5+ | `pip install pyserial` |

> **Nota:** O tkinter já vem incluso no Python do Raspbian. Caso não esteja disponível: `sudo apt install python3-tk`

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
6. Confirma que a instalação foi concluída

### Windows — `install.bat`

```bat
install.bat
```

O script executa automaticamente:
1. Verifica se Python 3 está disponível no PATH
2. Cria o ambiente virtual `venv\` na pasta do projeto
3. Instala `pyserial` dentro do venv
4. Confirma que a instalação foi concluída

> **Nota Windows:** O uso em Windows destina-se a testes com adaptador USB-Serial. A implantação definitiva é no Raspberry Pi.

---

## 5. Execução

### Linux / Raspberry Pi

```bash
# Ativar o ambiente virtual
source venv/bin/activate

# Executar a GUI
python3 esteira_control.py
```

Ou, se preferir sem ativar o venv manualmente:

```bash
venv/bin/python3 esteira_control.py
```

### Windows

```bat
venv\Scripts\activate
python esteira_control.py
```

---

## 6. Interface — Painéis e Controles

A janela é dividida em duas colunas e seis painéis:

### Barra Superior — Conexão Serial
Localizada no topo da janela. Contém:

| Controle | Função |
|---|---|
| **PORTA** | Seletor da porta serial detectada (`/dev/ttyACM0`, `/dev/ttyUSB0`, etc.) |
| **BAUD** | Taxa de transmissão (padrão: `115200`) |
| **⟳** | Atualiza a lista de portas disponíveis |
| **CONECTAR / DESCONECTAR** | Abre ou fecha a conexão serial |
| **●** | Indicador de status: cinza = desconectado, verde = conectado, vermelho = erro |
| **HANDSHAKE** | Envia `SYS_RDY (0x10)` para iniciar o handshake com o STM32 |

### Coluna Esquerda

**Painel STATUS DO HARDWARE**
Exibe o estado atual dos componentes, atualizado automaticamente a cada eco recebido do STM32:

| Campo | Valores possíveis |
|---|---|
| MODO | `FSM` (azul) / `DEBUG` (laranja) |
| CANCELA | `ABERTA` (verde) / `FECHADA` (vermelho) |
| MOTOR | `GIRANDO` (verde) / `LIVRE` / `PARADO` (cinza) |
| DIREÇÃO | `FRENTE` / `TRÁS` |
| FLASH | `ON` (amarelo) / `OFF` (cinza) |

**Painel MÁQUINA DE ESTADOS**
Exibe o nome do estado atual em destaque e um pipeline visual dos 5 estados. O estado ativo é iluminado com a cor correspondente; os demais ficam apagados.

```
[IDLE] ▸ [OBJ DET] ▸ [WAIT CLSS] ▸ [ROUTE A] ▸ [ROUTE B]
```

**Painel COMANDOS FSM**

| Botão | Comando enviado | Descrição |
|---|---|---|
| → ROTA A | `0xAA 0xDA` | Instrui o STM32 a encaminhar o objeto para a Rota A |
| → ROTA B | `0xAA 0xDB` | Instrui o STM32 a encaminhar o objeto para a Rota B |
| ⇄ TOGGLE DEBUG | `0xAA 0xDD` | Alterna entre modo FSM e modo Debug |

### Coluna Direita

**Painel CONTROLE ASSÍNCRONO (MODO DEBUG)**
Disponível em qualquer modo, mas com efeito real somente quando o firmware está em `OP_MODE_DEBUG`.

*Flash:*
| Botão | Comando | Descrição |
|---|---|---|
| ON | `0xAA 0xE1` | Liga a luminária |
| OFF | `0xAA 0xD1` | Desliga a luminária |

*Cancela:*
| Botão | Comando | Descrição |
|---|---|---|
| ABRIR | `0xAA 0xE2` | Abre a cancela (servo → openAngle = 45°) |
| FECHAR | `0xAA 0xD2` | Fecha a cancela (servo → closeAngle = 0°) |

*Motor:*
| Botão | Comando | Descrição |
|---|---|---|
| ENGAJAR | `0xAA 0xE3` | Habilita o driver A4988 e ativa a geração de pulsos |
| LIVRE | `0xAA 0xD3` | Desabilita o driver A4988 (eixo livre) |
| ◀ FRENTE | `0xAA 0xE4` | Define direção para frente |
| TRÁS ▶ | `0xAA 0xD4` | Define direção para trás |

*Controle de Passos:*

- **Nº DE PASSOS:** Campo numérico (1–255) que define o valor de `targetStepps` no STM32.
- **MODO LOOP:** Quando marcado, o software reenvia o comando de passos continuamente a cada 60 ms (≈ 16 comandos/s), mantendo o motor em rotação contínua enquanto o loop estiver ativo.
- **▶ ENVIAR PASSOS:** Envia um único `0xAA 0xE5 [steps]` ou inicia o loop.
- **■ PARAR LOOP:** Interrompe o loop de envio.

**Painel LOG DE COMUNICAÇÃO**
Exibe todas as mensagens trocadas com timestamp `HH:MM:SS.ms`:

| Cor | Tipo | Significado |
|---|---|---|
| Azul | `TX` | Frame enviado pelo RPi para o STM32 |
| Verde | `RX` | Frame recebido do STM32 |
| Amarelo | `···` | Mensagem de sistema (conexão, handshake, erros) |

Formato de cada linha:
```
[HH:MM:SS.mmm] TX  0xE1  LIGHT_EN
[HH:MM:SS.mmm] RX  0xE1  LIGHT_EN (eco)  [OK]
```

### Botão de Tema
O botão ☀/◑ no canto superior direito alterna entre **tema escuro** (padrão) e **tema claro** em tempo real, sem reiniciar a aplicação.

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
Frame padrão (2 bytes):  [0xAA] [CMD]
Frame com dado (3 bytes): [0xAA] [0xE5] [DATA]
```
O byte `0xAA` é o marcador de início de frame (`START_FRAME`). Todo comando deve ser precedido por ele.

**STM32 → RPi (RX):**
```
Confirmação positiva (2 bytes): [0x90] [ECO_DO_CMD]
Erro de reconhecimento (1 byte): [0x91]
```
O STM32 sempre responde com `0x90` seguido do byte do comando recebido como confirmação (`CMD_OK + eco`), ou com `0x91` isolado em caso de falha.

### Handshake de inicialização
```
RPi  →  STM32 : [0xAA][0x10]          (SYS_RDY)
STM32 →  RPi  : [0x90][0x01]          (CMD_OK + SYS_INIT)
```
O STM32 só responde ao handshake depois que todos os 4 sensores laser estiverem operacionais (frequência = 200 Hz detectada em todos os canais).

### Fluxo da FSM (modo normal)
```
STM32 →  RPi  : [0x90][0xA0]   OBJ_DETECTED     — objeto detectado no sensor 1
STM32 →  RPi  : [0x90][0xC0]   CLSS_REQUEST     — aguardando classificação (sensor 2 atingido)
RPi   →  STM32: [0xAA][0xDA]   ROUTE_A          — ou [0xAA][0xDB] para Rota B
STM32 →  RPi  : [0x90][0xDA]   ROUTE_A (eco)    — confirmação
STM32 →  RPi  : [0x90][0xFA]   ROUTE_A_FWD      — objeto encaminhado
STM32 →  RPi  : [0x90][0xBA]   ROUTE_A_OK       — entrega confirmada (sensor 3 atingido)
```

---

## 8. Tabela de Bytes do Protocolo

### Bytes fixos de frame
| Byte | Nome | Direção | Descrição |
|---|---|---|---|
| `0xAA` | `START_FRAME` | TX | Marcador de início de todo frame TX |
| `0x90` | `CMD_OK` | RX | Confirmação positiva |
| `0x91` | `CMD_ERR` | RX | Erro de reconhecimento |

### Handshake
| Byte | Nome | Direção | Descrição |
|---|---|---|---|
| `0x10` | `SYS_RDY` | TX | Inicia o handshake (enviado pelo RPi) |
| `0x01` | `SYS_INIT` | RX | Completa o handshake (enviado pelo STM32) |

### Mensagens espontâneas da FSM (STM32 → RPi)
| Byte | Nome | Descrição |
|---|---|---|
| `0xA0` | `OBJ_DETECTED` | Objeto detectado pelo sensor 1 |
| `0xC0` | `CLSS_REQUEST` | Sensor 2 atingido — aguardando rota |
| `0xFA` | `ROUTE_A_FWD` | Encaminhamento para Rota A iniciado |
| `0xFB` | `ROUTE_B_FWD` | Encaminhamento para Rota B iniciado |
| `0xBA` | `ROUTE_A_OK` | Entrega confirmada na Rota A |
| `0xBB` | `ROUTE_B_OK` | Entrega confirmada na Rota B |

### Comandos de roteamento FSM (RPi → STM32)
| Byte | Nome | Descrição |
|---|---|---|
| `0xDA` | `ROUTE_A` | Define destino como Rota A |
| `0xDB` | `ROUTE_B` | Define destino como Rota B |

### Comandos assíncronos (RPi → STM32)
| Byte | Nome | Descrição |
|---|---|---|
| `0xE1` | `LIGHT_EN` | Liga a luminária |
| `0xD1` | `LIGHT_DISABLE` | Desliga a luminária |
| `0xE2` | `GATE_OPEN` | Abre a cancela (servo → 45°) |
| `0xD2` | `GATE_CLOSE` | Fecha a cancela (servo → 0°) |
| `0xE3` | `STPR_EN` | Habilita o motor de passo |
| `0xD3` | `STPR_DISABLE` | Desabilita o motor de passo |
| `0xE4` | `SET_STPR_FORWARD` | Define direção: frente |
| `0xD4` | `SET_STPR_BACKWARD` | Define direção: trás |
| `0xE5` | `SET_STPR_TGT_STPS` | Define número de passos (frame de 3 bytes: `[0xAA][0xE5][steps]`) |
| `0xDD` | `DEBUG_MODE_TOGGLE` | Alterna entre modo FSM e modo Debug |

---

## 9. Arquitetura Interna do Software

### Classe `SerialManager`
Gerencia a porta serial com duas threads independentes:

**Thread RX (`serial-rx`):**
- Lê bytes da porta em loop com timeout de 20 ms
- Implementa um parser de estados (`IDLE` → `WAIT_PAYLOAD`) para montar os frames de 2 bytes recebidos do STM32
- Notifica a GUI via callbacks quando um frame completo chega
- Usa `port_lock` apenas para acessar o objeto `serial.Serial`, não bloqueia TX

**Thread TX (`serial-tx`):**
- Drena uma `queue.Queue` em loop com timeout de 500 ms
- Escreve na porta assim que um frame é enfileirado
- O enfileiramento pelo método `send_frame()` é instantâneo (não bloqueia a thread principal/GUI)
- Sem disputa de lock com a thread RX → latência uniforme e mínima para todos os comandos

**Por que isso importa:**
Na versão anterior, TX e RX compartilhavam um único `threading.Lock`. Quando a thread RX estava dentro do bloco `with self._lock`, qualquer chamada de `send_frame()` na thread principal ficava bloqueada esperando. Com a arquitetura de fila, o envio é sempre assíncrono — o frame vai para a fila em microssegundos e a thread TX o despacha na primeira oportunidade.

### Classe `EsteiraApp`
Herda de `tk.Tk`. Organizada nos seguintes grupos de métodos:

| Grupo | Métodos | Responsabilidade |
|---|---|---|
| Tema | `_toggle_theme`, `_recolor_all`, `_tw` | Troca de paleta em tempo real |
| Construção de UI | `_build_*` | Criação dos widgets na inicialização |
| Callbacks seriais | `_on_serial_event`, `_handle_rx` | Atualização da GUI ao receber dados |
| Ações | `_send`, `_toggle_connect`, `_start_loop` | Envio de comandos e controle de estado |
| Log | `_log`, `_log_sys`, `_clear_log` | Escrita colorida no widget Text |

### Sistema de Recoloração de Tema
Cada widget que precisa mudar de cor ao trocar de tema é registrado via `_tw(widget, prop=color_key)`. O dicionário `C` é substituído pelo tema novo, e `_recolor_all()` percorre a lista aplicando `C[color_key]` a cada propriedade. Isso evita recriar a janela e mantém o estado da sessão intacto durante a troca.

---

## 10. Modos de Operação

### Modo FSM (`OP_MODE_FSM`)
Modo normal de operação. O firmware executa a lógica da esteira autonomamente, avançando pelos estados conforme os sensores são ativados. A GUI acompanha passivamente via mensagens espontâneas e só intervém ao enviar ROTA A ou ROTA B quando o firmware solicita classificação.

### Modo Debug (`OP_MODE_DEBUG`)
Ativado pelo comando `DEBUG_TOGGLE (0xDD)`. A FSM do firmware é pausada e apenas os comandos assíncronos são processados. Permite:
- Testar cada atuador individualmente
- Calibrar ângulos da cancela
- Verificar resposta do motor de passo
- Validar sensores de forma isolada

Para sair do modo Debug e retornar à FSM, envie `DEBUG_TOGGLE (0xDD)` novamente. O firmware reinicia do estado `STATE_IDLE`.

---

## 11. Máquina de Estados Finitos (FSM)

A FSM reside no firmware do STM32. A GUI a observa e reflete seu estado:

```
                    SENS1 ativo
    ┌─────────────────────────────────────────────────────────────────┐
    ▼                                                                 │
 STATE_IDLE ──SENS1──▶ STATE_OBJECT_DETECTED ──SENS2──▶ STATE_WAIT_CLASSIFICATION
    ▲                                                          │
    │                                               ROUTE_A_RECV│ROUTE_B_RECV
    │                                                    ▼           ▼
    │                                              STATE_ROUTE_A  STATE_ROUTE_B
    │                                                    │           │
    └─────────────────────────SENS3 / SENS4──────────────┘           │
    └───────────────────────────────────────────────────────────────-┘
```

| Estado | ID | Cor na GUI | Condição de entrada |
|---|---|---|---|
| `STATE_IDLE` | 0 | Cinza | Inicialização ou entrega concluída |
| `STATE_OBJECT_DETECTED` | 1 | Amarelo | Sensor 1 interrompido |
| `STATE_WAIT_CLASSIFICATION` | 2 | Azul | Sensor 2 interrompido |
| `STATE_ROUTE_A` | 3 | Verde | Recebido `ROUTE_A (0xDA)` |
| `STATE_ROUTE_B` | 4 | Laranja | Recebido `ROUTE_B (0xDB)` |

---

## 12. Conexão com o STM32G070

### Conexão atual — USB (ST-Link)
O cabo USB conectado à placa Nucleo/Discovery cria uma porta serial virtual:
- Linux: `/dev/ttyACM0` ou `/dev/ttyUSB0`
- Windows: `COM3`, `COM4`, etc.

Selecione a porta no combobox da GUI e clique em CONECTAR.

### Conexão futura — UART direta (GPIO)
Para substituir o USB pela UART direta entre RPi 3B e STM32:

**Pinagem:**
| RPi 3B | Pino físico | STM32G070 | Observação |
|---|---|---|---|
| GPIO14 (TXD) | Pino 8 | PA3 (RX da USART2) | Cruzado |
| GPIO15 (RXD) | Pino 10 | PA2 (TX da USART2) | Cruzado |
| GND | Pino 6 | GND | Obrigatório |

Ambos operam em 3,3 V lógico — não é necessário conversor de nível, mas o GND comum é obrigatório caso sejam alimentados separadamente.

**Habilitando a UART no Raspbian:**
```bash
sudo raspi-config
# Interface Options → Serial Port
# "Would you like a login shell...?" → No
# "Would you like the serial port hardware...?" → Yes
sudo reboot
```

Após o reboot a porta aparecerá como `/dev/ttyS0` ou `/dev/ttyAMA0`.

---

## 13. Solução de Problemas

**A porta serial não aparece no seletor**
- Clique em ⟳ para atualizar a lista
- Verifique se o cabo USB está conectado: `ls /dev/tty*`
- Adicione seu usuário ao grupo `dialout`: `sudo usermod -aG dialout $USER` (requer logout)

**"Permission denied" ao conectar**
```bash
sudo usermod -aG dialout $USER
# Faça logout e login novamente
```

**Handshake não completa**
- Verifique se os 4 sensores laser estão com o feixe livre (o STM32 aguarda 200 Hz em todos os canais antes de aceitar o handshake)
- Confirme que o firmware v2.1 está gravado no STM32
- O LED de debug do STM32 pisca lento (500 ms) quando os sensores estão OK e aguarda o `SYS_RDY`; pisca rápido (100 ms) se algum sensor não estiver operacional

**Comandos sem resposta no modo Debug**
- Verifique se o modo Debug está ativo (painel STATUS deve mostrar `DEBUG` em laranja)
- Envie `⇄ TOGGLE DEBUG` para alternar o modo e confirme via log

**`ModuleNotFoundError: No module named 'serial'`**
- O ambiente virtual não está ativo. Execute: `source venv/bin/activate`

**`No module named 'tkinter'`**
```bash
sudo apt install python3-tk
```

---

## 14. Estrutura de Arquivos

```
Graphical_User_Interface_DBG/
├── esteira_control.py   # Aplicação principal (GUI + SerialManager)
├── install.sh           # Script de instalação para Linux / Raspberry Pi
├── install.bat          # Script de instalação para Windows
├── README.md            # Este documento
└── venv/                # Ambiente virtual Python (criado pelo install)
    ├── bin/             # Executáveis (Linux)
    │   ├── python3
    │   └── pip
    └── lib/
        └── python3.x/
            └── site-packages/
                └── serial/   # pyserial instalado aqui
```

---

*Projeto desenvolvido para a disciplina de Sistemas Operacionais Embarcados — 2026.1*
*Hardware: STM32G070 @ 64 MHz · Raspberry Pi 3B · Driver A4988 · Servo ES08MA*
