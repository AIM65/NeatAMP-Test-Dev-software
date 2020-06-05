/* USER CODE BEGIN Header */
/*
 * *****************************************************************************
 * NeatAmp software
 *
 * main.h
 *
 * Author :  AIM65
 * Date : 6 Avr 2020
 * (c) AIM65
 *
 * Made with ST CubeMX and ST CubeIDE
 *
 * *****************************************************************************
*/
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum
{
	No_Supervision_event,
	Clip_otw_active,
	Clipping,
	Vdd_drop,
	I2C_Fault,
	PSU_Fault,
	TAS_Fault,
} Supervision_Eventtd;

typedef enum
{
	ItRocks,
	Ok_NoOut,
	Shutdown,
	Green,
	White,
	Black,
	Red,
	Blue
	} Amp_Statusttd;


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

uint8_t User_Vol;
uint8_t User_Bal;
uint8_t User_stereo;

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MCU_LED_Pin GPIO_PIN_14
#define MCU_LED_GPIO_Port GPIOC
#define POT_Pin GPIO_PIN_0
#define POT_GPIO_Port GPIOA
#define PSU_MON_Pin GPIO_PIN_1
#define PSU_MON_GPIO_Port GPIOA
#define PUSH_BUT_Pin GPIO_PIN_5
#define PUSH_BUT_GPIO_Port GPIOA
#define PUSH_BUT_EXTI_IRQn EXTI4_15_IRQn
#define ENC1_Pin GPIO_PIN_6
#define ENC1_GPIO_Port GPIOA
#define ENC2_Pin GPIO_PIN_7
#define ENC2_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_1
#define LED_BLUE_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_2
#define LED_RED_GPIO_Port GPIOB
#define TAS_RST_Pin GPIO_PIN_7
#define TAS_RST_GPIO_Port GPIOF
#define CLIP_OTW_Pin GPIO_PIN_15
#define CLIP_OTW_GPIO_Port GPIOA
#define CLIP_OTW_EXTI_IRQn EXTI4_15_IRQn
#define FAULT_Pin GPIO_PIN_3
#define FAULT_GPIO_Port GPIOB
#define FAULT_EXTI_IRQn EXTI2_3_IRQn
#define ADR_Pin GPIO_PIN_4
#define ADR_GPIO_Port GPIOB
#define DAC_MUTE_Pin GPIO_PIN_8
#define DAC_MUTE_GPIO_Port GPIOB
#define CHAIN_RST_Pin GPIO_PIN_9
#define CHAIN_RST_GPIO_Port GPIOB
#define CHAIN_RST_EXTI_IRQn EXTI4_15_IRQn
/* USER CODE BEGIN Private defines */


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
