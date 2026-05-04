# Relatório Parcial — Esteira Separadora (Firmware v2.1)
**Sistemas Operacionais Embarcados (SOE) — 2026.1**
**Autores:** Paulo Caleb Fernandes da Silva, Felipe de Castro

---

## 1. Descrição do Hardware Utilizado

O sistema é composto por uma arquitetura híbrida que divide as responsabilidades entre um microprocessador de alto nível e um microcontrolador dedicado ao controle dos dispositivos físicos da esteira.

**Raspberry Pi 3B** atua como o cérebro central do sistema, sendo responsável por executar os algoritmos de visão computacional e classificação de objetos. Uma webcam USB conectada ao Raspberry Pi captura as imagens dos objetos sobre a esteira, permitindo que o processamento de alto nível determine o destino de cada item. As decisões de roteamento são então transmitidas ao microcontrolador via comunicação serial UART.

**STM32G070** é o microcontrolador responsável pelo controle direto dos atuadores e pela leitura dos sensores. Opera a uma frequência de CPU de 64 MHz e gerencia os seguintes periféricos:

- **Timer 15 (TIM15):** Gera o sinal PWM de frequência fixa (200 Hz, duty cycle de 50%) utilizado para modular os lasers dos sensores.
- **Timer 3 (TIM3):** Configurado em modo Input Capture com DMA, realiza a medição de frequência dos quatro sensores laser simultaneamente.
- **Timer 16 (TIM16):** Gera o sinal PWM de 50 Hz com duty cycle variável para o controle do servo motor.
- **Timer 6 (TIM6):** Opera a 1 MHz e dispara interrupções periódicas para o controle preciso dos pulsos enviados ao driver do motor de passo.
- **USART2:** Configurada a 115200 bps, 8N1, para comunicação com o Raspberry Pi 3B.

**Sensores laser** funcionam a partir de um princípio de detecção síncrona por ausência de portadora. Um laser modula seu feixe na mesma frequência gerada pelo TIM15. O feixe incide sobre um LDR que, após condicionamento de sinal, entrega ao STM32 um sinal digital pulsado na faixa de 0 a 3,3 V com frequência idêntica à portadora (200 Hz). Quando um objeto interrompe o feixe, a ausência do sinal pulsado é interpretada pelo firmware como presença de objeto.

**Motor de passo com driver A4988:** O tapete da esteira é acionado por um motor de passo controlado pelo driver A4988. Os pinos STEP, DIR e SLEEP são utilizados respectivamente para: avançar um passo, definir a direção de rotação e habilitar/desabilitar o driver (liberando ou travando o eixo).

**Servo motor ES08MA:** Responsável pelo acionamento mecânico da cancela que desvia os objetos para a Rota B. É controlado por PWM de 50 Hz, com largura de pulso variando entre 600 µs (0°) e 2300 µs (180°), mapeada diretamente a partir do ângulo desejado pelo firmware.

**Flash (luminária):** Controlado diretamente por uma GPIO dedicada do STM32G070, sendo acionado durante o estado de classificação para iluminar o objeto e auxiliar a câmera do Raspberry Pi na captura de imagem.

---

## 2. Descrição do Software Desenvolvido

O firmware v2.1 foi desenvolvido em linguagem C utilizando os drivers HAL da STMicroelectronics para a família STM32, com o STM32CubeIDE como ambiente de desenvolvimento. Esta versão introduz melhorias significativas em relação à versão anterior, com destaque para o parser de quadros UART, a função de transmissão padronizada e o modo de operação de depuração.

### 2.1 Modos de Operação

O firmware v2.1 introduz um sistema de dois modos de operação mutuamente exclusivos, controlados pela variável `operationMode`:

| Modo | Enumerador | Descrição |
|---|---|---|
| Normal | `OP_MODE_FSM` | FSM completa em execução. Modo padrão ao ligar. |
| Depuração | `OP_MODE_DEBUG` | FSM suspensa. Apenas comandos assíncronos são processados. |

A alternância entre os modos é realizada pelo comando `DEBUG_MODE_TOGGLE_MSG (0xDD)`, processado pela função `handleModeToggle()`, que é sempre chamada primeiro no loop principal, independentemente do modo ativo. Ao entrar no modo de depuração, o motor é parado com o eixo travado, o flash é apagado e o LED de depuração pisca 6 vezes rapidamente como sinalização visual. Ao retornar ao modo normal, a FSM é reiniciada a partir de `STATE_IDLE` e o LED realiza um pulso longo de 400 ms.

### 2.2 Parser de Quadros UART

Uma das principais mudanças da v2.1 é a substituição do recebimento de bytes avulsos por um parser de quadros estruturado, implementado diretamente na callback de interrupção `HAL_UART_RxCpltCallback`. O parser opera como uma máquina de estados interna com três fases:

| Estado do Parser | Enumerador | Descrição |
|---|---|---|
| Aguarda início | `RX_WAIT_START` | Descarta bytes até receber `0xAA` |
| Aguarda comando | `RX_WAIT_CMD` | Armazena o byte de CMD. Se for `0xE5`, avança para aguardar DATA; caso contrário, frame de 2 bytes é concluído. |
| Aguarda dado | `RX_WAIT_DATA` | Armazena o byte de DATA e conclui o frame de 3 bytes. |

Quando um frame é concluído, os campos `rxCMD` e `rxDATA` são preenchidos e a flag `rxFrameReady` é setada. O loop principal consome essa flag ao processar o comando, evitando condições de corrida. Bytes recebidos fora de um frame válido são descartados silenciosamente.

### 2.3 Transmissão Padronizada

A v2.1 introduz a função `sendFrame()`, que centraliza toda a transmissão UART e padroniza o formato das respostas:

- **Resposta positiva:** `[CMD_OK 0x90][payload]` — enviada como confirmação de qualquer comando recebido com sucesso, com eco do byte de comando como payload.
- **Resposta negativa:** `[CMD_ERR 0x91]` — enviada como byte único quando o comando recebido não é reconhecido.

Essa abordagem substitui as chamadas diretas a `HAL_UART_Transmit` espalhadas pelo código da versão anterior, tornando o protocolo mais robusto e o código mais legível.

### 2.4 Protocolo de Comunicação UART

O protocolo proprietário foi mantido e expandido na v2.1. O quadro de comunicação é organizado da seguinte forma:

**Estrutura do quadro:**

| Direção | Formato |
|---|---|
| Recepção (RX) | `[START 0xAA][CMD][DATA]` |
| Transmissão positiva (TX) | `[CMD_OK 0x90][CMD_RECEBIDO]` |
| Transmissão negativa (TX) | `[CMD_ERR 0x91]` |

**Handshake de inicialização:**

| Byte | Valor | Direção | Descrição |
|---|---|---|---|
| `SYS_RDY_MSG` | `0x10` | RX | Raspberry Pi sinaliza que está pronto |
| `SYS_INIT_MSG` | `0x01` | TX (payload) | STM32 confirma inicialização via `sendFrame(CMD_OK, SYS_INIT_MSG)` |

**Mensagens da FSM:**

| Byte | Valor | Direção | Descrição |
|---|---|---|---|
| `OBJ_DETECTED_MSG` | `0xA0` | TX | Objeto detectado na esteira |
| `CLSS_REQUEST_MSG` | `0xC0` | TX | Solicitação de classificação ao Raspberry Pi |
| `ROUTE_A_RECEIVE_MSG` | `0xDA` | RX | Raspberry Pi ordena roteamento para Rota A |
| `ROUTE_B_RECEIVE_MSG` | `0xDB` | RX | Raspberry Pi ordena roteamento para Rota B |
| `ROUTE_A_FWRDNG_MSG` | `0xFA` | TX | Confirmação de encaminhamento para Rota A |
| `ROUTE_B_FWRDNG_MSG` | `0xFB` | TX | Confirmação de encaminhamento para Rota B |
| `ROUTE_A_SCCSS_DLVRY_MSG` | `0xBA` | TX | Entrega concluída na Rota A |
| `ROUTE_B_SCCSS_DLVRY_MSG` | `0xBB` | TX | Entrega concluída na Rota B |

**Mensagens de controle assíncrono (RX) — disponíveis exclusivamente no modo de depuração:**

| Byte | Valor | Descrição |
|---|---|---|
| `LIGHT_EN_MSG` | `0xE1` | Ativa a luminária |
| `LIGHT_DISABLE_MSG` | `0xD1` | Desativa a luminária |
| `GATE_OPEN_MSG` | `0xE2` | Abre a cancela |
| `GATE_CLOSE_MSG` | `0xD2` | Fecha a cancela |
| `STPR_EN_MSG` | `0xE3` | Ativa o motor de passo (eixo travado) |
| `STPR_DISABLE_MSG` | `0xD3` | Desativa o motor de passo (eixo livre) |
| `SET_STPR_FORWARD_MSG` | `0xE4` | Define rotação do motor para frente |
| `SET_STPR_BACKWARD_MSG` | `0xD4` | Define rotação do motor para trás |
| `SET_STPR_TGT_STPS_MSG` | `0xE5` | Define quantidade de passos alvo (frame de 3 bytes: `[0xAA][0xE5][valor]`) |
| `DEBUG_MODE_TOGGLE_MSG` | `0xDD` | Alterna entre modo normal e modo de depuração (processado em qualquer modo) |

### 2.5 Máquina de Estados Finitos (FSM)

A FSM mantém os cinco estados da versão anterior, com a adição do estado sentinela `STATE_NONE (255)` utilizado como valor inicial de `last_state`, garantindo que a *entry action* do `STATE_IDLE` seja executada obrigatoriamente na primeira iteração do loop principal.

| Estado | Descrição |
|---|---|
| `STATE_IDLE` | Estado de repouso. Motor em movimento, flash desligado, cancela fechada (0°). Aguarda detecção no Sensor 1. |
| `STATE_OBJECT_DETECTED` | Objeto detectado. Motor continua. Aguarda o objeto alcançar o Sensor 2 (ponto de parada). |
| `STATE_WAIT_CLASSIFICATION` | Motor parado (eixo travado), flash acionado. Aguarda comando de roteamento via UART, consumindo `rxFrameReady`. |
| `STATE_ROUTE_A` | Cancela fechada (0°). Motor em movimento. Aguarda confirmação de entrega no Sensor 3. |
| `STATE_ROUTE_B` | Cancela aberta (45°). Motor em movimento. Aguarda confirmação de entrega no Sensor 4. |

Na v2.1, o estado `STATE_WAIT_CLASSIFICATION` passou a verificar diretamente a flag `rxFrameReady` e a confirmar o recebimento do comando via `sendFrame()` antes de avançar o estado, tornando o handshake de roteamento explícito e rastreável pelo Raspberry Pi.

### 2.6 Loop Principal

O loop principal da v2.1 foi reestruturado para refletir a separação entre os modos de operação:

```
loop:
    getSensorsReadings()       ← leitura periódica dos sensores (a cada 10 ms)
    handleModeToggle()         ← verifica 0xDD em qualquer modo
    if OP_MODE_DEBUG:
        handleAsyncCommands()  ← processa comandos manuais
    else:
        stepperDirInst = 0     ← garante direção correta da FSM
        FSM()                  ← executa a máquina de estados
    [envio de mensagens TX pendentes via flags]
```

### 2.7 Rotina de Inicialização

Antes de entrar no loop principal, o firmware bloqueia a execução até duas condições serem satisfeitas simultaneamente: todos os quatro sensores laser devem estar operando na frequência esperada de 200 Hz, e o Raspberry Pi deve enviar o frame de handshake com `SYS_RDY_MSG (0x10)`. Na v2.1, a verificação do handshake passa pelo parser de frames (`rxFrameReady && rxCMD == SYS_RDY_MSG`), e a resposta é enviada via `sendFrame(CMD_OK_MSG, SYS_INIT_MSG)`, alinhada ao protocolo padronizado. O LED de depuração pisca em alta frequência (100 ms) enquanto os sensores não estiverem prontos, e em baixa frequência (500 ms) quando aguarda apenas o handshake.

### 2.8 Controle do Motor de Passo

O controle do motor de passo é realizado inteiramente por interrupção do TIM6. A cada disparo, a ISR verifica se há passos pendentes em `targetStepps` e realiza um toggle no pino STEP, gerando os pulsos necessários ao driver A4988. A velocidade do motor é ajustada modificando o registrador ARR do TIM6 através da função `stepperSetSpeed()`, que limita a operação entre `STEPPER_MIN_VEL` (300 passos/s) e `STEPPER_MAX_VEL` (420 passos/s).

---

## 3. Resultados Obtidos nos Testes

Os testes foram realizados com o hardware físico da esteira, validando individualmente cada subsistema do projeto.

**Sensores laser:** Os quatro sensores operam conforme especificado. A lógica de detecção síncrona por ausência de portadora foi validada com sucesso — ao interromper o feixe, as flags internas (`SENS1_flag` a `SENS4_flag`) são acionadas corretamente, permitindo as transições de estado da FSM.

**Flash (luminária):** O controle via GPIO dedicada funciona corretamente. O flash é ativado e desativado em resposta às transições de estado da FSM e aos comandos assíncronos recebidos via UART no modo de depuração, sem instabilidades observadas.

**Motor de passo:** O motor opera de forma estável em frequências mais baixas de acionamento. Os testes indicaram que o controle de velocidade por alteração do ARR do TIM6 funciona, porém com uma faixa operacional limitada — comportamento esperado dadas as características físicas do motor de passo (torque versus velocidade). Em razão disso, a velocidade de operação será mantida fixa no valor mínimo configurado (`STEPPER_MIN_VEL` = 300 passos/s), que corresponde a um período de interrupção do TIM6 de aproximadamente 1,67 ms, calculado com base na configuração de prescaler de 63 e clock de 64 MHz.

**Servo motor:** O servo motor ES08MA responde corretamente a qualquer valor de angulação dentro da faixa suportada (0° a 180°). Para a aplicação na esteira, os ângulos definidos no firmware — 0° para cancela fechada (Rota A) e 45° para cancela aberta (Rota B) — foram validados e apresentaram o posicionamento mecânico desejado.

**Integração com RBPi3:** O sistema já é capaz de operar em conjunto com o Raspberry pi 3B pelo canal de comunicação UART, e o raspberry pi 3B é capaz de controlar a esteria de forma assíncrona ou no modo automático, utilizando a FSM desenvolvida para a aplicação.