/*
 * *****************************************************************************
 * NeatAmp software
 *
 * neatamp.c
 *
 * Author :  AIM65
 * Date : 6 Avr 2020
 * (c) AIM65
 *
 * Use some parts of common.h, module of ST AN2557 software package
 *
 * *****************************************************************************
*/

/**
  ******************************************************************************
  * @file    IAP/inc/common.h 
  * @author  MCD Application Team
  * @version V3.3.0
  * @date    10/15/2010
  * @brief   This file provides all the headers of the common functions.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2010 STMicroelectronics</center></h2>
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _NEATAMP_H
#define _NEATAMP_H

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "stm32f0xx_hal.h"


/* Exported types ------------------------------------------------------------*/
typedef enum
{
	CommandLine,
	CommandList,
	Not_a_command,
	Download,
	MemoryDisplay,
	Memory2load,
	Memory2play,
	Memory2erase,
	Status
} Serial_Eventtd;


typedef enum
{
	No_UI_event,
	Push_button_pressed,
	Ack,
	Transient_Error,
	Flash_on_clip,
	Fault
}Ui_Eventtd;

/* Exported constants --------------------------------------------------------*/
// Constants used by Serial Command Line Mode
#define CR					0x0D
#define LF					0x0A
#define CMD_STRING_SIZE     128

#define MAX_FILE_SIZE       50*1024				//50kbytes file size limit

// Constants used by the storage of config and filter send data in EEPROM
#define NAMESIZE			13							// 8.3 format + \0
#define RECORDSIZE			(NAMESIZE+2)				// 2 bytes to store number of byte
#define PRESETQTY			8							// 8 config presets and 8 filterset presets
#define STORAGESIZE			((PRESETQTY*RECORDSIZE)+1)	//in byte,
#define USAGE_IDX			STORAGESIZE-1				//

// Memory map constant
#define CONFIG_SIZE			6		// 6 pages of 64 Bytes
#define FILTERSET_SIZE		50		// 50 pages

#define CONFIG1_PAADDR		6
#define FILTERSET1_PAADDR	(CONFIG1_PAADDR+(CONFIG_SIZE*PRESETQTY))

#define ADC_MAX_VALUE			4095
#define VOLUME_STEP				4
#define VOL_DEC_SPEED			300			//Volume decrease speed = 1/100 every 500ms
#define MAXGAINSETTING			48			//See Datasheet Reg 0x3d,0x3e, MAXGAINSETTING set to +6dB
#define FLASH_DURATION			300			//Led flash duration is 300ms

/* Exported variables --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
/* Common routines */
#define IS_AF(c)  ((c >= 'A') && (c <= 'F'))
#define IS_af(c)  ((c >= 'a') && (c <= 'f'))
#define IS_09(c)  ((c >= '0') && (c <= '9'))
#define ISVALIDHEX(c)  IS_AF(c) || IS_af(c) || IS_09(c)
#define ISVALIDDEC(c)  IS_09(c)
#define CONVERTDEC(c)  (c - '0')

#define CONVERTHEX_alpha(c)  (IS_AF(c) ? (c - 'A'+10) : (c - 'a'+10))
#define CONVERTHEX(c)   (IS_09(c) ? (c - '0') : CONVERTHEX_alpha(c))

#define SerialPutString(x) Serial_PutString((uint8_t*)(x))

/* Exported functions ------------------------------------------------------- */
void Manage_serial_event(void);
void Manage_UI_event(void);
void Manage_supervision_event(void);
void Manage_delay_event(void);

void Serial_Init(void);
void Manage_serial_cmd(uint8_t cmd);
void Serial_Download(void);

void Display_memory(void);

void Save_data_page(int pagenumber, uint8_t memory, int type, uint8_t * buffer);
void Save_preset_memories(void);
void Save_settings(void);

void Update_preset_name(uint8_t memory, char* name, int type);
void Update_preset_size(uint8_t memory, uint16_t* size, int type);
void Get_preset_size(uint8_t memory, uint16_t* size, int type);

void TAS_Send_cmdbloc(uint8_t* cmd);
void TAS_Set_Volume (uint8_t vol);

uint8_t GetKey(void);
uint32_t SerialKeyPressed(uint8_t *key);
void SerialPutChar(uint8_t c);
void Serial_PutString(uint8_t *s);
void Serial_Draw_line(uint8_t nbr, char c);

uint32_t GetIntegerInput(int32_t * num);
void GetInputString(uint8_t * buff);

void Int2Str(uint8_t* str,int32_t intnum);
uint8_t HexStr2Byte (uint8_t *inputstr);
uint32_t Str2Int(uint8_t *inputstr,int32_t *intnum);

#endif  /* _NEATAMP_H */
