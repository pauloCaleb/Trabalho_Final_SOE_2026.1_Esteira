# Esteira Separadora — Guia de Utilização

Sistema de controle em C para **Raspberry Pi 3B**, responsável pela comunicação
com o firmware STM32G070 que governa a esteira separadora via UART.

---

## Requisitos de Hardware

| Componente | Detalhe |
|---|---|
| Raspberry Pi 3B | UART primária habilitada (`/dev/ttyS0`) |
| STM32G070 | Conectado aos pinos GPIO 14 (TX) e 15 (RX) do RPi |
| Câmera USB | Compatível com V4L2, índice 0 = `/dev/video0` |
| Nível lógico | STM32 opera em 3,3 V — mesmo nível do RPi, sem conversor |

> **Atenção:** Desabilite o console serial do RPi antes de usar.
> Caso contrário o Linux usará `/dev/ttyS0` como TTY de console e o
> programa receberá lixo na UART.

---

## Configuração da UART no Raspberry Pi

1. Execute `sudo raspi-config`
2. Acesse **Interface Options → Serial Port**
3. Responda **Não** para "login shell over serial"
4. Responda **Sim** para "serial port hardware enabled"
5. Reinicie o RPi

Verifique que o dispositivo existe:

```bash
ls -l /dev/ttyS0
```

---

## Dependências

```bash
sudo apt update
sudo apt install libopencv-dev libzbar-dev libgpiod-dev
```

---

## Compilação

```bash
make
```

Binário gerado: `./esteira`

Para limpar os objetos:

```bash
make clean
```

---

## Uso

```
./esteira [opções]

  -p <porta>    Porta serial     (padrão: /dev/ttyS0)
  -b <baud>     Baud rate        (padrão: 115200)
  -c <índice>   Índice da câmera (padrão: 0)
  -t <ms>       Timeout QR em ms (padrão: 30000 | 0 = sem limite)
  -m <1|2|3>    Modo direto: 1=FSM, 2=DEBUG, 3=HMI (pula menu interativo)
  -h            Exibe esta ajuda
```

### Exemplos

```bash
# Inicialização padrão — exibe menu de seleção de modo
./esteira

# Modo FSM direto, câmera índice 1, timeout de 20 s
./esteira -m 1 -c 1 -t 20000

# Modo DEBUG direto
./esteira -m 2

# Modo HMI direto (display LCD + botões START/STOP)
./esteira -m 3

# Baud rate diferente (altere também no firmware STM32)
./esteira -b 9600
```

Atalhos do Makefile:

```bash
make run        # compila e abre menu de seleção
make run-fsm    # compila e inicia direto no modo FSM
make run-dbg    # compila e inicia direto no modo DEBUG
make run-hmi    # compila e inicia direto no modo HMI
```

---

## Modos de Operação

### Modo FSM (1) — Operação Autônoma

O programa aguarda eventos do STM32 e responde automaticamente:

```
STM32 → OBJ_DETECTED   Objeto entrou na esteira
STM32 → CLSS_REQUEST   Objeto chegou sob a câmera
RPi   → lê QR code     Classifica destino (Rota A ou B)
RPi   → ROUTE_A_SEND   Envia rota ao STM32
STM32 → ROUTE_A_OK     Confirma entrega — ciclo reinicia
```

Encerre com **Ctrl+C**.

### Modo DEBUG (2) — Controle Manual

Menu interativo no terminal para acionar cada atuador individualmente.
Útil para testes de bancada.

| Tecla | Ação |
|---|---|
| `1` | Flash ON |
| `2` | Flash OFF |
| `3` | Cancela abrir |
| `4` | Cancela fechar |
| `5` | Motor engajar |
| `6` | Motor livre |
| `7` | Direção frente |
| `8` | Direção trás |
| `9` | Enviar N passos |
| `t` | Toggle DEBUG/FSM no STM32 |
| `r` | SW Reset STM32 (pede confirmação) |
| `q` | Sair |

### Modo HMI (3) — Display LCD + Botões START/STOP + Buzzer

Interface física para operação sem terminal: um display LCD 2004A
(I2C) mostra o status da esteira e os contadores de objetos entregues
em cada rota; um botão START e um botão STOP comandam a operação; um
buzzer reforça os alertas visuais do display.

```
[Tela de espera]  "Aperte START"  (mostra contadores A/B)
        │
        ▼ START pressionado
  Inicializa a câmera
        │
        ▼ sucesso
  Handshake com o STM32
        │
        ▼ sucesso
[Operação autônoma — mesma máquina de estados do Modo FSM,
 status + contadores no LCD]
        │
        │  START pressionado durante a operação → zera contadores A/B
        │
        │  falha na leitura do QR → alerta "QR nao identificado /
        │  Libere a esteira!" (backlight + buzzer piscando, sem
        │  timeout) até o operador confirmar com START
        │       │
        │       ▼ START (confirma o alerta)
        │  Reset do STM32 + reenvio do handshake, aguardado sem
        │  timeout fixo (backlight + buzzer continuam piscando)
        │  até o STM32 responder espontaneamente quando os sensores
        │  da esteira ficarem livres
        │       │
        │       ▼ handshake concluído
        │  Retoma a operação autônoma, contadores preservados
        │
        ▼ STOP pressionado (em qualquer estado da operação, inclusive
          durante os alertas acima)
  SW Reset no STM32 + volta para [Tela de espera]
```

O handshake **não** ocorre ao entrar no modo HMI — só é disparado ao
pressionar START, permitindo que o sistema fique ocioso por tempo
indefinido na tela de espera. Ao pressionar START, a câmera é
inicializada primeiro; o handshake com o STM32 só é disparado depois
dela estar pronta, garantindo que a operação autônoma só comece com
todo o hardware necessário já inicializado.

O display mostra, tanto na tela de espera quanto durante a operação,
os contadores de objetos entregues em cada rota (`A:<n> B:<n>`). Eles
persistem entre ciclos e só são zerados ao pressionar **START enquanto
o sistema está em operação**, sem afetar o ciclo em andamento. O botão
**STOP** mantém uma única função: encerrar a operação e voltar à tela
de espera, a partir de qualquer estado.

Se a leitura do QR falhar (timeout ou erro), o display mostra o aviso
"QR nao identificado" e instrui o operador a liberar a esteira; o
backlight e o buzzer piscam em sincronia, sem prazo definido, até o
operador confirmar com START. A confirmação reseta o STM32 e reenvia
o handshake; como o firmware do STM32 só responde ao handshake de
forma espontânea quando os sensores da esteira estão livres, o
Raspberry Pi aguarda essa resposta sem impor um timeout fixo — o
display permanece piscando durante toda a espera, e STOP cancela a
qualquer momento. Se o STM32 responder com erro de protocolo nesse
processo, o programa retorna à tela de espera.

Hardware esperado (ajustável em `hmi.h`):

| Componente | Padrão | Define em hmi.h |
|---|---|---|
| Display LCD 2004A (PCF8574) | `/dev/i2c-1`, endereço `0x27` | `HMI_I2C_DEVICE`, `HMI_LCD_I2C_ADDR` |
| Botão START | `gpiochip0`, linha 17 (GPIO17) | `HMI_BTN_START_LINE` |
| Botão STOP | `gpiochip0`, linha 27 (GPIO27) | `HMI_BTN_STOP_LINE` |
| Buzzer (saída) | `gpiochip0`, linha 23 (GPIO23) | `HMI_BUZZER_LINE` |
| Debounce (software) | 30 ms | `HMI_BTN_DEBOUNCE_MS` |

Os botões usam pull-up **externo** (nível lógico 0 = pressionado); o
GPIO do RPi é configurado sem bias interno para não competir com o
pull-up do circuito. O buzzer é uma saída digital simples (nível 1 =
ligado); se a linha não abrir, o modo HMI segue funcionando
normalmente e os alertas ficam apenas com o backlight piscando.

Ctrl+C encerra o modo HMI a partir de qualquer estado (inclusive
durante os alertas) e exibe no display a mensagem final:

```
PROGRAMA ENCERRADO
Reinicie pelo
terminal
```

Como o controlador HD44780 retém o conteúdo exibido sem energização
contínua, essa mensagem permanece visível no display mesmo após o
processo ser encerrado.

---

## Formato do QR Code

O QR deve conter o campo `Destino:` com o byte de rota em hexadecimal:

```
Nome da Peca: Parafuso M6
Destino: 0xAADA
```

`0xDA` → Rota A  
`0xDB` → Rota B  

O parser também aceita tokens isolados `0xDA` / `0xDB` em qualquer
posição do texto do QR.

---

## Saída no Terminal

Todas as mensagens seguem o formato:

```
HH:MM:SS.mmm  [CATEGORIA]  mensagem
```

| Categoria | Cor | Conteúdo |
|---|---|---|
| `[UART TX]` | Ciano | Frames enviados ao STM32 com bytes hex |
| `[UART RX]` | Verde | Frames recebidos do STM32 com bytes hex |
| `[FSM    ]` | Azul | Transições de estado e pipeline visual |
| `[CAM    ]` | Magenta | Eventos de câmera e leitura de QR |
| `[DBG    ]` | Amarelo | Ações do modo debug |
| `[SYS    ]` | Branco | Sistema: serial, threads, handshake |
| `[ERR    ]` | Vermelho | Erros de protocolo ou hardware |
