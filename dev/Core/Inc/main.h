/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define XTAL0_Pin GPIO_PIN_0
#define XTAL0_GPIO_Port GPIOF
#define XTAL1_Pin GPIO_PIN_1
#define XTAL1_GPIO_Port GPIOF
#define USR_SW_Pin GPIO_PIN_0
#define USR_SW_GPIO_Port GPIOA
#define PIEZO_Pin GPIO_PIN_1
#define PIEZO_GPIO_Port GPIOA
#define UART2_TX_Pin GPIO_PIN_2
#define UART2_TX_GPIO_Port GPIOA
#define UART2_RX_Pin GPIO_PIN_3
#define UART2_RX_GPIO_Port GPIOA
#define CTX_LED_Pin GPIO_PIN_4
#define CTX_LED_GPIO_Port GPIOA
#define CRX_LED_Pin GPIO_PIN_5
#define CRX_LED_GPIO_Port GPIOA
#define SYS_LED_Pin GPIO_PIN_6
#define SYS_LED_GPIO_Port GPIOA
#define UART1_TX_Pin GPIO_PIN_9
#define UART1_TX_GPIO_Port GPIOA
#define UART1_RX_Pin GPIO_PIN_10
#define UART1_RX_GPIO_Port GPIOA
#define ERR_LED_Pin GPIO_PIN_15
#define ERR_LED_GPIO_Port GPIOA
#define STATUS_LED_Pin GPIO_PIN_3
#define STATUS_LED_GPIO_Port GPIOB
#define RDY_LED_Pin GPIO_PIN_4
#define RDY_LED_GPIO_Port GPIOB
#define LOCK_Pin GPIO_PIN_5
#define LOCK_GPIO_Port GPIOB
#define I2C_SCL_Pin GPIO_PIN_6
#define I2C_SCL_GPIO_Port GPIOB
#define I2C_SDA_Pin GPIO_PIN_7
#define I2C_SDA_GPIO_Port GPIOB
#define BOOT_SEL_Pin GPIO_PIN_8
#define BOOT_SEL_GPIO_Port GPIOB
#define PN532_RST_Pin GPIO_PIN_7
#define PN532_RST_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
