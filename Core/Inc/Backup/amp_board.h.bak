/*
 * *****************************************************************************
 * NeatAmp software
 *
 * amp_board.h
 *
 * Author :  AIM65
 * Date : 6 Avr 2020
 * (c) AIM65
 *
 * *****************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef AMP_BOARD_H_
#define AMP_BOARD_H_

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/
enum DataType_en
{
	Byte,
	HalfWord,
	Word,
};

//Status of encoder : still/left/right, updated by polling
typedef enum
{
	still,
	left,
	right,
}
ENC_event_td;

typedef struct
{
	bool 		nochange;
	uint32_t	crc;
}chk_res_td;

enum Time_cmd_en
{
	Get_Tick,
	Launch_ctr1,
	Launch_ctr2,
	Launch_ctr3,
	Launch_tmr1,
	Launch_tmr2,
	Launch_tmr3,
	Get_ctr1,
	Get_ctr2,
	Get_ctr3,
	Get_tmr1,
	Get_tmr2,
	Get_tmr3,
};

//Typedef  to handle parameters of two boards : master/slave
typedef struct
{
	uint16_t		m_volume;
	uint16_t		m_balance;
	uint8_t			m_stereo;
	uint8_t			m_lrswap;
	uint8_t			m_config;
	uint8_t			m_filterset;
	uint16_t		m_Param1;
	uint16_t		m_Param2;
	uint16_t		m_Param3;
	uint16_t		m_Param4;
	uint16_t		m_Param5;
	uint16_t		m_Param6;
	uint16_t		m_Param7;
	uint16_t		m_Param8;
	uint16_t		m_Param9;
	uint16_t		m_Param10;
	uint16_t		m_Param11;
	uint16_t		m_Param12;
	uint16_t		s_volume;
	uint16_t		s_balance;
	uint8_t			s_stereo;
	uint8_t			s_lrswap;
	uint8_t			s_config;
	uint8_t			s_filterset;
	uint16_t		s_Param1;
	uint16_t		s_Param2;
	uint16_t		s_Param3;
	uint16_t		s_Param4;
	uint16_t		s_Param5;
	uint16_t		s_Param6;
	uint16_t		s_Param7;
	uint16_t		s_Param8;
	uint16_t		s_Param9;
	uint16_t		s_Param10;
	uint16_t		s_Param11;
	uint16_t		s_Param12;
}board_user_param_td;				//size is 64 bytes


/* Exported constants --------------------------------------------------------*/
//Amp define
#define BAL_CENTER				20
#define BAL_RIGHT				(BAL_CENTER+BAL_CENTER)
#define BAL_LEFT				(BAL_CENTER-BAL_CENTER)
#define DEFAULT_VOL				30

#define NOCONFIG				0xFF

#define PSU_FAULT_RST_DLY		3000		// 3s delay to reset integrator
#define PSU_FAULT_LONG_DLY		1000		// integrator will start with 1s minimun period
#define PSU_FAULT_SHRT_DLY		100			// integrator will use 100ms period

//STM32 MSP define
#define I2CXTIMEOUT				20			//20ms

#define TIMERSIZE				0xFFFF		//Function TimeCounter uses 16bits counters

#define STM32_VDDA 				3.3f	   	// ADC Vref
#define STM32ADC_Resol			4096
#define STM32ANALOGBITS			12
#define STM32ANALOGMID			(STM32ADC_Resol/2-1)
#define STM32_VREFINTCAL		*(uint16_t*)0x1FFFF7BA	//factory recorded value

#define MASK_LED				0x07
#define BLACK_LED				0
#define WHITE_LED				0x07
#define GREEN_LED				0x01
#define BLUE_LED				0x02
#define RED_LED					0x04


//TAS3251 define
#define TAS_BASEADDR0			0x94
#define TAS_BASEADDR1			0x96
#define CLIP_TRESHOLD			400			//maximum duration in ms of a CLIP_OTW pulse low to notify a clipping condition

//ATMEL/MICROCHIP AT24C256B define
#define EEPROM_BASEADDR			0xA0								//24C256 I2C base address
#define EEPROM_PAGE				512									//qty of pages
#define EEPROM_PAGESIZE			64
#define CHKSUMADDR				(EEPROM_PAGESIZE*EEPROM_PAGE - 2)	//two last byte on top of EEPROM memory space

/* Exported macro --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

void AMP_Init(void);

uint32_t Checksum_Calc(int new, uint8_t *pdata, uint32_t length);

void I2Cx_Error(void);
void GPIO_Int_On(uint32_t Pin);
void GPIO_Int_Off(uint32_t Pin);

void EEPROM_Init(void);
void EEPROM_Checksum(chk_res_td *result);
void EEPROM_Update_checksum(void);
void EEPROM_WR(uint16_t Addr, enum DataType_en Type, int Data);
void EEPROM_RD(uint16_t Addr, uint16_t nbyte, uint8_t* pdata);
void EEPROM_WR_Page(uint16_t Page, const uint8_t* pdata);

ENC_event_td ENC_Manage(void);

void TAS_Init(void);
void TAS_On(void);
void TAS_Off(void);
void TAS_Write_Register(const uint8_t* data);
void TAS_Write_Coeff(uint8_t Reg, uint32_t* Coeff, int Qty);
uint8_t TAS_RD_reg(uint8_t book, uint8_t page, uint8_t reg);
void TAS_WR_Preset(uint8_t memory, uint8_t type);
void TAS_Get_Status (void);

uint32_t TimeCounter(enum Time_cmd_en Cmd, uint32_t duration);

#endif /* AMP_BOARD_H_ */
