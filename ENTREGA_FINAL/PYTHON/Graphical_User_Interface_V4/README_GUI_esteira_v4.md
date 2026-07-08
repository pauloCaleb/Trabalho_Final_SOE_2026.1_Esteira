# Esteira Separadora — GUI v4.0

**Sistema de Controle com Interface Gráfica**
STM32G070 ↔ Raspberry Pi 3B · Firmware v3.0 · SOE 2026.1

Autores: Felipe de Castro · Paulo Caleb Fernandes da Silva

---

## Índice

1. [Visão Geral](#1-visão-geral)
2. [Requisitos](#2-requisitos)
3. [Instalação](#3-instalação)
4. [Como Executar](#4-como-executar)
5. [Estrutura da Interface](#5-estrutura-da-interface)
   - 5.1 [Barra Superior Compartilhada](#51-barra-superior-compartilhada)
   - 5.2 [Aba APLICACAO — FSM Automática](#52-aba-aplicacao--fsm-automática)
   - 5.3 [Aba DEBUG — Controle Assíncrono](#53-aba-debug--controle-assíncrono)
6. [Fluxo de Operação Normal (Aba APLICACAO)](#6-fluxo-de-operação-normal-aba-aplicacao)
7. [Protocolo de Comunicação UART](#7-protocolo-de-comunicação-uart)
   - 7.1 [Estrutura dos Frames](#71-estrutura-dos-frames)
   - 7.2 [Tabela de Comandos — GUI → STM32](#72-tabela-de-comandos--gui--stm32)
   - 7.3 [Tabela de Mensagens — STM32 → GUI](#73-tabela-de-mensagens--stm32--gui)
8. [Leitura e Interpretação de QR Codes](#8-leitura-e-interpretação-de-qr-codes)
9. [Sistema de Log](#9-sistema-de-log)
10. [Temas Visual (Claro / Escuro)](#10-temas-visual-claro--escuro)
11. [Máquina de Estados (FSM)](#11-máquina-de-estados-fsm)
12. [Arquitetura do Código](#12-arquitetura-do-código)
13. [Comportamento de Reconexão Automática](#13-comportamento-de-reconexão-automática)
14. [Formato dos QR Codes do Projeto](#14-formato-dos-qr-codes-do-projeto)
15. [Solução de Problemas](#15-solução-de-problemas)
16. [Histórico de Versões](#16-histórico-de-versões)

---

## 1. Visão Geral

A `esteira_v4.py` é a interface gráfica principal do projeto Esteira Separadora. Ela roda no **Raspberry Pi 3B** (ou em qualquer PC com Windows/Linux para testes com adaptador USB-Serial) e se comunica com o **STM32G070** via UART a 115200 baud.

A v4.0 unifica em um único script duas funcionalidades que antes existiam em arquivos separados:

- **Aba APLICACAO** — fluxo automático de classificação por QR Code. A câmera lê o código, extrai o destino e envia o comando de rota ao STM32 sem intervenção manual.
- **Aba DEBUG** — painel de controle assíncrono completo para desenvolvimento e diagnóstico. Permite acionar individualmente todos os atuadores (flash, cancela, motor de passo), monitorar os sensores laser em tempo real e enviar comandos FSM manualmente.

As duas abas compartilham uma única instância de `SerialManager` e um único estado da FSM, garantindo consistência total dos dados independentemente de qual aba estiver visível.

---

## 2. Requisitos

### Hardware

| Componente | Descrição |
|---|---|
| Raspberry Pi 3B | Executa este script (plataforma definitiva) |
| STM32G070 | Microcontrolador da esteira (firmware v3.0) |
| Cabo USB-Serial | Para testes no PC (ex: CH340, CP2102) |
| Câmera USB | Para leitura de QR Code (índice 0 por padrão) |

### Software

| Pacote | Versão mínima | Uso |
|---|---|---|
| Python | 3.10+ | Interpretador |
| pyserial | 3.5+ | Comunicação UART |
| opencv-python | 4.5+ | Captura e decodificação de QR |
| Pillow | 9.0+ | Renderização do frame no canvas tkinter |

> **Nota:** `opencv-python` e `Pillow` são opcionais para a aba DEBUG. Sem eles, a aba APLICACAO funciona sem câmera (os comandos de rota podem ser enviados manualmente pelos botões ROTA A / ROTA B na aba DEBUG).

---

## 3. Instalação

### No Raspberry Pi ou Linux

```bash
# Clonar o repositório
git clone <url_do_repo>
cd <pasta_do_projeto>

# Criar ambiente virtual (recomendado)
python3 -m venv --without-pip venv
python3 -m ensurepip --upgrade
venv/bin/pip install pyserial opencv-python Pillow
```

### No Windows (para testes com USB-Serial)

Execute o `install.bat` incluído no projeto. Ele cria o ambiente virtual, instala todas as dependências e verifica o arquivo principal automaticamente.

```bat
install.bat
```

Após a instalação:

```bat
venv\Scripts\activate
python esteira_v4.py
```

---

## 4. Como Executar

```bash
# Com o ambiente virtual ativo:
python esteira_v4.py

# Ou diretamente:
venv/bin/python esteira_v4.py       # Linux/Mac
venv\Scripts\python.exe esteira_v4.py  # Windows
```

Se `opencv-python` ou `Pillow` não estiverem instalados, o script exibe um aviso no terminal e inicia normalmente — apenas a câmera ficará desabilitada na aba APLICACAO.

---

## 5. Estrutura da Interface

### 5.1 Barra Superior Compartilhada

Sempre visível, independente da aba ativa. Contém:

| Elemento | Função |
|---|---|
| Título e versão | Identificação do sistema |
| Botão `*` (tema) | Alterna entre tema escuro e claro |
| Botões de aba | `APLICACAO` e `DEBUG` — troca de aba com salvamento do log |
| Seletor PORTA | Lista as portas seriais disponíveis (COM3, /dev/ttyUSB0, etc.) |
| Seletor BAUD | Taxa de comunicação (padrão: 115200) |
| Botão `R` | Atualiza a lista de portas seriais |
| Botão `CONECTAR` / `DESCONECTAR` | Gerencia a conexão UART |
| Indicador `*` (atividade) | Pisca a cada frame TX ou RX recebido (120 ms) |
| LED `*` (status) | Verde = conectado, Vermelho = erro/queda |
| Label de status | Exibe `CONECTADO COMx` ou `DESCONECTADO` |
| Botão `HANDSHAKE` | Envia `[0xAA][0x10]` para inicializar o STM32 |
| Botão `SW RESET` | Envia `[0xAA][0x33]` para reset por software do STM32 |

> O botão `HANDSHAKE` e `SW RESET` ficam desabilitados enquanto não há conexão ativa.

---

### 5.2 Aba APLICACAO — FSM Automática

Layout em **três colunas**:

#### Coluna 1 — Pipeline FSM e Status

**Painel MAQUINA DE ESTADOS**

Exibe o estado atual da FSM em texto grande e destaca a caixa correspondente no pipeline visual:

```
IDLE  >  OBJ DET  >  CLASSIF  >  ROTA A  >  ROTA B
```

Os estados possíveis são:

| ID | Nome exibido | Cor |
|---|---|---|
| 0 | IDLE | Cinza |
| 1 | OBJETO DETECTADO | Amarelo |
| 2 | CLASSIFICANDO | Azul ciano |
| 3 | ROTA A | Verde |
| 4 | ROTA B | Laranja |
| 5 | ENTREGUE - ROTA A | Verde (transitório, 2 s) |
| 6 | ENTREGUE - ROTA B | Laranja (transitório, 2 s) |

**Painel STATUS**

Exibe em tempo real:

- **MODO** — `FSM` (azul) ou `DEBUG` (laranja), sincronizado via mensagens `0x11`/`0x22` do firmware
- **CANCELA** — `ABERTA` (verde) ou `FECHADA` (vermelho)
- **MOTOR** — `ENGAJADO` (verde) ou `LIVRE` (cinza)
- **FLASH** — `ON` (amarelo) ou `OFF` (cinza)
- **ULTIMO QR** — conteúdo e rota do último QR lido com sucesso

#### Coluna 2 — Log FSM

Log colorido com quatro categorias:

| Tag | Cor (dark) | Conteúdo |
|---|---|---|
| `[UART]` | Azul | Frames TX e RX brutos (`[0xAA][0xDA]`, `[0x90][0xC0]`) |
| `[FSM]` | Verde | Eventos da máquina de estados |
| `[QR]` | Roxo | Leituras de QR, rota extraída, resultado da classificação |
| `[AVISO]` | Amarelo | Alertas de tentativas sem leitura |

Botões na base: **SALVAR** (gera `logFSM_NNN.txt`) e **LIMPAR** (esvazia o log sem salvar).

#### Coluna 3 — Câmera em Tempo Real

- **Seletor CAM** — índice da câmera USB (0 a 9)
- **Botão INICIAR / PARAR CAMERA** — liga e desliga a captura
- **Canvas de vídeo** — exibe o frame em tempo real com contorno verde sobre QR detectado
- **Borda do canvas** — muda de cor conforme o estado:
  - Cinza escuro: câmera ativa, monitoramento passivo
  - Amarelo: modo de classificação ativo (aguardando leitura)
  - Verde: câmera inativa

A câmera é ligada automaticamente ao concluir o handshake com o STM32.

---

### 5.3 Aba DEBUG — Controle Assíncrono

Layout em **três colunas**, idêntico ao painel de debug standalone validado:

#### Coluna 1 — Status, FSM, Comandos FSM, Sensores

**Painel STATUS DO HARDWARE** — igual ao da aba APLICACAO, com campo adicional de DIRECAO do motor.

**Painel MAQUINA DE ESTADOS** — mesmo pipeline sincronizado, atualizado pelo mesmo `_handle_rx`.

**Painel COMANDOS FSM**

| Botão | Comando enviado |
|---|---|
| ROTA A | `[0xAA][0xDA]` |
| ROTA B | `[0xAA][0xDB]` |
| TOGGLE DEBUG | `[0xAA][0xDD]` |

**Painel SENSORES LASER** — quatro indicadores visuais (S1 a S4). Atualizados via telemetria `[0x90][0x55][STATUS_BYTE]` enviada pelo firmware quando em modo DEBUG. Cada bit do `STATUS_BYTE` corresponde a um sensor:

```
bit 0 → S1 (Sensor entrada)
bit 1 → S2 (Sensor câmera)
bit 2 → S3 (Sensor saída Rota A)
bit 3 → S4 (Sensor saída Rota B)
```

#### Coluna 2 — Controle Assíncrono

| Seção | Controles |
|---|---|
| FLASH | ON / OFF |
| CANCELA | ABRIR / FECHAR |
| MOTOR | ENGAJAR / LIVRE |
| DIRECAO | FRENTE / TRAS |
| PASSOS | Spinbox 1–255 + ENVIAR / MODO LOOP |
| SW RESET | Reset por software com caixa de confirmação |

**Modo Loop:** quando ativado, envia o comando `[0xAA][0xE5][N_PASSOS]` continuamente a cada 60 ms (≈ 16 cmd/s), mantendo o motor em movimento contínuo. O botão PARAR encerra o loop.

#### Coluna 3 — Log de Comunicação UART

Log colorido com as mesmas categorias da aba APLICACAO, acrescido de:

| Tag | Cor (dark) | Conteúdo |
|---|---|---|
| `[sys]` | Amarelo | Mensagens do sistema (conexão, reconexão, modo) |

Recursos adicionais do log de debug:

- **Contador de erros `ERR: N`** — incrementado a cada `[0x91]` (CMD_ERR) recebido, reseta ao limpar o log
- **Botão SALVAR LOG** — gera `logDBG_NNN.txt`
- **Botão LIMPAR LOG** — limpa o texto e reseta o contador de erros

---

## 6. Fluxo de Operação Normal (Aba APLICACAO)

```
Usuário                    GUI                         STM32
  |                          |                            |
  |-- Seleciona porta -----→ |                            |
  |-- CONECTAR -----------→  |--- (abre porta serial) --> |
  |-- HANDSHAKE ----------→  |--- [0xAA][0x10] --------→ |
  |                          |← [0x90][0x01] (SYS_INIT) -|
  |                          | Liga câmera automaticamente|
  |                          |                            |
  |            (objeto entra na esteira)                  |
  |                          |← [0x90][0xA0] OBJ_DET  ---| Sensor 1 ativado
  |                          | FSM → OBJETO DETECTADO     |
  |                          |                            |
  |            (objeto chega sob a câmera)                |
  |                          |← [0x90][0xC0] CLSS_REQ ---|
  |                          | Modo classificação ATIVO   |
  |                          | Borda canvas → AMARELO     |
  |                          | Camera tenta ler QR...     |
  |                          |                            |
  |            (QR detectado e decodificado)              |
  |                          | parse_qr() → 0xDA ou 0xDB  |
  |                          |--- [0xAA][0xDA] --------→ | ROTA A
  |                          |← [0x90][0xFA] ROUTE_A_FWD |
  |                          | Classificação encerrada    |
  |                          | Log: N tentativas          |
  |                          |                            |
  |            (objeto entregue na rota A)                |
  |                          |← [0x90][0xBA] ROUTE_A_OK  |
  |                          | FSM → ENTREGUE ROTA A      |
  |                          | Aguarda 2s → FSM → IDLE    |
```

### Comportamento das tentativas de leitura

O `Classifier` conta cada frame sem leitura válida como uma tentativa. A cada **10 tentativas consecutivas sem sucesso**, uma mensagem de aviso é registrada no log:

```
[AVISO] AVISO: 10 tentativas sem leitura do QRcode -- aguardando...
[AVISO] AVISO: 20 tentativas sem leitura do QRcode -- aguardando...
```

Ao receber `ROUTE_A_FWD` ou `ROUTE_B_FWD` do STM32 (confirmação de que a rota foi aceita), o classificador encerra e o log registra o total de tentativas realizadas na sessão:

```
[QR] Classificacao encerrada -- 3 tentativa(s)
```

O contador zera automaticamente para o próximo ciclo.

---

## 7. Protocolo de Comunicação UART

### 7.1 Estrutura dos Frames

**GUI → STM32 (TX):**

```
[0xAA] [CMD]           — frame de 2 bytes (maioria dos comandos)
[0xAA] [0xE5] [DATA]   — frame de 3 bytes (apenas STPR_TGT_STPS)
```

**STM32 → GUI (RX):**

```
[0x90] [PAYLOAD]       — resposta positiva ou mensagem espontânea
[0x91]                 — CMD_ERR (byte único, frame inválido)
[0x90] [0x55] [STATUS] — telemetria de sensores (3 bytes)
```

O parser RX implementado na GUI suporta os três formatos acima. Qualquer byte fora de frame é ignorado silenciosamente.

### 7.2 Tabela de Comandos — GUI → STM32

| Byte | Nome | Aba | Descrição |
|---|---|---|---|
| `0x10` | SYS_RDY | Ambas | Inicia handshake |
| `0xDA` | ROUTE_A | Ambas | Envia destino: Rota A |
| `0xDB` | ROUTE_B | Ambas | Envia destino: Rota B |
| `0xDD` | DEBUG_TOGGLE | DEBUG | Alterna modo FSM ↔ DEBUG |
| `0xE1` | LIGHT_EN | DEBUG | Liga flash/luminária |
| `0xD1` | LIGHT_DIS | DEBUG | Desliga flash/luminária |
| `0xE2` | GATE_OPEN | DEBUG | Abre cancela (servo) |
| `0xD2` | GATE_CLOSE | DEBUG | Fecha cancela (servo) |
| `0xE3` | STPR_EN | DEBUG | Engaja motor de passo (eixo travado) |
| `0xD3` | STPR_DIS | DEBUG | Libera motor de passo (eixo livre) |
| `0xE4` | STPR_FORWARD | DEBUG | Define sentido: frente |
| `0xD4` | STPR_BACKWARD | DEBUG | Define sentido: trás |
| `0xE5` | STPR_TGT_STPS | DEBUG | Define e executa N passos (+ DATA byte) |
| `0x33` | SW_RESET | Ambas | Reset por software do STM32 |

### 7.3 Tabela de Mensagens — STM32 → GUI

| Byte | Nome | Descrição |
|---|---|---|
| `0x01` | SYS_INIT | Completa o handshake; câmera é ligada automaticamente |
| `0xA0` | OBJ_DETECTED | Objeto passou pelo Sensor 1; FSM → estado 1 |
| `0xC0` | CLSS_REQUEST | Objeto parado sob a câmera; modo classificação iniciado |
| `0xFA` | ROUTE_A_FWD | STM32 aceitou Rota A; classificação encerrada |
| `0xFB` | ROUTE_B_FWD | STM32 aceitou Rota B; classificação encerrada |
| `0xBA` | ROUTE_A_OK | Entrega confirmada na Rota A; FSM → IDLE em 2 s |
| `0xBB` | ROUTE_B_OK | Entrega confirmada na Rota B; FSM → IDLE em 2 s |
| `0x11` | MODE_FSM | Confirma que firmware está em modo FSM |
| `0x22` | MODE_DEBUG | Confirma que firmware está em modo DEBUG |
| `0x33` | SW_RESET_ECO | Eco de confirmação do reset |
| `0x55` | SENS_STATUS | Telemetria dos 4 sensores (frame de 3 bytes, só no modo DEBUG) |
| `0x91` | CMD_ERR | Byte único; frame inválido recebido pelo STM32 |

---

## 8. Leitura e Interpretação de QR Codes

### Formato suportado (gerador do projeto)

O gerador de QR Codes (`gera_qr.py`) produz o seguinte texto dentro de cada código:

```
Nome da Peça: CAIXA_INDO_PARA_A
Destino: 0xAADA
Projeto final SOE 2026.1 - Felipe e Caleb - esteira separadora de itens
```

A função `parse_qr()` extrai a rota com duas estratégias em cascata:

**Estratégia 1 — campo `Destino:` (preferencial):**

Busca a expressão `Destino: <valor>` em qualquer posição do texto usando regex. Se encontrar o campo mas o valor não corresponder a uma rota válida (ex: `0x3D` do `CAIXA_ERRO`), retorna `None` imediatamente — sem tentar a estratégia 2. Isso evita falsos positivos de letras `DA` ou `DB` que possam aparecer em outros campos do texto.

**Estratégia 2 — tokens com prefixo `0x` (fallback):**

Usada apenas quando o campo `Destino:` não está presente. Busca tokens no formato `0xAADA`, `0xAADB`, `0xDA` ou `0xDB` em qualquer posição.

### Valores aceitos no campo Destino

| Valor no QR | Rota | Byte enviado |
|---|---|---|
| `0xAADA` | Rota A | `0xDA` |
| `0xAADB` | Rota B | `0xDB` |
| `0xDA` | Rota A | `0xDA` |
| `0xDB` | Rota B | `0xDB` |
| Qualquer outro | Inválido | Nenhum (tentativa contada) |

### Implementação sem race condition

A câmera e o processamento de QR foram projetados para evitar a condição de corrida que existia em versões anteriores. Em vez de dois callbacks separados (`on_frame` e `on_qr`) que geravam dois eventos independentes na fila do tkinter, a `CameraReader` usa um único callback `on_tick(rgb, qr_raw, qr_cmd)` que entrega frame e resultado de decodificação atomicamente. O handler `_upd_tick()` decide em sequência:

1. Atualiza o canvas com o frame
2. Se `qr_raw` não é `None` → entrega ao classificador via `feed()`
3. Caso contrário → conta tentativa vazia via `count_attempt()`

Nunca os dois ao mesmo tempo, garantindo que uma leitura válida nunca seja descartada.

---

## 9. Sistema de Log

### Regra de aba ativa

Cada aba tem seu próprio log (widget `tk.Text`) e seu próprio buffer de linhas em memória. O método `_write_log_to(tab, tag, msg)` verifica `self._active_tab` antes de escrever: se o evento pertence a uma aba que não está visível, ele é silenciado. Isso garante que:

- Enquanto o operador usa a aba DEBUG, nenhum evento polui o log da aba APLICACAO
- Ao retornar à aba APLICACAO, o log estará exatamente no estado em que foi deixado

### Salvamento ao trocar de aba

Ao clicar em qualquer tab, o método `_request_tab()` verifica se o log atual tem conteúdo. Se sim, exibe uma caixa de diálogo com três opções:

- **Sim** — salva o log e troca de aba
- **Não** — descarta o log e troca de aba
- **Cancelar** — permanece na aba atual sem nenhuma alteração

O mesmo comportamento ocorre ao fechar a janela (botão X), que verifica ambos os logs antes de encerrar.

### Arquivos de saída

Os arquivos são salvos no diretório de trabalho atual (onde o script foi executado):

```
logFSM_001.txt   ← primeiro save da aba APLICACAO na sessão
logFSM_002.txt   ← segundo save da aba APLICACAO na sessão
logDBG_001.txt   ← primeiro save da aba DEBUG na sessão
logDBG_002.txt   ← segundo save da aba DEBUG na sessão
```

O contador `NNN` (com zero à esquerda, três dígitos) reinicia em `001` a cada vez que o script é executado. Arquivos de sessões anteriores não são sobrescritos — os nomes são simplesmente incrementados.

### Formato do arquivo de log

```
# Esteira Separadora v4.0 -- Log FSM
# Salvo em: 05/05/2026 23:49:41

[23:46:12.034]  UART conectada: COM5 @ 115200  --  pressione HANDSHAKE
[23:46:14.201]  >> TX  [0xAA][0x10]  SYS_RDY        (0x10)
[23:46:14.245]  << RX  [0x90][0x01]  SYS_INIT       (0x01)
[23:46:14.246]  HANDSHAKE OK -- sistema inicializado
[23:46:20.103]  << RX  [0x90][0xA0]  OBJ_DETECTED   (0xA0)
[23:46:20.104]  Objeto detectado -- encaminhando para a camera
[23:46:22.891]  << RX  [0x90][0xC0]  CLSS_REQUEST   (0xC0)
[23:46:22.892]  Modo de classificacao ATIVO -- aguardando QRcode
[23:46:23.114]  QR LIDO: 'Nome da Peca: CAIXA_INDO_PARA_A...'
[23:46:23.115]    ROTA EXTRAIDA: A  (0xDA)
[23:46:23.116]  CLASSIFICACAO OK -> ROTA A  |  enviando 0xDA
[23:46:23.117]  >> TX  [0xAA][0xDA]  ROUTE_A        (0xDA)
[23:46:23.301]  << RX  [0x90][0xFA]  ROUTE_A_FWD    (0xFA)
[23:46:23.302]  Classificacao encerrada -- 3 tentativa(s)
```

---

## 10. Temas Visual (Claro / Escuro)

O botão `*` no canto superior direito alterna entre os dois temas em tempo real, sem reiniciar a aplicação. A troca recolore todos os widgets registrados, incluindo os logs (mantendo as tags de cor por categoria), os pipelines FSM, os indicadores de sensor e o canvas da câmera.

### Paleta tema escuro

| Elemento | Cor |
|---|---|
| Fundo principal | `#0D0F14` |
| Painel | `#13161E` |
| Borda de painel | `#1E2330` |
| Accent (ciano) | `#00D4FF` |
| Accent2 (laranja) | `#FF6B35` |
| Verde | `#00D97A` |
| Vermelho | `#FF3B5C` |
| Amarelo | `#FFD700` |
| Texto normal | `#C8D0E0` |
| Texto atenuado | `#4A5568` |
| Log UART (TX) | `#38BDF8` |
| Log FSM (RX) | `#34D399` |
| Log sistema | `#FBBF24` |
| Log QR | `#C084FC` |

---

## 11. Máquina de Estados (FSM)

O pipeline visual está presente em **ambas as abas** e é atualizado pelo mesmo método `_refresh_fsm_panels(sid)` toda vez que um evento relevante chega via UART. Isso garante que, independente de qual aba o operador esteja usando, o estado da esteira está sempre visível e atualizado.

### Transições de estado

```
                    OBJ_DETECTED (0xA0)
IDLE (0) ─────────────────────────────→ OBJETO DETECTADO (1)
                                                │
                    CLSS_REQUEST (0xC0)         │
                ┌───────────────────────────────┘
                ↓
        CLASSIFICANDO (2)  ←── câmera lê QR e envia 0xDA ou 0xDB
                │
                │   ROUTE_A_FWD (0xFA)
                ├──────────────────────→ ROTA A (3)
                │                              │ ROUTE_A_OK (0xBA)
                │                              └──→ ENTREGUE ROTA A (5) → IDLE (0)
                │
                │   ROUTE_B_FWD (0xFB)
                └──────────────────────→ ROTA B (4)
                                               │ ROUTE_B_OK (0xBB)
                                               └──→ ENTREGUE ROTA B (6) → IDLE (0)
```

Os estados ENTREGUE (5 e 6) são transitórios: após 2 segundos, a GUI retorna automaticamente ao estado IDLE, pronta para o próximo ciclo.

---

## 12. Arquitetura do Código

O script é composto pelas seguintes classes e funções globais:

### Funções globais

| Função | Descrição |
|---|---|
| `now_ts()` | Retorna timestamp formatado `HH:MM:SS.mmm` |
| `parse_qr(raw)` | Extrai byte de rota de qualquer texto de QR do projeto |

### Classes

#### `SerialManager`

Instância única compartilhada. Gerencia a comunicação UART com threads dedicadas para TX e RX, eliminando bloqueios na thread principal do tkinter.

- **TX:** fila `queue.Queue()` — comandos são enfileirados e transmitidos pela thread `ser-tx`
- **RX:** parser de estados (`IDLE → WAIT_CMD → WAIT_DATA`) suportando frames de 2 e 3 bytes
- **Callbacks:** notifica a GUI via `_notify(event, data)`, que usa `self.after(0, ...)` para garantir execução na thread do tkinter
- **Reconexão:** armazena `_last_port` e `_last_baud` para uso pelo `_reconnect_loop`

#### `CameraReader`

Captura frames de câmera USB em thread própria (`cam`). Usa `cv2.QRCodeDetector` para decodificação e entrega frame + resultado QR em um único callback `on_tick(rgb, qr_raw, qr_cmd)`, eliminando a condição de corrida presente em versões anteriores.

#### `Classifier`

Gerencia o ciclo de classificação ativado por `CLSS_REQUEST`. Mantém contagem de tentativas, dispara avisos a cada 10 tentativas sem leitura e notifica a GUI via `on_classified(raw, cmd)` ao obter uma leitura válida. Encerra-se ao receber confirmação via `ROUTE_A_FWD` ou `ROUTE_B_FWD`.

#### `EsteiraApp`

Classe principal (herda de `tk.Tk`). Responsável por toda a interface gráfica. Principais responsabilidades:

| Método | Função |
|---|---|
| `_build_topbar()` | Barra superior compartilhada |
| `_request_tab(target)` | Troca de aba com verificação de log |
| `_save_log(tab)` | Serializa buffer de log para arquivo txt |
| `_build_page_fsm()` | Constrói a aba APLICACAO |
| `_build_page_dbg()` | Constrói a aba DEBUG |
| `_build_fsm_pipeline_panel(parent, tag)` | Reutilizado nas duas abas |
| `_upd_tick(rgb, qr_raw, qr_cmd)` | Handler único por frame de câmera |
| `_write_log_to(tab, tag, msg)` | Roteador de log com verificação de aba ativa |
| `_refresh_fsm_panels(sid)` | Atualiza pipeline nas duas abas simultaneamente |
| `_handle_rx(data)` | Processa todos os eventos UART recebidos |
| `_toggle_theme()` | Alterna tema e recolore todos os widgets |
| `_recolor_all()` | Percorre `_tw_list` e `_btn_list` para recoloração |

---

## 13. Comportamento de Reconexão Automática

Ao detectar queda de conexão (exceção na thread RX ou desconexão física), a GUI:

1. Atualiza o LED para vermelho e exibe `CONEXAO PERDIDA`
2. Registra o evento no log da aba ativa
3. Inicia `_reconnect_loop()` em thread daemon separada
4. Tenta reconectar a cada **3 segundos** na mesma porta e baud
5. Ao reconectar, atualiza a barra de conexão e registra o evento no log

A reconexão automática é desativada (`_auto_reconnect = False`) apenas quando o usuário clica em `DESCONECTAR` explicitamente ou fecha a janela, evitando tentativas desnecessárias após desconexão intencional.

---

## 14. Formato dos QR Codes do Projeto

O gerador de QR Codes (`gera_qr.py`) lê o arquivo `pecas.json` e produz uma imagem `.jpg` por entrada. O texto codificado em cada QR segue o padrão:

```
Nome da Peça: <nome_peca>
Destino: <destino_hex>
Projeto final SOE 2026.1 - Felipe e Caleb - esteira separadora de itens
```

### Exemplo de JSON de entrada

```json
[
  { "nome_peca": "CAIXA_INDO_PARA_A",  "destino": "0xAADA" },
  { "nome_peca": "CAIXA_INDO_PARA_B",  "destino": "0xAADB" },
  { "nome_peca": "CAIXA_DESTINO_A",    "destino": "0xDA"   },
  { "nome_peca": "CAIXA_DESTINO_B",    "destino": "0xDB"   },
  { "nome_peca": "CAIXA_ERRO",         "destino": "0x3D"   }
]
```

`CAIXA_ERRO` tem destino `0x3D`, que não é rota válida (`0xDA` ou `0xDB`). A GUI reconhece e trata esse caso: `parse_qr()` encontra o campo `Destino: 0x3D`, constata que não é uma rota válida e retorna `None` — a tentativa é contada e o ciclo continua aguardando uma nova leitura.

---

## 15. Solução de Problemas

### A câmera não abre

- Verifique o índice no seletor **CAM** (0, 1, 2...) — câmeras adicionais incrementam o índice
- Confirme que `opencv-python` e `Pillow` estão instalados: `pip list | grep -E "opencv|Pillow"`
- No Linux, verifique permissão: `ls -l /dev/video0` e `sudo usermod -aG video $USER`

### O QR é detectado (contorno verde aparece) mas a rota não é reconhecida

- Verifique se o campo `Destino:` está presente no texto do QR — leia o QR com o celular para confirmar o conteúdo exato
- Confirme que o valor de destino é `0xAADA`, `0xAADB`, `0xDA` ou `0xDB`
- Valores como `0x3D` são intencionalmente ignorados (QR de erro)

### CMD_ERR aparece no log (contador ERR sobe)

- O STM32 recebeu um frame inválido — verifique se o baud rate está correto (115200)
- Verifique o cabo e conexão física
- Execute o handshake novamente

### A GUI não lista portas seriais

- No Windows: verifique o Gerenciador de Dispositivos
- No Linux: confirme com `ls /dev/ttyUSB* /dev/ttyACM*` e adicione o usuário ao grupo `dialout`: `sudo usermod -aG dialout $USER` (requer logout)

### O motor não responde aos comandos de passo

- Confirme que o STM32 está em **modo DEBUG** (o painel STATUS deve mostrar `DEBUG`)
- Use o botão `TOGGLE DEBUG` para alternar o modo
- O motor precisa estar ENGAJADO antes de receber passos

### O log fica em branco mesmo com eventos UART chegando

- Verifique se a aba correta está ativa — eventos só alimentam o log da aba visível no momento
- Isso é comportamento esperado: mude para a aba desejada antes de iniciar a operação

---

## 16. Histórico de Versões

| Versão | Descrição |
|---|---|
| v1.0 (Felipe) | Primeira GUI com câmera, duas abas, QR integrado (bugs na lógica de roteamento) |
| v2.0 | Correção dos bugs de lógica QR: `_clear_qr()` movido para `ROUTE_OK`, `_waiting_cls` corrigido |
| v3.0 (Caleb) | GUI de debug standalone: reconexão automática, sensores, SW RESET, exportação de log, indicador de atividade, tema claro/escuro |
| v3.1 | Fusão experimental das duas GUIs (descontinuada — escopo muito amplo) |
| v4.0 | **Versão atual.** Fusão definitiva: duas abas com log independente e condicional, `SerialManager` único com parser 3 bytes, `CameraReader` com callback atômico `on_tick()` (elimina race condition), `parse_qr()` com suporte ao formato real do gerador do projeto, salvamento de log ao trocar de aba ou fechar, contador de saves por sessão (`logFSM_NNN` / `logDBG_NNN`) |
