/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
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
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * CubeIDE debug variables
 */
volatile uint8_t  pn532_debug_state = 0;
volatile uint8_t  pn532_debug_ack[6] = {0};
volatile uint8_t  pn532_debug_rx[64] = {0};
volatile uint8_t  pn532_debug_rx_len = 0;
volatile uint8_t  pn532_debug_uid[10] = {0};
volatile uint8_t  pn532_debug_uid_len = 0;
volatile uint32_t pn532_debug_errors = 0;
volatile uint8_t  last_valid_frame[64] = {0};
volatile uint8_t  last_valid_frame_len = 0;
volatile HAL_StatusTypeDef debug_uart_status = HAL_OK;
volatile uint32_t           debug_uart_error_code = 0;
volatile uint8_t            debug_ack_buffer[6] = {0};




#define PN532_MAX_FRAME 64

static uint8_t pn532_frame[PN532_MAX_FRAME];


/*
 * ACK officiel du PN532
 *
 * 00 00 FF 00 FF 00
 */
static const uint8_t PN532_ACK[] =
{
    0x00,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00
};


/*
 * Envoie une commande au PN532.
 *
 * data[0] = TFI (0xD4)
 * data[1] = commande
 * ...
 */
static HAL_StatusTypeDef PN532_SendCommand(uint8_t *data, uint8_t len)
{
    uint8_t frame[32];
    uint8_t i;
    uint8_t checksum = 0;
    uint8_t index = 0;

    if (len > 24)
        return HAL_ERROR;

    /*
     * Preamble
     */
    frame[index++] = 0x00;
    frame[index++] = 0x00;
    frame[index++] = 0xFF;

    /*
     * LEN
     */
    frame[index++] = len;

    /*
     * LCS = complément à 2 de LEN
     */
    frame[index++] = (uint8_t)(0x100 - len);

    /*
     * DATA
     */
    for (i = 0; i < len; i++)
    {
        frame[index++] = data[i];
        checksum += data[i];
    }

    /*
     * DCS = complément à 2 de la somme DATA
     */
    frame[index++] = (uint8_t)(0x100 - checksum);

    /*
     * Postamble
     */
    frame[index++] = 0x00;

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2,
                                                  frame,
                                                  index,
                                                  100);

    if (status == HAL_OK)
        pn532_debug_state = 1;
    else
    {
        pn532_debug_state = 5;
        pn532_debug_errors++;
    }

    return status;
}


/*
 * Attend et vérifie l'ACK du PN532.
 */
static uint8_t PN532_ReadAck(void)
{
    uint8_t ack[6];

    // On stocke le statut exact du retour de la fonction HAL
    debug_uart_status = HAL_UART_Receive(&huart2,
                                         ack,
                                         6,
                                         5000);

    // On capture aussi le code d'erreur interne de l'UART STM32
    debug_uart_error_code = huart2.ErrorCode;

    // On copie ce qui a éventuellement été lu pour l'observer
    for(int i=0; i<6; i++) {
        debug_ack_buffer[i] = ack[i];
    }

    if (debug_uart_status != HAL_OK)
    {
        return 0;
    }

    if (memcmp(ack, PN532_ACK, 6) == 0)
    {
        pn532_debug_state = 2;
        return 1;
    }

    pn532_debug_state = 5;
    pn532_debug_errors++;
    return 0;
}


/*
 * Lit une trame PN532 complète.
 *
 * Retourne dans 'data':
 *
 *   TFI + DATA
 *
 * Exemple réponse:
 *
 * D5 4B 01 01 00 04 08 04 53 4C 48 FA
 *
 */
static uint8_t PN532_ReadFrame(uint8_t *data,
                               uint8_t *data_len,
                               uint32_t timeout)
{
    uint8_t byte;
    uint8_t len;
    uint8_t lcs;
    uint8_t checksum;
    uint8_t i;

    /*
     * Cherche le début:
     *
     * 00 00 FF
     */
    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    while (byte != 0x00)
    {
        if (HAL_UART_Receive(&huart2,
                             &byte,
                             1,
                             timeout) != HAL_OK)
        {
            return 0;
        }
    }

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0x00)
        return 0;

    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0xFF)
        return 0;

    /*
     * LEN
     */
    if (HAL_UART_Receive(&huart2,
                         &len,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    /*
     * ACK = LEN 0
     * Ce cas ne devrait normalement pas arriver ici.
     */
    if (len == 0)
    {
        uint8_t tmp;

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        HAL_UART_Receive(&huart2,
                         &tmp,
                         1,
                         timeout);

        return 0;
    }

    /*
     * LCS
     */
    if (HAL_UART_Receive(&huart2,
                         &lcs,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if ((uint8_t)(len + lcs) != 0)
        return 0;

    if (len > PN532_MAX_FRAME)
        return 0;

    /*
     * DATA
     */
    checksum = 0;

    for (i = 0; i < len; i++)
    {
        if (HAL_UART_Receive(&huart2,
                             &data[i],
                             1,
                             timeout) != HAL_OK)
        {
            return 0;
        }

        checksum += data[i];
    }

    /*
     * DCS
     */
    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if ((uint8_t)(checksum + byte) != 0)
        return 0;

    /*
     * Postamble
     */
    if (HAL_UART_Receive(&huart2,
                         &byte,
                         1,
                         timeout) != HAL_OK)
    {
        return 0;
    }

    if (byte != 0x00)
        return 0;

    *data_len = len;
    memcpy((void *)pn532_debug_rx, data, len);
    pn532_debug_rx_len = len;
    pn532_debug_state = 3;

    return 1;
}


/*
 * Réveille le PN532.
 */
static void PN532_Wakeup(void)
{
    // Séquence exacte de réveil HSU (Préambule + Commande SAMConfiguration)
    uint8_t wakeup_packet[] = {
        0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Long préambule de réveil
        0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00  // Commande SAMConfiguration (Normal Mode)
    };

    // Envoi de la séquence au module
    HAL_UART_Transmit(&huart2, wakeup_packet, sizeof(wakeup_packet), 500);

    // Pause indispensable pour laisser le temps au PN532 de traiter le réveil
    HAL_Delay(100);
}


/*
 * Initialise le PN532.
 */
static uint8_t PN532_Init(void)
{
    // 1. Réveil matériel et logiciel indispensable en UART
    PN532_Wakeup();

    // 2. Ensuite, on peut envoyer une commande de test (ex: GetFirmwareVersion)
    uint8_t cmd[] = {
        0x00, 0x00, 0xFF,
        0x02, 0xFE,
        0xD4, 0x02,
        0x2A,
        0x00
    };

    __HAL_UART_CLEAR_OREFLAG(&huart2);

    if (HAL_UART_Transmit(&huart2, cmd, sizeof(cmd), 500) != HAL_OK)
    {
        return 0;
    }

    uint8_t dummy_ack[6];
    // On récupère l'ACK de cette commande d'initialisation
    if (HAL_UART_Receive(&huart2, dummy_ack, 6, 500) != HAL_OK)
    {
    	// TODO: debug the init fail here
        return 0;
    }

    return 1;
}

/*
 * Recherche une carte ISO14443A.
 *
 * Retourne:
 *
 *   1 = carte détectée
 *   0 = aucune carte
 */
static uint8_t PN532_ReadCard(uint8_t *uid,
                              uint8_t *uid_len)
{
    uint8_t command[] =
    {
        0xD4,
        0x4A,
        0x01,
        0x00
    };

    uint8_t response[PN532_MAX_FRAME];
    uint8_t response_len;


    /*
     * Envoie InListPassiveTarget.
     */
    if (PN532_SendCommand(command,
                          sizeof(command)) != HAL_OK)
    {
        return 0;
    }

    /*
     * ACK immédiat.
     */

    if (!PN532_ReadAck())
    {
        return 0;
    }


    /*
     * La réponse peut prendre du temps,
     * notamment lorsqu'aucune carte n'est présente.
     * timout of 1.5 seconds
     */
    if (!PN532_ReadFrame(response,
                         &response_len,
                         1500))
    {
        return 0;
    }

    /*
     * Réponse:
     *
     * D5 4B NbTg ...
     */
    if (response_len < 3)
        return 0;

    if (response[0] != 0xD5 ||
        response[1] != 0x4B)
    {
        return 0;
    }

    /*
     * NbTg = nombre de cartes détectées.
     */
    if (response[2] == 0)
    {
        return 0;
    }

    /*
     * Structure réponse Type A:
     *
     * D5 4B
     * NbTg
     * Tg
     * SENS_RES[2]
     * SEL_RES
     * NFCIDLength
     * NFCID...
     */

    if (response_len < 8)
        return 0;

    *uid_len = response[7];

    if (*uid_len > 10)
        return 0;

    if ((8 + *uid_len) > response_len)
        return 0;

    memcpy(uid,
           &response[8],
           *uid_len);
    // save the complete trame
	memcpy((void *)last_valid_frame, response, response_len);
	last_valid_frame_len = response_len;

	memcpy(uid,
		   &response[8],
		   *uid_len);

	return 1;

    return 1;
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1000);

  HAL_GPIO_WritePin(LOCK_GPIO_Port, LOCK_Pin, GPIO_PIN_RESET);

  pn532_debug_state = 0;
  pn532_debug_errors =0;

  if (!PN532_Init()) {
	  // error led if init fail
	  HAL_GPIO_WritePin(ERR_LED_GPIO_Port, ERR_LED_Pin, GPIO_PIN_SET);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /*
	   //blink test
	        HAL_GPIO_WritePin(LOCK_GPIO_Port,
                        LOCK_Pin,
                        GPIO_PIN_SET);
      HAL_Delay(500);
      HAL_GPIO_WritePin(LOCK_GPIO_Port,
                        LOCK_Pin,
                        GPIO_PIN_RESET);
	   */


      uint8_t uid[10];
      uint8_t uid_len = 0;

      pn532_debug_state = 0;

      if (PN532_ReadCard(uid, &uid_len))
      {
          memcpy((void *)pn532_debug_uid, uid, uid_len);
          pn532_debug_uid_len = uid_len;
          pn532_debug_state = 4;

          HAL_GPIO_WritePin(LOCK_GPIO_Port,
                            LOCK_Pin,
                            GPIO_PIN_SET);

          HAL_Delay(3000);

          HAL_GPIO_WritePin(LOCK_GPIO_Port,
                            LOCK_Pin,
                            GPIO_PIN_RESET);

          HAL_Delay(500);
      }

      HAL_Delay(50);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN;
  hcan.Init.Prescaler = 16;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 38400;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, PIEZO_Pin|CTX_LED_Pin|CRX_LED_Pin|SYS_LED_Pin
                          |ERR_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, STATUS_LED_Pin|RDY_LED_Pin|LOCK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USR_SW_Pin */
  GPIO_InitStruct.Pin = USR_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USR_SW_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PIEZO_Pin CTX_LED_Pin CRX_LED_Pin SYS_LED_Pin
                           ERR_LED_Pin */
  GPIO_InitStruct.Pin = PIEZO_Pin|CTX_LED_Pin|CRX_LED_Pin|SYS_LED_Pin
                          |ERR_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : STATUS_LED_Pin RDY_LED_Pin LOCK_Pin */
  GPIO_InitStruct.Pin = STATUS_LED_Pin|RDY_LED_Pin|LOCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT_SEL_Pin */
  GPIO_InitStruct.Pin = BOOT_SEL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT_SEL_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
