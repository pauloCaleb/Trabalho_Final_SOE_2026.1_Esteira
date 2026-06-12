# Configuração do PWM do Flash — Esteira Separadora v3.0

## Visão geral

O flash da esteira é controlado pelo **TIM17 Canal 1** do STM32G070, gerando um PWM cuja frequência é alta o suficiente para eliminar flicker visível, enquanto o duty cycle varia continuamente para produzir o efeito de fade-in/fade-out durante o estado `STATE_WAIT_CLASSIFICATION`.

---

## 1. Clock base do STM32G070

O microcontrolador opera a **64 MHz**, configurado via PLL a partir do oscilador interno HSI de 16 MHz:

```
HSI = 16 MHz
PLLM = 1  →  VCO input  = 16 / 1 = 16 MHz
PLLN = 8  →  VCO output = 16 × 8 = 128 MHz
PLLR = 2  →  SYSCLK     = 128 / 2 = 64 MHz
```

Todos os timers do APB1 (incluindo TIM17) recebem o clock diretamente:

```
f_TIM17 = 64 MHz
```

---

## 2. Clock do timer após prescaler

O prescaler divide o clock do timer antes que ele alimente o contador. O valor configurado no registrador é **PSC = 63**, mas o divisor efetivo é PSC + 1:

```
Divisor = PSC + 1 = 63 + 1 = 64

f_contador = f_TIM17 / Divisor
f_contador = 64.000.000 / 64 = 1.000.000 Hz = 1 MHz
```

Cada tick do contador corresponde a **1 µs**.

---

## 3. Frequência do PWM

A frequência do PWM é determinada pelo período do contador, definido pelo registrador ARR (Auto-Reload Register). O contador conta de 0 até ARR e reinicia, logo o período tem ARR + 1 ticks:

```
f_PWM = f_contador / (ARR + 1)

ARR configurado = 999

f_PWM = 1.000.000 / (999 + 1)
f_PWM = 1.000.000 / 1000
f_PWM = 1000 Hz = 1 kHz
```

**Por que 1 kHz?** O limiar de fusão de flicker do olho humano está entre 50–100 Hz dependendo do brilho e da pessoa. A 50 Hz (configuração anterior) parte dos usuários ainda percebe o piscar. A 1 kHz o PWM é completamente imperceptível — o led parece uma fonte de luz contínua com brilho variável.

---

## 4. Resolução do duty cycle

Com ARR = 999, o CCR (Compare/Capture Register) pode assumir valores inteiros de 0 a 999, dando **1000 passos de resolução**:

```
Resolução = 1 / (ARR + 1) = 1 / 1000 = 0,1% por passo

Duty cycle = CCR / (ARR + 1) × 100%
```

Exemplos práticos:

| CCR  | Duty cycle |
|------|------------|
| 0    | 0%         |
| 50   | 5%         |
| 500  | 50%        |
| 950  | 95%        |
| 999  | 99,9%      |

---

## 5. Limites do sweep

Os defines `FLASH_SWEEP_MIN` e `FLASH_SWEEP_MAX` estabelecem o piso e o teto do fade, evitando que o flash apague completamente ou fique em brilho máximo constante:

```c
#define FLASH_SWEEP_MIN  50    // duty mínimo = 50/1000 = 5%
#define FLASH_SWEEP_MAX  950   // duty máximo = 950/1000 = 95%
```

O piso de 5% garante que o flash nunca apague completamente durante a classificação (a câmera sempre tem alguma iluminação). O teto de 95% evita saturação da câmera no pico.

---

## 6. Velocidade e cadência do sweep

O sweep triangular avança `FLASH_SWEEP_STEP` unidades de CCR a cada `FLASH_SWEEP_PERIOD` milissegundos:

```c
#define FLASH_SWEEP_STEP    5    // passo por intervalo (em unidades de CCR)
#define FLASH_SWEEP_PERIOD  20   // intervalo entre passos (em ms)
```

### Cálculo do tempo de um ciclo completo (min → max → min)

```
Excursão total = FLASH_SWEEP_MAX - FLASH_SWEEP_MIN
               = 950 - 50 = 900 unidades de CCR

Passos por semiciclo = Excursão / STEP = 900 / 5 = 180 passos

Tempo por semiciclo = 180 × PERIOD = 180 × 20 ms = 3600 ms

Tempo de ciclo completo = 2 × 3600 ms = 7200 ms ≈ 7,2 segundos
```

Um ciclo completo de fade-in seguido de fade-out leva aproximadamente **7,2 segundos**, produzindo o efeito de "respiração" lenta semelhante ao standby de dispositivos Apple.

### Tabela de cadências possíveis

Para ajuste futuro, mantendo `STEP = 5`:

| PERIOD (ms) | Ciclo completo |
|-------------|----------------|
| 10          | ~3,6 s         |
| 20          | ~7,2 s (atual) |
| 30          | ~10,8 s        |
| 40          | ~14,4 s        |

Para ajuste via `STEP`, mantendo `PERIOD = 20 ms`:

| STEP | Ciclo completo |
|------|----------------|
| 10   | ~3,6 s         |
| 5    | ~7,2 s (atual) |
| 3    | ~12,0 s        |
| 2    | ~18,0 s        |

---

## 7. Resumo dos parâmetros configurados

| Parâmetro         | Registrador | Valor  | Significado                        |
|-------------------|-------------|--------|------------------------------------|
| Clock do timer    | —           | 64 MHz | SYSCLK sem divisão APB             |
| Prescaler         | PSC         | 63     | Divisor efetivo = 64 → 1 MHz/tick  |
| Período do PWM    | ARR         | 999    | 1000 ticks → f_PWM = 1 kHz         |
| Duty mínimo       | CCR min     | 50     | 5% de brilho                       |
| Duty máximo       | CCR max     | 950    | 95% de brilho                      |
| Passo do sweep    | STEP        | 5      | 0,5% de variação por intervalo     |
| Intervalo         | PERIOD      | 20 ms  | Cadência de atualização do sweep   |
| Ciclo de fade     | —           | ~7,2 s | Tempo de um ciclo min→max→min      |

---

## 8. Onde cada parâmetro vive no código

```
MX_TIM17_Init()          →  PSC = 63, ARR = 999
USER CODE BEGIN PV       →  FLASH_SWEEP_MIN, FLASH_SWEEP_MAX,
                             FLASH_SWEEP_STEP, FLASH_SWEEP_PERIOD
flashPWM_Start()         →  inicializa CCR em FLASH_SWEEP_MIN e liga o PWM
flashPWM_Sweep()         →  atualiza o CCR a cada FLASH_SWEEP_PERIOD ms
flashPWM_Stop()          →  para o PWM e força o pino em LOW
STATE_WAIT_CLASSIFICATION →  entry action chama Start(), corpo chama Sweep()
```
