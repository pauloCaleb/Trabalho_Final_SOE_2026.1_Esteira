/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Corpo do programa principal
  ******************************************************************************
  * 	TRABALHO FINAL - SISTEMAS OPERACIONAIS EMBARCADOS - SOE
  * 			FIRMWARE EMBARCADO - ESTEIRA SEPARADORA
  *
  * 		Semestre: 2026.1
  * 		Autor: Paulo Caleb Fernandes da Silva
  * 		Co-Autor: Felipe de Castro
  *
  * Objetivo: Gerenciar o Hardware embarcado na esteira para controlar os sensores
  *			  e atuadores previstos no esquemático, abrindo uma porta de comunicação UART
  * 		  para comunicação entre o hardware embarcado na esteira e o software embarcado
  * 	      no Raspberry pi 3;
  * 		  O sistema controlado pelo presente firmware não realiza a escolha do destino do
  * 		  objeto sobre a esteira, apenas segue comandos e executa a ação. O processamento
  * 		  de alto nível de abstração (imagem) ocorre no SBC Raspberry Pi 3b, que é responsável
  * 		  por definir a rota e comandar o hardware da esteira (controlado pelo presente firmware) por UART;
  *
  * 		  O presente firmware utiliza os Driversa HAL,
  * 		  disponibilizados industrialmente pela STMicroelectronics
  * 	      para a família de microcontroladores STM32.
  *
  * Informações técnicas:	Fcpu: 64MHz;
  * 			     		As GPIOs configuram os pinos de controle do driver do motor de passo;
  * 			    		O timer 15 é usado para a modulação dos lasers (PWM fixo);
  * 			    		O timer 3 é usado para o frequencímetro dos sensores (Input capture);
  * 			    		O timer16 é usado para o controle do servo motor (PWM com duty variável);
  * 			    		O timer 6 é usado para o controle do motor de passo (interrupção interna);
  * 			    		O timer 17 é usado para o PWM de brilho variável do flash (fade-in/fade-out);
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*----------------------------QUADRO DE COMUNICAÇÃO---------------------------*/
// Estrutura do quadro
#define START_FRAME_MSG         0xAA
#define CMD_OK_MSG              0x90
#define CMD_ERR_MSG             0x91
// Handshake
#define SYS_RDY_MSG             0x10
#define SYS_INIT_MSG            0x01
// FSM - RX
#define ROUTE_A_RECEIVE_MSG     0xDA
#define ROUTE_B_RECEIVE_MSG     0xDB
// FSM - TX
#define OBJ_DETECTED_MSG        0xA0
#define CLSS_REQUEST_MSG        0xC0
#define ROUTE_A_FWRDNG_MSG      0xFA
#define ROUTE_B_FWRDNG_MSG      0xFB
#define ROUTE_A_SCCSS_DLVRY_MSG 0xBA
#define ROUTE_B_SCCSS_DLVRY_MSG 0xBB
// Controle assíncrono - RX
#define LIGHT_EN_MSG            0xE1
#define LIGHT_DISABLE_MSG       0xD1
#define GATE_OPEN_MSG           0xE2
#define GATE_CLOSE_MSG          0xD2
#define STPR_EN_MSG             0xE3
#define STPR_DISABLE_MSG        0xD3
#define SET_STPR_FORWARD_MSG    0xE4
#define SET_STPR_BACKWARD_MSG   0xD4
#define SET_STPR_TGT_STPS_MSG   0xE5
// Modo de operação
#define DEBUG_MODE_TOGGLE_MSG   0xDD
#define MODE_FSM_MSG            0x11
#define MODE_DEBUG_MSG          0x22
#define SW_RESET_MSG            0x33
// Telemetria de sensores
#define SENS_STATUS_MSG         0x55

/*----------------------------SERVO MOTOR-------------------------------------*/
#define SERVO_MIN_US 600	//Menor faixa de pulso do servo (em us)
#define SERVO_MAX_US 2300	//Maior faixa de pulso do servo (em us)
#define SERVO_MAX_ANGLE 180	//Mior ângulo do servo (em graus)
#define SERVO_MIN_ANGLE 0	//Menor ângulo do servo (em graus)
/*----------------------------MOTOR DE-PASSO----------------------------------*/
#define TIM6_FREQUENCY 1000000	//Frequencia de trabalho do timer 6
#define STEPPER_MAX_VEL 420	//Velocidade máxima do motor em passos/s (ajustar para valor real)
#define STEPPER_MIN_VEL 300//Velocidade mínima do motor em passos/s

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim16;
TIM_HandleTypeDef htim17;
DMA_HandleTypeDef hdma_tim3_ch1;
DMA_HandleTypeDef hdma_tim3_ch2;
DMA_HandleTypeDef hdma_tim3_ch3;
DMA_HandleTypeDef hdma_tim3_ch4;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/*----------------------------SERVO MOTOR-------------------------------------*/
volatile uint8_t angle = 0;
uint8_t openAngle = 45;
uint8_t closeAngle = 0;
/*----------------------------MOTOR DE-PASSO----------------------------------*/
volatile uint8_t pulse_ctrl = 1; //flag que controla o disparo dos pulsos
volatile uint8_t movement_done_flag = 0; //Flag de movimento concluído do motor de passo
volatile uint32_t targetStepps = 0; //Globbal que armazena a quantidade de passos a serem dados pelo motor
volatile uint32_t stepperEN = 1; //Controla o acionamento do motor de passo
volatile uint8_t stepperDirInst = 0; //Define a direção do motor de passo instantâneamente
/*----------------------------SENSORES LASER----------------------------------*/
uint32_t sensUPDT = 10; //Período de captura dos sensores (em ms), esse valor deve ser no mínimo o dobro do período do sinal da portadora (TIM15)
uint32_t nowTick = 0;//Auxiliar para a contagem do tempo
//
uint32_t inputCapture_SENS1[2];//Captura duas bordas de descida do sensor 1
uint32_t SENS1_capture = 0;//Diferenca entre a ultima e a primeira capturas de bordas
uint32_t SENS1_freq = 0;//Frequencia instantânea do sensor
uint8_t  SENS1_flag = 0;//Flag de controle do sensor
//
uint32_t inputCapture_SENS2[2];//Captura duas bordas de descida do sensor 2
uint32_t SENS2_capture = 0;//Diferenca entre a ultima e a primeira capturas de bordas
uint32_t SENS2_freq = 0;//Frequencia instantânea do sensor
uint8_t  SENS2_flag = 0;//Flag de controle do sensor
//
uint32_t inputCapture_SENS3[2];//Captura duas bordas de descida do sensor 3
uint32_t SENS3_capture = 0;//Diferenca entre a ultima e a primeira capturas de bordas
uint32_t SENS3_freq = 0;//Frequencia instantânea do sensor
uint8_t  SENS3_flag = 0;//Flag de controle do sensor
//
uint32_t inputCapture_SENS4[2];//Captura duas bordas de descida do sensor 4
uint32_t SENS4_capture = 0;//Diferenca entre a ultima e a primeira capturas de bordas
uint32_t SENS4_freq = 0;//Frequencia instantânea do sensor
uint8_t  SENS4_flag = 0;//Flag de controle do sensor
//
uint8_t sensStatusByte     = 0x00; // STATUS_BYTE atual (bits 0-3 = flags SENS1-4)
uint8_t sensStatusByteLast = 0xFF; // Valor anterior; 0xFF força envio na primeira iteração

/*------------------------MÁQUINA DE ESTADOS FINITOS----------------------------------*/
typedef enum {
    STATE_IDLE = 0,
    STATE_OBJECT_DETECTED = 1,
    STATE_WAIT_CLASSIFICATION = 2,
    STATE_ROUTE_A = 3,
    STATE_ROUTE_B = 4,
	STATE_NONE = 255  // sentinela: força entry action na primeira execução
} FSM_State_t;

FSM_State_t state = STATE_IDLE;
FSM_State_t last_state = STATE_NONE; // força execução inicial

/*----------------------------MODO DE OPERAÇÃO--------------------------------*/
typedef enum {
    OP_MODE_FSM   = 0,  // Modo normal: FSM rodando
    OP_MODE_DEBUG = 1   // Modo debug: apenas comandos assíncronos
} OperationMode_t;

volatile OperationMode_t operationMode = OP_MODE_FSM; // Inicia em modo FSM

/*----------------------------RECEPÇÃO DE FRAME UART--------------------------*/
typedef enum {
    RX_WAIT_START = 0,  // Aguardando byte 0xAA
    RX_WAIT_CMD   = 1,  // Aguardando byte de comando
    RX_WAIT_DATA  = 2   // Aguardando byte de dado (só SET_STPR_TGT_STPS_MSG)
} RxFrameState_t;

volatile RxFrameState_t rxFrameState = RX_WAIT_START;
volatile uint8_t rxPendingCMD  = 0x00; // CMD do frame em montagem
volatile uint8_t rxFrameReady  = 0;    // Flag: frame completo pronto para processar
volatile uint8_t rxCMD         = 0x00; // CMD do último frame completo recebido
volatile uint8_t rxDATA        = 0x00; // DATA do último frame completo (se houver)
/*--------------------------FLAGS DE TRANSMISSÃO-------------------*/
//tx
uint8_t objDetectedMSG_flag = 0;//Flag de controle da mensagem de objeto detectado
uint8_t classificationRequestMSG_flag = 0;//Flag de controle da mensagem de solicitação de destino
uint8_t routeAforwardingMSG_flag = 0;//Flag de controle da mensagem de encaminhamento para a rota A
uint8_t routeBforwardingMSG_flag = 0;//Flag de controle da mensagem de encaminhamento para a rota B
uint8_t routeASuccesDeliveryMSG_flag = 0;//Flag de controle da mensagem de sucesso na entrega para a rota A
uint8_t routeBSuccesDeliveryMSG_flag = 0;//Flag de controle da mensagem de sucesso na entrega para a rota B


/*----------------------------FLASH PWM SWEEP---------------------------------*/
#define FLASH_SWEEP_MIN    50   // Duty mínimo (brilho mínimo ~5%)
#define FLASH_SWEEP_MAX    950  // Duty máximo (brilho máximo ~95%)
#define FLASH_SWEEP_STEP   10    // Incremento por chamada (velocidade do fade)
#define FLASH_SWEEP_PERIOD 20     // Intervalo entre passos em ms

uint32_t flashSweep_duty    = FLASH_SWEEP_MIN;
int8_t   flashSweep_dir     = 1;
uint32_t flashSweep_lastTick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM15_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM17_Init(void);
/* USER CODE BEGIN PFP */
void getSensorsReadings();//Obtém a leitura dos sensores laser
void SetServoAngle(uint16_t angle); //Seta o angulo do motor de passo
void stepperMove(uint32_t steps, uint8_t direction);//Move o motor de passo uma quantidade de passos em um sentido definido
void stepperSetSpeed(uint32_t steppsPerSeconds);//Define a velocidade do motor de passo
void stepperStopDisengaged();//Para o motor com o eixo livre
void stepperStopEngaged();//Para o motor com o eixo travado
void stepperFollowSteps();//Asegue a quantidade de passos definida por stepperMove
void STEPPER_TIM6_ISR();//Interrupção responsável por controlar o motor de passo
void handleModeToggle(void); // Verifica e processa o comando de troca de modo
void handleAsyncCommands(void); // Executa os comandos assíncronos
void FSM();	//Executa a máquina de estados finitos
void sendFrame(uint8_t status, uint8_t payload); //Executa a montagem e o envio do quadro de TX para as mensagens espontâneas
void telemetryFrame(uint8_t status, uint8_t cmd, uint8_t data); //Executa a montagem do quadro de TX da telemetria dos sensores
void sendTelemetryData(void); //Faz a tomada de decisão para o envio dos dados de telemetria
void flashPWM_Start(void); // Inicia PWM do flash via TIM17
void flashPWM_Stop(void);           // Para PWM do flash
void flashPWM_Sweep(void);          // Atualiza fade-in/fade-out do flash
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*--------------------RECEPÇÃO DE FRAME UART------------------------------*/
uint8_t rxBuffer [5];
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	HAL_UART_Receive_IT(&huart2, rxBuffer, 1);
	    uint8_t byte = rxBuffer[0];

	    switch(rxFrameState){
	        case RX_WAIT_START:
	            if(byte == START_FRAME_MSG){        // 0xAA
	                rxFrameState = RX_WAIT_CMD;
	            }
	            // Qualquer outro byte fora de frame é ignorado silenciosamente
	            break;

	        case RX_WAIT_CMD:
	            rxPendingCMD = byte;
	            if(byte == SET_STPR_TGT_STPS_MSG){  // 0xE5 — espera DATA a seguir
	                rxFrameState = RX_WAIT_DATA;
	            } else {
	                // Frame de 2 bytes completo
	                rxCMD        = rxPendingCMD;
	                rxDATA       = 0x00;
	                rxFrameReady = 1;
	                rxFrameState = RX_WAIT_START;
	            }
	            break;

	        case RX_WAIT_DATA:
	            // Frame de 3 bytes completo
	            rxCMD        = rxPendingCMD;
	            rxDATA       = byte;
	            rxFrameReady = 1;
	            rxFrameState = RX_WAIT_START;
	            break;
	    }
}

/*--------------------TRANSMISSÃO DE FRAME UART------------------------------*/
/**
 * @brief Envia um frame TX padronizado.
 *
 * Resposta a comando RX:  sendFrame(CMD_OK_MSG,  cmdEco)   → [0x90][cmdEco]
 * Erro de reconhecimento: sendFrame(CMD_ERR_MSG, 0x00)     → [0x91]
 * Mensagem espontânea:    sendFrame(CMD_OK_MSG,  MSG_BYTE) → [0x90][MSG_BYTE]
 */
void sendFrame(uint8_t status, uint8_t payload){
    uint8_t frame[2];
    frame[0] = status;
    frame[1] = payload;

    if(status == CMD_ERR_MSG){
        HAL_UART_Transmit(&huart2, frame, 1, 1000); // CMD_ERR é single-byte
    } else {
        HAL_UART_Transmit(&huart2, frame, 2, 1000); // CMD_OK sempre com eco
    }
}


/**
 * @brief Envia frame de 3 bytes: [status][cmd][data]
 * Usado pela telemetria de sensores:
 *   telemetryFrame(CMD_OK_MSG, SENS_STATUS_MSG, statusByte)
 */
void telemetryFrame(uint8_t status, uint8_t cmd, uint8_t data){
    uint8_t frame[3];
    frame[0] = status;
    frame[1] = cmd;
    frame[2] = data;
    HAL_UART_Transmit(&huart2, frame, 3, 1000);
}

/**
 * @brief Envia telemetria dos 4 sensores laser.
 *
 * Só executa no modo Debug (OP_MODE_DEBUG).
 * Só envia quando o STATUS_BYTE mudar em relação à última transmissão,
 * evitando flood no barramento UART.
 *
 * STATUS_BYTE:
 *   bit 0 → SENS1_flag  (1 = objeto detectado, 0 = feixe livre)
 *   bit 1 → SENS2_flag
 *   bit 2 → SENS3_flag
 *   bit 3 → SENS4_flag
 *   bits 4-7 → reservados (0)
 *
 * Frame enviado: [0x90][0x55][STATUS_BYTE]
 */
void sendTelemetryData(void){
    if(operationMode != OP_MODE_DEBUG) return; // Só no modo debug

    sensStatusByte = (SENS1_flag & 0x01)        |
                     ((SENS2_flag & 0x01) << 1)  |
                     ((SENS3_flag & 0x01) << 2)  |
                     ((SENS4_flag & 0x01) << 3);

    if(sensStatusByte != sensStatusByteLast){   // Só envia se mudou
        telemetryFrame(CMD_OK_MSG, SENS_STATUS_MSG, sensStatusByte);
        sensStatusByteLast = sensStatusByte;
    }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  //Todos os SysInit foram incluídos nos trechos de configuração dos respectivos periféricos
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM15_Init();
  MX_TIM3_Init();
  MX_TIM16_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_TIM17_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Receive_IT(&huart2, rxBuffer, 1);

  //Aguarda a inicialização dos sensores
  uint8_t start = 0;
  uint8_t sensorsInit = 0;
  uint32_t dbgLedBlink = 0;
  while(!start){
	  stepperStopDisengaged();
	  getSensorsReadings();
	  if(SENS1_freq == 200 && SENS2_freq == 200 && SENS3_freq == 200 && SENS4_freq == 200 ){
		  sensorsInit = 1;
	  }
	  if(sensorsInit && rxFrameReady && rxCMD == SYS_RDY_MSG){
	      rxFrameReady = 0;
	      sendFrame(CMD_OK_MSG, SYS_INIT_MSG); //Informa ao RBPi3 que o sistema iniciou (fim do Handshake)
	      sendFrame(CMD_OK_MSG, MODE_FSM_MSG); //Por default inicia em modo FSM
	      HAL_GPIO_WritePin(dbgLED_GPIO_Port, dbgLED_Pin, GPIO_PIN_RESET);//Desliga o led
	      start = 1;
	  }
	  //Blink do LED de debug: pisca rápido se for identificado falha nos sensores, pisca devagar se estiver esperando o handshake do RBPi3
	  if(HAL_GetTick() - dbgLedBlink > (sensorsInit ? 500 : 100)){
			HAL_GPIO_TogglePin(dbgLED_GPIO_Port, dbgLED_Pin); // Assumindo LED no PA5 (comum em NUCLEO)
			dbgLedBlink = HAL_GetTick();
		}
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  getSensorsReadings(); //Obtém as leituras dos sensores
	  sendTelemetryData(); //Chamada da função para envio da telemetria (ocorre somente em modo debug)
	  handleModeToggle();      // 1º: verifica troca de modo (sempre, em qualquer modo)

	    if(operationMode == OP_MODE_DEBUG){
	        handleAsyncCommands(); // Modo debug: só comandos assíncronos
	        //SetServoAngle(angle); //Aciona o servo com stlink no modo debug
	    } else {
	        FSM();                 // Modo normal: FSM completa
	    }

	    //Verificação para a transmissão das mensagens
	    if(objDetectedMSG_flag){
	        objDetectedMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, OBJ_DETECTED_MSG);
	    }
	    if(classificationRequestMSG_flag){
	        classificationRequestMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, CLSS_REQUEST_MSG);
	    }
	    if(routeAforwardingMSG_flag){
	        routeAforwardingMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, ROUTE_A_FWRDNG_MSG);
	    }
	    if(routeBforwardingMSG_flag){
	        routeBforwardingMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, ROUTE_B_FWRDNG_MSG);
	    }
	    if(routeASuccesDeliveryMSG_flag){
	        routeASuccesDeliveryMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, ROUTE_A_SCCSS_DLVRY_MSG);
	    }
	    if(routeBSuccesDeliveryMSG_flag){
	        routeBSuccesDeliveryMSG_flag = 0;
	        sendFrame(CMD_OK_MSG, ROUTE_B_SCCSS_DLVRY_MSG);
	    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 63;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 63;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */
  HAL_TIM_Base_Start_IT(&htim6);	//Habilita a interrupção do timer 6
  stepperSetSpeed(STEPPER_MIN_VEL); //Define a velocidade mínima para o motor de passo
  stepperFollowSteps();	//Ativa o modo seguidor de passos
  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM15 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM15_Init(void)
{

  /* USER CODE BEGIN TIM15_Init 0 */

  /* USER CODE END TIM15_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM15_Init 1 */

  /* USER CODE END TIM15_Init 1 */
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 6399;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 49;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM15_Init 2 */
  HAL_TIM_MspPostInit(&htim15);
  HAL_TIM_Base_Start(&htim15);
  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);	//Inicia o modo PWM
  __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, 25); //Seta o PWM para 50 por cento a partir do CCR

  /* USER CODE END TIM15_Init 2 */
  HAL_TIM_MspPostInit(&htim15);

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 63;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 19999;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */
  HAL_TIM_MspPostInit(&htim16);
  HAL_TIM_Base_Start(&htim16);
  HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);	//Inicia o modo PWM do servo
  SetServoAngle(0); //Coloca o servo na posição inicial

  /* USER CODE END TIM16_Init 2 */
  HAL_TIM_MspPostInit(&htim16);

}

/**
  * @brief TIM17 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM17_Init(void)
{

  /* USER CODE BEGIN TIM17_Init 0 */

  /* USER CODE END TIM17_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM17_Init 1 */

  /* USER CODE END TIM17_Init 1 */
  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 63;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 999;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim17, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim17, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM17_Init 2 */
  HAL_TIM_MspPostInit(&htim17);
  HAL_TIM_Base_Start(&htim17);
  /* USER CODE END TIM17_Init 2 */
  HAL_TIM_MspPostInit(&htim17);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  /* DMA1_Ch4_7_DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Ch4_7_DMAMUX1_OVR_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DIR_Pin|STEP_Pin|SLEEP_Pin|dbgLED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : DIR_Pin STEP_Pin SLEEP_Pin dbgLED_Pin */
  GPIO_InitStruct.Pin = DIR_Pin|STEP_Pin|SLEEP_Pin|dbgLED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//---------------------DESENVOLVIMENTO DAS FUNÇÕES-----------------------------*/
/*--------------------SENSORES LASER---------------------------------*/
/**
 * @brief Implementa um detector síncrono digital por aus~encia de portadora;
 * A portadora é gerada pelo timer 15, que pulsa o laser no sensor.
 * A frequência detectada pelo sensor é a mesma frequência da portadora;
 * Quando um objeto interrompe o feixe do laser, a portadora some, fato que é
 * indicado pela ausencia do sinal no ponto de interrupção do feixe.
 *
 * 	SENSn_freq - Frequencia medida no n-ésimo sensor
 * 	SENSn_flag - Bandeira de indicação do estado do feixe do laser sobre o sensor
 */
void getSensorsReadings(){
	//Base de tempo no loop infinito
    if(HAL_GetTick() - nowTick > sensUPDT){
		__HAL_TIM_SET_COUNTER(&htim3, 0);//Zera o contador do timer 3 para iniciar nova amostragem dos sensores
		//Sensor 1
		HAL_TIM_IC_Stop_DMA(&htim3, TIM_CHANNEL_1);
		HAL_TIM_IC_Start_DMA(&htim3, TIM_CHANNEL_1, inputCapture_SENS1, 2); //Inicia o DMA no canal 1, correspondente ao sensor 1 do sistema
		SENS1_capture = inputCapture_SENS1[1] - inputCapture_SENS1[0];
		if(SENS1_capture == 0){
			SENS1_flag = 1; //Encontrou um objeto na frente do sensor
			SENS1_freq = 0;
		} else {
			SENS1_freq = 1000000/SENS1_capture;
			SENS1_flag = 0;
		}
		inputCapture_SENS1[1] = 0;
		inputCapture_SENS1[0] = 0;
		//Sensor 2
		HAL_TIM_IC_Stop_DMA(&htim3, TIM_CHANNEL_2);
		HAL_TIM_IC_Start_DMA(&htim3, TIM_CHANNEL_2, inputCapture_SENS2, 2); //Inicia o DMA no canal 1, correspondente ao sensor 1 do sistema
		SENS2_capture = inputCapture_SENS2[1] - inputCapture_SENS2[0];
		if(SENS2_capture == 0){
			SENS2_flag = 1; //Encontrou um objeto na frente do sensor
			SENS2_freq = 0;
		} else {
			SENS2_freq = 1000000/SENS2_capture;
			SENS2_flag = 0;
		}
		inputCapture_SENS2[1] = 0;
		inputCapture_SENS2[0] = 0;
		//Sensor 3
		HAL_TIM_IC_Stop_DMA(&htim3, TIM_CHANNEL_3);
		HAL_TIM_IC_Start_DMA(&htim3, TIM_CHANNEL_3, inputCapture_SENS3, 2); //Inicia o DMA no canal 1, correspondente ao sensor 1 do sistema
		SENS3_capture = inputCapture_SENS3[1] - inputCapture_SENS3[0];
		if(SENS3_capture == 0){
			SENS3_flag = 1; //Encontrou um objeto na frente do sensor
			SENS3_freq = 0;
		} else {
			SENS3_freq = 1000000/SENS3_capture;
			SENS3_flag = 0;
		}
		inputCapture_SENS3[1] = 0;
		inputCapture_SENS3[0] = 0;
		//Sensor 4
		HAL_TIM_IC_Stop_DMA(&htim3, TIM_CHANNEL_4);
		HAL_TIM_IC_Start_DMA(&htim3, TIM_CHANNEL_4, inputCapture_SENS4, 2); //Inicia o DMA no canal 1, correspondente ao sensor 1 do sistema
		SENS4_capture = inputCapture_SENS4[1] - inputCapture_SENS4[0];
		if(SENS4_capture == 0){
			SENS4_flag = 1; //Encontrou um objeto na frente do sensor
			SENS4_freq = 0;
		} else {
		   SENS4_freq = 1000000/SENS4_capture;
		   SENS4_flag = 0;
		}
		inputCapture_SENS4[1] = 0;
		inputCapture_SENS4[0] = 0;
		//Atualiza a contagem do tick
		nowTick = HAL_GetTick();
    }
}
/*--------------------SERVO MOTOR-----------------------------------*/
/**
 * @brief Função para definição do angulo do servo motor ES08MA
 * Objetivo: mapear o valor do algulo em ticks do timeR;
 *
 * Utiliza o TIM16, que está gerando uma base de tempo de 1us.
 * O argumento da função é o ângulo desejadopara o servo, não retorna argumentos,
 * apenas define o duty cycle correspondente ao angulo desejado
 *
 */
void SetServoAngle(uint16_t angle){
	//Verificação da faixa
	if(angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
	if(angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
	//Mapeamento da fiaxa para os valores de ticks do timer
	uint32_t pulse_length = SERVO_MIN_US + ((uint32_t)angle*(SERVO_MAX_US-SERVO_MIN_US)/SERVO_MAX_ANGLE); //Realiza o mapeamento de acordo com as constantes do servo motor e a faixa 0-180 graus
	__HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, pulse_length);//Seta o duty de acorodo com o mapeamento do angulo desejado
}
/*--------------------MOTOR DE PASSO-----------------------------------*/
/**
 * @brief Move o motor de passo uma quantidade finita de passos a uma
 * velocidade constante definida pelo timer 6, de acordo com a direção escolhida pelo usuário:
 * '0' move o motor para frente (na esteira), '1' move o motor para trás (na esteira)
 */
void stepperMove(uint32_t steps, uint8_t direction)
{
	//Define a direção do motor
	if(direction == 0){
		HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
	}
	if(targetStepps == 0){
		//Se o motor concluiu a utima solicitação de passos
		targetStepps = steps;//Atualiza nova quantidade de passos
	}
	//Avaliar situação de busy do motor de passo

}
/*-------------------------------------------------------------------*/
/**
 * @brief Ajusta o ARR para mudar a frequência de interrupção do TIM6,
 * ajustando a velocidade do motor em passos/s
 */
void stepperSetSpeed(uint32_t steppsPerSeconds){
	//Verifica se o parametro passado é igual ou superior a velocidade máxima do mmotor de passo
	if(steppsPerSeconds >= STEPPER_MAX_VEL){
		steppsPerSeconds = STEPPER_MAX_VEL;
	}
	if(steppsPerSeconds <= STEPPER_MIN_VEL){
		steppsPerSeconds = STEPPER_MIN_VEL;
	}
	//Se não, calcula novo ARR baseado na velocidade
	uint32_t TIM6_intFreq = 2 * steppsPerSeconds;
	uint32_t newARR = ((TIM6_FREQUENCY/TIM6_intFreq) - 1);
	__HAL_TIM_SET_AUTORELOAD(&htim6, newARR); //Atualiza o ARR
	//__HAL_TIM_SET_COUNTER(&htim6, 0); //Zera o contador do timer para nova velocidade
}
/*-------------------------------------------------------------------*/
/**
 * @brief Para o motor liberando o eixo
 */
void stepperStopDisengaged(){
	stepperEN = 0; //Inibe a geração de pulsos do timer 6
	__HAL_TIM_DISABLE(&htim6); //Desativa o timer 6
	__HAL_TIM_SET_COUNTER(&htim6, 0); //Zera o contador do timer 6
	HAL_GPIO_WritePin(SLEEP_GPIO_Port, SLEEP_Pin, GPIO_PIN_RESET); //Desliga o driver a4988
	HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, GPIO_PIN_RESET); //Prepara para borda de subida
}
/*-------------------------------------------------------------------*/
/**
 * @brief Para o motor mantendo o eixo travado
 */
void stepperStopEngaged(){
	stepperEN = 0; //Inibe a geração de pulsos do timer 6
	__HAL_TIM_DISABLE(&htim6); //Desativa o timer 6
	__HAL_TIM_SET_COUNTER(&htim6, 0); //Zera o contador do timer 6
	HAL_GPIO_WritePin(SLEEP_GPIO_Port, SLEEP_Pin, GPIO_PIN_SET); //Liga o driver a4988
	HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, GPIO_PIN_RESET); //Prepara para borda de subida
}
/*-------------------------------------------------------------------*/
/**
 * @brief Ativa o modo follow stepps do motor de passo, quando chamada,
 * habilita o timer 6 e permite a continuidade da lógica de acionamento do motor,
 * fazendo-o girar até zerar o targetStepps
 */
void stepperFollowSteps(){
	HAL_GPIO_WritePin(SLEEP_GPIO_Port, SLEEP_Pin, GPIO_PIN_SET); //Liga o driver a4988
	HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, GPIO_PIN_RESET); //Prepara para borda de subida
	stepperEN = 1; //Permite a geração de pulsos do timer 6
	pulse_ctrl = 1; //Reseta controle de pulsos
	__HAL_TIM_SET_COUNTER(&htim6, 0); //Zera o contador do timer 6
	__HAL_TIM_ENABLE(&htim6); //Ativa o timer 6
}
/*------------INTERRUPT SERVICE ROUTINES --------------------------------------*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	//Avalia qual timer causou a interrupção
	if(htim == &htim6){
		STEPPER_TIM6_ISR(); //Se foi o timer 6, chama a função de controle do motor de passo
	}
}
/*---------DESENVOLVIMETO DAS ROTINAS DE INTERRUPÇÃO---------------------------*/
/**
 * @brief Função chamada na interrupção do timer 6 para controlar os
 * pulsos enviados ao driver a4988
 */
void STEPPER_TIM6_ISR(){
	//Se a interrupção foi causada pelo timer 6, verifica se pode dar um passo no motor
	if(stepperEN == 1){
		//Permitido o passo, verifica se chegou a zero
		if(targetStepps > 0){
			//Se a quantidade de passos for superior a 0
			HAL_GPIO_TogglePin(STEP_GPIO_Port, STEP_Pin); //Dá um toggle no pino de step
			//Lógica para coincidir a quantidade de passos com a geração de pulsos:
			if(pulse_ctrl){
				//Se já deu uma borda de subida
				targetStepps --;//Decrementa
			}
			pulse_ctrl = !pulse_ctrl;//Inverte o estado para garantir o dobro de toggles no pino STEP (um pulso)
		} else if(targetStepps == 0){
			HAL_GPIO_WritePin(STEP_GPIO_Port, STEP_Pin, GPIO_PIN_RESET); //Desliga a GPIO para o proximo trem de pulsos iniciar com a borda de subida
			movement_done_flag = 1;
		}
	}
}
/*-------------------------------------------------------------------*/

/*--------------------CONTROLE DE MODO DE OPERAÇÃO---------------------------*/
/**
 * @brief Verifica se o comando de toggle de modo foi recebido e alterna
 * entre OP_MODE_FSM e OP_MODE_DEBUG.
 * Deve ser chamada ANTES de handleAsyncCommands() e FSM() no loop principal.
 *
 * OP_MODE_FSM   -> recebeu 0xDD -> OP_MODE_DEBUG: para a FSM, motor travado,
 *                                   flash apagado, ouve só comandos assíncronos
 * OP_MODE_DEBUG -> recebeu 0xDD -> OP_MODE_FSM:   retoma a FSM do STATE_IDLE
 */

void handleModeToggle(void){
    if(!rxFrameReady) return;

    //Reset por software
		if(rxCMD == SW_RESET_MSG){
			rxFrameReady = 0;
			sendFrame(CMD_OK_MSG, SW_RESET_MSG); // Confirma antes de resetar
			HAL_Delay(50);                       // Pequena espera para o frame sair pelo TX
			__NVIC_SystemReset();                  // Reset por software via CMSIS
		}


    if(rxCMD != DEBUG_MODE_TOGGLE_MSG) return;

    rxFrameReady = 0; // Consome o frame
    sendFrame(CMD_OK_MSG, rxCMD); // Confirma recebimento

    if(operationMode == OP_MODE_FSM){
        operationMode = OP_MODE_DEBUG;
        stepperStopEngaged();
        flashPWM_Stop();
        sendFrame(CMD_OK_MSG, MODE_DEBUG_MSG); //Informa o modo de operação
        for(uint8_t i = 0; i < 6; i++){
            HAL_GPIO_TogglePin(dbgLED_GPIO_Port, dbgLED_Pin);
            HAL_Delay(80);
        }
    } else {
        operationMode = OP_MODE_FSM;
        state      = STATE_IDLE;
        last_state = STATE_NONE;
        sensStatusByteLast = 0xFF; //força reenvio de telemetria ao retornar ao debug
        sendFrame(CMD_OK_MSG, MODE_FSM_MSG); //Informa o modo de operação
        HAL_GPIO_WritePin(dbgLED_GPIO_Port, dbgLED_Pin, GPIO_PIN_SET);
        HAL_Delay(400);
        HAL_GPIO_WritePin(dbgLED_GPIO_Port, dbgLED_Pin, GPIO_PIN_RESET);
    }
}
/*-------------------- CONTROLE ASSÍNCRONO --------------------------*/
/**
 * @brief Handler do controle assíncrono da esteira
 * Define a aplicação de debug
 */
void handleAsyncCommands(void){
    if(operationMode != OP_MODE_DEBUG) return;
    if(!rxFrameReady) return;

    rxFrameReady = 0; // Consome o frame
    stepperMove(targetStepps, stepperDirInst);
    switch(rxCMD){
        case LIGHT_EN_MSG:
        	HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
        	__HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, FLASH_SWEEP_MAX);
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case LIGHT_DISABLE_MSG:
        	flashPWM_Stop();
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case GATE_OPEN_MSG:
            SetServoAngle(openAngle);
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case GATE_CLOSE_MSG:
            SetServoAngle(closeAngle);
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case STPR_EN_MSG:
            stepperFollowSteps();
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case STPR_DISABLE_MSG:
            stepperStopDisengaged();
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case SET_STPR_FORWARD_MSG:
            stepperDirInst = 0;
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case SET_STPR_BACKWARD_MSG:
            stepperDirInst = 1;
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        case SET_STPR_TGT_STPS_MSG:
            targetStepps = (uint32_t)rxDATA;
            sendFrame(CMD_OK_MSG, rxCMD);   break;
        default:
            sendFrame(CMD_ERR_MSG, 0x00);   break;
    }
}


/*--------------------MÁQUINA DE ESTADOS FINITOS---------------------------*/
/**
 * @brief Desenvolvimento da função Máquina de estados finitos, que implementa a rotina
 * da aplicação principal da esteira
 */
void FSM(void){
	stepperDirInst = 0; //Coloca a direção do motor de passo no sentido correto da FSM
    /* ========= ENTRY ACTION ========= */
    if(state != last_state)
    {
        switch(state)
        {
        case STATE_IDLE:
        	flashPWM_Stop();//Desliga o flash
            SetServoAngle(0);//Abre a cancela
            stepperFollowSteps(); //Manda o motor seguir os passos
            break;
        case STATE_OBJECT_DETECTED:
        	stepperFollowSteps(); //Manda o motor seguir os passos
            break;
        case STATE_WAIT_CLASSIFICATION:
        	flashPWM_Start(); //Inicializa o timer e reseta o Duty
            stepperStopEngaged(); //Para o motor com o eixo travado
            break;
        case STATE_ROUTE_A:
            SetServoAngle(closeAngle);//Abre a cancela
            flashPWM_Stop(); //Desliga o flash
            stepperFollowSteps(); //Manda o motor seguir os passos
            break;
        case STATE_ROUTE_B:
            SetServoAngle(openAngle);//Fecha a cancela
            flashPWM_Stop(); //Desliga o flash
            stepperFollowSteps(); //Manda o motor seguir os passos
            break;
        case STATE_NONE:
        	break;
        }
        last_state = state; //Iguala os estados para fechar a condição de entrada
    }
    /* ========= STATE MACHINE ========= */
    switch(state) {
		case STATE_IDLE:
			//No estado IDLE
			stepperMove(350, stepperDirInst); //Solicita passos do motor
			if(SENS1_flag){ //Verifica o estado do sensor 1
				SENS1_flag = 0;//consome evento
				state = STATE_OBJECT_DETECTED; //Avança para o próximo estado
				objDetectedMSG_flag = 1; //Seta a flag de envio da mensagem de objeto detectado
			}
			break;
		case STATE_OBJECT_DETECTED:
			//No estado STATE OBJECT_DETECTED
			stepperMove(350, stepperDirInst); //Solicita passos do motor
			if(SENS2_flag){//Verifica o sensor 2
				SENS2_flag = 0;
				state = STATE_WAIT_CLASSIFICATION;//Avança o estado
				classificationRequestMSG_flag = 1; //Seta a flag de envio da mensagem de solicitação de classificação
			}
			break;
		case STATE_WAIT_CLASSIFICATION:
			flashPWM_Sweep(); //Atualiza continuamente o sweep do flash enquanto nesse estado
		    if(rxFrameReady){
		        if(rxCMD == ROUTE_A_RECEIVE_MSG){
		            rxFrameReady = 0;
		            sendFrame(CMD_OK_MSG, rxCMD);
		            state = STATE_ROUTE_A;
		            routeAforwardingMSG_flag = 1;
		        } else if(rxCMD == ROUTE_B_RECEIVE_MSG){
		            rxFrameReady = 0;
		            sendFrame(CMD_OK_MSG, rxCMD);
		            state = STATE_ROUTE_B;
		            routeBforwardingMSG_flag = 1;
		        }
		    }
		    break;
		case STATE_ROUTE_A:
			//Na rota A
			stepperMove(350, stepperDirInst); //Solicita passos do motor
			if(SENS3_flag){ //Verifica o sensor da saída da rota A
				SENS3_flag = 0;
				state = STATE_IDLE;//Retorna para o estado inicial
				routeASuccesDeliveryMSG_flag = 1; //Seta a flag de entrega bem sucedida na rota A
			}
			break;
		case STATE_ROUTE_B:
			//Na rota B
			stepperMove(350, stepperDirInst); //Solicita passos do motor
			if(SENS4_flag){//Verifica o sensor da saíida da rota B
				SENS4_flag = 0;
				state = STATE_IDLE;//Retorna para o estado inicial
				routeBSuccesDeliveryMSG_flag = 1; //Seta a flag de entrega bem sucedida na rota B
			}
			break;
		default:
			state = STATE_IDLE;//Por default entra no estado IDLE
			break;
		}
}

/*--------------------FLASH PWM (TIM17 CH1) ----------------------------------*/
/**
 * @brief Inicia o PWM do flash com duty cycle especificado (0-19999)
 * Chamada no entry action de STATE_WAIT_CLASSIFICATION
 */
void flashPWM_Start(void){
    // Reinicia estado do sweep
    flashSweep_duty     = FLASH_SWEEP_MIN;
    flashSweep_dir      = 1;
    flashSweep_lastTick = HAL_GetTick();

    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, flashSweep_duty);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
}

/**
 * @brief Para o PWM do flash e garante que o pino fique em LOW
 * Chamada nos entry actions de STATE_ROUTE_A, STATE_ROUTE_B e STATE_IDLE
 */
void flashPWM_Stop(void){
    HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
    HAL_GPIO_WritePin(FLASH_PWM_GPIO_Port, FLASH_PWM_Pin, GPIO_PIN_RESET);
}

/*--------------------FLASH PWM SWEEP (fade-in / fade-out) -------------------*/
/**
 * @brief Atualiza o duty cycle do TIM17 para criar efeito fade-in/fade-out.
 * Deve ser chamada repetidamente no loop do STATE_WAIT_CLASSIFICATION.
 * Usa base de tempo própria (FLASH_SWEEP_PERIOD) para ser independente
 * da velocidade do loop principal.
 *
 * Parâmetros configuráveis via defines:
 *   FLASH_SWEEP_MIN    -- duty mínimo (piso do fade)
 *   FLASH_SWEEP_MAX    -- duty máximo (teto do fade)
 *   FLASH_SWEEP_STEP   -- tamanho do passo por intervalo (velocidade)
 *   FLASH_SWEEP_PERIOD -- intervalo entre passos em ms (cadência)
 */
void flashPWM_Sweep(void){
    if(HAL_GetTick() - flashSweep_lastTick < FLASH_SWEEP_PERIOD) return;
    flashSweep_lastTick = HAL_GetTick();

    int32_t next = (int32_t)flashSweep_duty + flashSweep_dir * (int32_t)FLASH_SWEEP_STEP;

    if(next >= (int32_t)FLASH_SWEEP_MAX){
        next = FLASH_SWEEP_MAX;
        flashSweep_dir = -1;
    } else if(next <= (int32_t)FLASH_SWEEP_MIN){
        next = FLASH_SWEEP_MIN;
        flashSweep_dir = 1;
    }

    flashSweep_duty = (uint32_t)next;
    __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, flashSweep_duty);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
