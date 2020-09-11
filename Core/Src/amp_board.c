/*
 * *****************************************************************************
 * NeatAmp software
 *
 * amp_board.c
 *
 * Author :  AIM65
 * Date : 6 Avr 2020
 * (c) AIM65
 *
 * *****************************************************************************
*/

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "amp_board.h"
#include "neatamp.h"
#include "adc.h"
#include "crc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
const char Board_ID[EEPROM_PAGESIZE]=" NeatAMP V1 Project by JMF11 / Soft By AIM65 / DIYAudio / 2020";
const char Version[] = {"\r\n V2.1 - 10 Septembre 2020\r\n"};
extern const char crlf[];

volatile uint16_t ADC_Values[2];
uint8_t tas_addr;
uint8_t tas_status[]={					//This array define the registers which will be displayed by the 's' serial command
				    0x04,0x00,			//it also provide space to store the value of each registers
					0x0d,0x00,
					0x5b,0x00,
					0x5c,0x00,
					0x5d,0x00,
					0x5e,0x00,
					0x5f,0x00};

uint8_t tas_status_siz=sizeof(tas_status);
union {
	uint8_t page1[EEPROM_PAGESIZE];
	board_user_param_td boards_param;
	} user_param;

extern const uint8_t serial_cmd[];
extern const char *Serial_cmd_desc[];
extern uint8_t config_memory[STORAGESIZE];
extern uint8_t filterset_memory[STORAGESIZE];

extern uint8_t cmd_qty;
extern uint8_t rx_serial;

extern Serial_Eventtd Serial_Event;
extern Supervision_Eventtd Supervision_Event;
extern Ui_Eventtd Ui_Event;
extern Amp_Statusttd Amp_Status;


extern bool wait4command;

extern const uint8_t Cmd_Bloc_Swap[];
extern const uint8_t Cmd_Bloc_Unmute[];
extern const uint8_t Cmd_Bloc_Switch2b8c[];
extern const uint8_t Cmd_Bloc_Switch2b00[];


/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************
 * AMP Initialization
 *
 * STM32 MSP usage :
 * TIM3		Encoder
 * TIM1		100Hz Time base for ADC1-1&2 then DMA1-1
 * TIM6		1ms Time base
 * ADC		PSU_Monitor : GVDD & VDD monitored in ADC_Values[1]
 * 			Potentiometer monitored in ADC_Values[0]
 * 			Watchdog set on Channel 1 to 0xC25 to trig interrupt
 * DMA		to write to memory results of ADC conversion
 * CRC		To calculate EEPROM checksum
 * I2C1		To deal with EEPROM and TAS3251
 * I2C2		To deal with other NeatAMP boards (not implemneted)
 * USART1	for serial link
 * GPIO		for IO
 * NVIC		for interrupts
 *
 *******************************************************************************
 */
void AMP_Init(void)
{
	HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);			//Start counter for encoder
	HAL_TIM_Base_Start(&htim1);								//Start counter for ADC acquisition
	HAL_TIM_Base_Start(&htim6);								//Start counter software timer
	HAL_ADCEx_Calibration_Start(&hadc);
	HAL_ADC_Start_DMA(&hadc, (uint32_t*) ADC_Values,2);		//launch free running DAC acquisition

	//EEPROM_WR(32760,Byte, 0xFF);							//force CRC error to reset EEPROM

	EEPROM_Init();											//Check EEPROM Status
	Serial_Init();											//launch serial monitor
	TAS_Init();												//Launch the amplifier
}

/*
 *******************************************************************************************************
 * Compute CRC32 of an array
 * Set new to zero in order to start a new CRC calculation
 *******************************************************************************************************
*/
uint32_t Checksum_Calc(int new, uint8_t *pdata, uint32_t length)
{
	uint32_t temp;
	if (new !=0)
	{
		temp = HAL_CRC_Accumulate(&hcrc, (uint32_t*) pdata,length);
	}
	else
	{
		temp = HAL_CRC_Calculate(&hcrc, (uint32_t*) pdata,length);
	}
	return temp;
}

	
void I2Cx_Error(void)
{
	Supervision_Event =  I2C_Fault;
	Manage_supervision_event();				//End loop : as this is major fault, function will never return
}

/*
 * ******************************************************************************************************
 * 2 functions to mask and unmask GPIO interrupts
 * ******************************************************************************************************
*/
void GPIO_Int_On(uint32_t Pin)
	{
	__HAL_GPIO_EXTI_CLEAR_IT(Pin);			//clear pending interrupt
	EXTI->IMR |= Pin;						//UnMask Int
	}
void GPIO_Int_Off(uint32_t Pin)
{
//Disable STM32 INT
	EXTI->IMR &= ~Pin;
}


//EEPROM related functions
/*
 * ******************************************************************************************************
 * Compute 16 bit truncated CRC32 of EEPROM
 * Return result in *presult
 * 16b truncated CRC32 in presult->crc
 * true in presult->nochange if computed value = stored value
 * ******************************************************************************************************
*/
void EEPROM_Checksum(chk_res_td *presult)
{
  uint16_t prev_chksum;
  uint8_t buffer[EEPROM_PAGESIZE];
	for (int i=0; i < EEPROM_PAGE; i++)
	{
		EEPROM_RD(i*EEPROM_PAGESIZE,EEPROM_PAGESIZE,buffer);
		if (i==EEPROM_PAGE-1)
		{
		  //Get checksum currently stored in last page EEPROM
		  prev_chksum = buffer[EEPROM_PAGESIZE-2];
		  prev_chksum += buffer[EEPROM_PAGESIZE-1]<<8;
		  //Clear previous checksum for new checksum calculation
		  buffer[EEPROM_PAGESIZE-2]=0;
		  buffer[EEPROM_PAGESIZE-1]=0;
		}
		//Is current EEPROM checksum equal to checksum stored in EEPROM?
		presult->crc = Checksum_Calc(i, (uint8_t*) buffer, EEPROM_PAGESIZE);
		prev_chksum ==  (uint16_t)presult->crc ? (presult->nochange = true) : (presult->nochange = false);
	}
}

void EEPROM_Update_checksum(void)
{
	chk_res_td result;
	EEPROM_Checksum(&result);
	EEPROM_WR((EEPROM_PAGE*EEPROM_PAGESIZE)-2,HalfWord,result.crc);
}

/*
 *******************************************************************************************************
 * Check if EEPROM content OK (CRC32), if not load default values
 *
 * EEPROM Page map :
 * Page0	Board_ID
 * Page1	User Parameter
 * Page2-3	Config list
 * Page4-5	Filter list
 *
 * Volume is between 0 and 99
 * 		0 is mute and 99 is Maximum gain. Maximum gain is set by MAXGAINSETTING
 * Balance id between 0 and 40
 * 		BAL_LEFT is left, BAL_CENTER is center, BAL_RIGHT is right
 *
 ********************************************************************************************************
*/
void EEPROM_Init(void)
{
	uint8_t buffer[EEPROM_PAGESIZE];
	int j,i=0;
	chk_res_td result;

	EEPROM_Checksum(&result);
	if (result.nochange == false)
	{
		//Bad checksum : load EEPROM with default values
		EEPROM_WR_Page(i, Board_ID);
		i++;
		user_param.boards_param.m_volume=DEFAULT_VOL;
		user_param.boards_param.s_volume=DEFAULT_VOL;
		user_param.boards_param.m_balance=BAL_CENTER;
		user_param.boards_param.s_balance=BAL_CENTER;
		user_param.boards_param.m_stereo=true;
		user_param.boards_param.s_stereo=true;
		user_param.boards_param.m_lrswap=false;
		user_param.boards_param.s_lrswap=false;
		user_param.boards_param.m_config=NOCONFIG;
		user_param.boards_param.s_config=NOCONFIG;
		user_param.boards_param.m_filterset=NOCONFIG;
		user_param.boards_param.s_filterset=NOCONFIG;
		EEPROM_WR_Page(i, user_param.page1);

		//Clear remaining space
		for (j=0; j < EEPROM_PAGESIZE; j++) buffer[j]=0;
		do
		{
			i++;
			EEPROM_WR_Page(i, buffer);
		}
		while (i < EEPROM_PAGE-1);
		EEPROM_Checksum(&result);
		buffer[EEPROM_PAGESIZE-2] = (uint8_t)result.crc;
		buffer[EEPROM_PAGESIZE-1] = (uint8_t)(result.crc >> 8);
		EEPROM_WR_Page(EEPROM_PAGE-1, buffer);
	}
	else
	{
		//Load in RAM, user parameters and preset memories. Data stays in EEPROM
		EEPROM_RD(1*EEPROM_PAGESIZE, EEPROM_PAGESIZE, user_param.page1);
		EEPROM_RD(2*EEPROM_PAGESIZE, 2*EEPROM_PAGESIZE, config_memory);
		EEPROM_RD(4*EEPROM_PAGESIZE, 2*EEPROM_PAGESIZE, filterset_memory);
	}
}

/*
 *******************************************************************************************************
 * Write data to EEPROM
 * handle format : 8,16 & 32b
 * handle page boundaries crossing
 *******************************************************************************************************
*/

void EEPROM_WR(uint16_t Addr, enum DataType_en Type, int Data)
{
	uint8_t size;
	if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_BASEADDR, 64, I2CXTIMEOUT)!= HAL_OK) I2Cx_Error();
	switch (Type){
		case Byte:
			size = 1;
			break;
		case HalfWord:
			if (Addr % EEPROM_PAGESIZE > EEPROM_PAGESIZE-2)
			{
				size = 1;
				EEPROM_WR(Addr, Byte, Data & 0xFF);
				EEPROM_WR(Addr+1, Byte, (Data >> 8) & 0xFF);
			}
			else
			{
				size = 2;
			}
			break;
		case Word:
			if (Addr % EEPROM_PAGESIZE > EEPROM_PAGESIZE-3)
			{
				size = 1;
				EEPROM_WR(Addr, Byte, Data & 0xFF);
				EEPROM_WR(Addr+1, Byte, (Data >> 8) & 0xFF);
				EEPROM_WR(Addr+2, Byte, (Data >> 16) & 0xFF);
				EEPROM_WR(Addr+3, Byte, (Data >> 24) & 0xFF);
			}
			else
			{
				size = 4;
			}
			break;
	}
	if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_BASEADDR, Addr, I2C_MEMADD_SIZE_16BIT,
			(uint8_t*) &Data, size, I2CXTIMEOUT) != HAL_OK)
		I2Cx_Error();
}

//Read one or several byte from EEPROM
void EEPROM_RD(uint16_t Addr, uint16_t nbyte, uint8_t* pdata)
{
	if (Addr > (EEPROM_PAGE*EEPROM_PAGESIZE)-nbyte) I2Cx_Error();
	if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_BASEADDR, 64, I2CXTIMEOUT)!= HAL_OK) I2Cx_Error();
	if (HAL_I2C_Mem_Read(&hi2c1, EEPROM_BASEADDR, Addr, I2C_MEMADD_SIZE_16BIT,pdata, nbyte, I2CXTIMEOUT) != HAL_OK) I2Cx_Error();
}

//Write a page (of bytes aligned on page boundaries)
void EEPROM_WR_Page(uint16_t Page, uint8_t* pdata)
{
	if (Page > EEPROM_PAGE-1) I2Cx_Error();
	if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_BASEADDR, 64, I2CXTIMEOUT) != HAL_OK) I2Cx_Error();
	if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_BASEADDR, Page * EEPROM_PAGESIZE,I2C_MEMADD_SIZE_16BIT, pdata, EEPROM_PAGESIZE, I2CXTIMEOUT) != HAL_OK)
		I2Cx_Error();
}

//TAS related functions
void TAS_Init(void)
{
	GPIO_Int_Off(FAULT_Pin);
	HAL_Delay(1000);
	User_Vol = user_param.boards_param.m_volume;
	User_Bal = user_param.boards_param.m_balance;
	User_stereo = user_param.boards_param.m_stereo;

	tas_addr =((HAL_GPIO_ReadPin(ADR_GPIO_Port, ADR_Pin) == GPIO_PIN_SET) ? TAS_BASEADDR1 : TAS_BASEADDR0);
	if (user_param.boards_param.m_config != NOCONFIG)
		{
			TAS_WR_Preset(user_param.boards_param.m_config, 'C');		//Apply Config to TAS
			TAS_WR_Preset(user_param.boards_param.m_filterset, 'F');	//Apply FilterSet to TAS
			TAS_Send_cmdbloc(Cmd_Bloc_Swap);							//Send Swap command
		}
	TAS_Set_Volume (User_Vol);											//Set volume
	if (!HAL_GPIO_ReadPin(FAULT_GPIO_Port, FAULT_Pin))
		{
			Supervision_Event = TAS_Fault;								//Do not start TAS if error at startup
		}
	else
		{
			TAS_On();
			Supervision_Event = No_Supervision_event;
			Amp_Status = ItRocks;
		}
}

void TAS_On(void)
{
	HAL_GPIO_WritePin(TAS_RST_GPIO_Port, TAS_RST_Pin, SET);			//Enable output stage
	HAL_Delay(1);
	GPIO_Int_On(FAULT_Pin);											//Enable Interrupt on Fault Pin
}


void TAS_Off(void)
{
	GPIO_Int_Off(FAULT_Pin);										//Mask Fault Interrupt
	HAL_GPIO_WritePin(TAS_RST_GPIO_Port, TAS_RST_Pin, RESET);		//Disable output stage
}


/*
 *******************************************************************************************************
 * Write a value to a register
 * register is *pdata; value is *pdata+1
 *******************************************************************************************************
*/
void TAS_Write_Register(uint8_t* pdata)
{
	uint8_t adrdta[2];
	adrdta[0] = (uint8_t) *pdata;
	adrdta[1] = (uint8_t) *(pdata+1);
	//adrdta[0] |= 0x80;
	if (HAL_I2C_Mem_Write(&hi2c1, tas_addr, adrdta[0],I2C_MEMADD_SIZE_8BIT, &adrdta[1], 1, I2CXTIMEOUT) != HAL_OK)
			I2Cx_Error();
}

/*
 ****************************************************************************************************
 * Write coefficients in the TAS, assuming Book and Page already set in TAS3251
 * Coeff are 32bits word
 *
 * Register is TAS sub adress
 * Coeff is an array of consecutive coeff
 * Qty is number of coeff to write
 *
 ****************************************************************************************************
 */

void TAS_Write_Coeff(uint8_t Reg, uint32_t* Coeff, int Qty)
{
	Reg |= 0x80;		//Set adress autoincrement mode
	if (HAL_I2C_Mem_Write(&hi2c1, tas_addr, Reg,I2C_MEMADD_SIZE_8BIT, Coeff, Qty*4, I2CXTIMEOUT) != HAL_OK)
				I2Cx_Error();
}


/*
 *******************************************************************************************************
 * Write a preset to EEPROM
 * type is C for Config preset, F for FilterSet preset
 * memory is the # of the preset (1 to 8)
 *******************************************************************************************************
*/
void TAS_WR_Preset(uint8_t memory, uint8_t type)
{
	uint16_t size,i,in_eeprom_addr;
	uint8_t	buff[2];
	Get_preset_size(memory, &size, type);
	if (type == 'C')
	{
		in_eeprom_addr = (CONFIG1_PAADDR + ((memory-1) * CONFIG_SIZE)) * EEPROM_PAGESIZE;
		//DAC_MUTE should be set to 0 while sending a whole config to the TAS
		HAL_GPIO_WritePin(DAC_MUTE_GPIO_Port, DAC_MUTE_Pin, RESET);
		HAL_Delay(5);			//required @I2C Clock = 400 kHz, when writing config preset to the TAS
	}
	if (type == 'F')
	{
		in_eeprom_addr = (FILTERSET1_PAADDR + ((memory-1) * FILTERSET_SIZE)) * EEPROM_PAGESIZE;
	}

	for (i=0; i < size; i += 2)
	{
		EEPROM_RD(in_eeprom_addr+i, 2, buff);
		TAS_Write_Register(buff);
	}
	if (type == 'C')
	{
		HAL_Delay(10);
		HAL_GPIO_WritePin(DAC_MUTE_GPIO_Port, DAC_MUTE_Pin, SET);
	}
}

void TAS_Get_Status (void)
{
	uint16_t dta=0;
	TAS_Send_cmdbloc(Cmd_Bloc_Switch2b00);
	for (int i=0; i < tas_status_siz; i+=2)
	{
		HAL_I2C_Mem_Read(&hi2c1, tas_addr, tas_status[i], I2C_MEMADD_SIZE_8BIT, tas_status+i+1, 1, I2CXTIMEOUT);
	}
}


//User Interface related functions

/*
 *******************************************************************************************************
 * Get status of the encoder with TIM3 encoder mode
 *******************************************************************************************************
*/
ENC_event_td ENC_Manage(void)
{
	ENC_event_td event;
	static int ENC_previous_pos=0;
	if (TIM3->CNT != ENC_previous_pos)
	{
		event = (((TIM3->CR1) & TIM_CR1_DIR) ? left:right);
		ENC_previous_pos = TIM3->CNT;
	}
	else event = still;
	return event;
}

/*
 *******************************************************************************************************
 * Non blocking timer based on TIM6, 1ms Time base, 16bits counter
 * Provide counters ctr1 & ctr2 & ctr3 which count Ticks from 0 to time base timer overflow (16bit)
 * Provide timers tmr1 & tmr2 & tmr3 which return 0 when timer delay elapsed
 *
 * Return Tick on Get_Tick command, duration not used
 * Return Tick on all Launch_ctrx commands, duration not used
 * Return Tick on all Launch_tmrx commands, duration used
 * Return duration since launch on Get_ctrX command, duration not used
 * Return 0 on Get_tmrX command when timer is stopped (delay elapsed)
 * Return 1 on Get_tmrX command when timer is running
 *
 *******************************************************************************************************
 * Current usage:
 * ctr1 is used to integrate undervoltage event : timeout to avoid false detection
 * ctr2 is used to integrate undervoltage event : provide a small integration period
 * ctr3 is used to screen pulses on CLIP_OTW
 * tmr1 is used to flash UI Leds
 * tmr2 is used to blink MCU_Led
 * tmr3 is used to set auto decrease volume speed on undervoltage and clip conditions
 *******************************************************************************************************
*/

uint32_t TimeCounter(enum Time_cmd_en Cmd, uint32_t duration)
{
	uint32_t time = TIM6->CNT;
	static uint32_t marker01, marker02, marker03, marker11, marker12, marker13, duration11, duration12, duration13;
	uint32_t temp = time;
	switch (Cmd)
	{
		case Get_Tick:
			break;
		case Launch_ctr1:
			marker01 = time;
			break;
		case Launch_ctr2:
			marker02 = time;
			break;
		case Launch_ctr3:
			marker03 = time;
			break;
		case Launch_tmr1:
			marker11 = time;
			duration11 = duration;
			break;
		case Launch_tmr2:
			marker12 = time;
			duration12 = duration;
			break;
		case Launch_tmr3:
			marker13 = time;
			duration13 = duration;
			break;
		case Get_ctr1:
			if (time < marker01)					//when CNT overflows
			{
				temp = TIMERSIZE - marker01 + time;
			}
			else
			{
				temp = time - marker01;
			}
			break;
		case Get_ctr2:
			if (time < marker02)					//when CNT overflows
			{
				temp = TIMERSIZE - marker02 + time;
			}
			else
			{
				temp = time - marker02;
			}
			break;
		case Get_ctr3:
			if (time < marker03)					//when CNT overflows
			{
				temp = TIMERSIZE - marker03 + time;
			}
			else
			{
				temp = time - marker03;
			}
			break;
		case Get_tmr1:
			if (duration11 == 0)				//is timer stopped ?
			{
				temp = 0;						//0 means timer stopped
			}
			else
			{
				if (time < marker11)			//when CNT overflows
				{
					temp = TIMERSIZE - marker11 + time;
				}
				else
				{
					temp = time - marker11;
				}
				if (temp > duration11)			//when duration11 elapsed
				{
					duration11 = 0;				//stop timer
					temp = 0;
				}
				else
				{
					temp = 1;					//1 means timer running
				}
			}
			break;
		case Get_tmr2:
			if (duration12 == 0)
			{
				temp = 0;
			}
			else
			{

				if (time < marker12)
				{
					temp = TIMERSIZE - marker12 + time;
				}
				else
				{
					temp = time - marker12;
				}
				if (temp > duration12)
				{
					duration12 = 0;
					temp = 0;
				}
				else
				{
					temp = 1;
				}
			}
			break;
		case Get_tmr3:
			if (duration13 == 0)
			{
				temp = 0;
			}
			else
			{
				if (time < marker13)
				{
					temp = TIMERSIZE - marker13 + time;
				}
				else
				{
					temp = time - marker13;
				}
				if (temp > duration13)
				{
					duration13 = 0;
					temp = 0;
				}
				else
				{
					temp = 1;
				}
			}
			break;
	}
	return temp;
}

//Interrupt related functions

/******************************************************************************
 * GPIO Interrupt callback
 *
 * Fault pin, Chain reset pin and pushbutton pin trig on falling edge.
 * Clip_otw pin trig on both edges.
 * Fault is on PB3 which is on NVIC EXTI2-3 handler and is set to have higher
 * preemption priority than EXTI4-15 where CLIP_OTW and ADC watchdog are.
 *******************************************************************************
 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	static uint32_t spot;

	switch (GPIO_Pin){

		case FAULT_Pin:
			Supervision_Event = TAS_Fault;					//Fault Pin will cause immediate shutdown of TAS
			break;

		case CLIP_OTW_Pin:
			if (!HAL_GPIO_ReadPin(CLIP_OTW_GPIO_Port, CLIP_OTW_Pin))
			{
				//Launch a counter on falling edge in order to screen clip vs over-temperature condition
				TimeCounter(Launch_ctr3, 0);
				Supervision_Event = Clip_otw_active;
			}
			else if (TimeCounter(Get_ctr3,0) < CLIP_TRESHOLD)
			{
				//Clipping condition detected on rising edge of CLIP_OTW_Pin
				Supervision_Event = Clipping;
			}
			else if (Supervision_Event == Clip_otw_active)
			{
				//back normal condition restored on rising edge CLIP_OTW_Pin after over-temperature condition
				Supervision_Event = No_Supervision_event;
			}
			break;

		case CHAIN_RST_Pin:
			GPIO_Int_Off(CHAIN_RST_Pin);
			//Not implemented
			break;

		case PUSH_BUT_Pin:
			GPIO_Int_Off(PUSH_BUT_Pin);			//mask interrupt
			Ui_Event = Push_button_pressed;
			break;
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
	int i=0;
	bool match=false;
	while ((i < cmd_qty) && !match )
	{
		match = (rx_serial == serial_cmd[i]);
		i++;
	}
	if (match)
	{
		i--;
		SerialPutChar(serial_cmd[i]);
		Serial_PutString(": ");
		Serial_PutString(Serial_cmd_desc[i]);
		Serial_PutString(crlf);
		Serial_PutString(crlf);
		switch (serial_cmd[i])
		{
			case '?':
				Serial_Event = CommandList;
				break;
			case 'm':
				Serial_Event = MemoryDisplay;
				break;
			case 'l':
				Serial_Event = Memory2load;
				break;
			case 'e':
				Serial_Event = Memory2erase;
				break;
			case 'p':
				Serial_Event = Memory2play;
				break;
			case 'd':
				Serial_Event = Download;
				break;
			case 's':
				Serial_Event = Status;
				break;
			case 'b':
				Serial_Event = OutXbar1;
				break;
			case 'c':
				Serial_Event = OutXbar2;
				break;
			case '*':
				Serial_Event = VolInit;
				break;
			case '+':
				Serial_Event = VolUp;
				break;
			case '-':
				Serial_Event = VolDn;
				break;
		}
	}
	else
	{
		Serial_Event = Not_a_command;
	}

}

/******************************************************************************
 * ADC watchdog callback, called when ADC_Ch12 value < 0xC25
 *
 * Watchdog set to 0xC25 to trig interrupt if PSU_Monitor below 11,64V
 * 11,64V is min value of UTC LM2940-12 in order to avoid false triggering
 *
 * Max rate of this callback = ADC sampling rate = 100Hz
 * Rate of Timecounter Tick = 1000Hz -> Ratio max callback rate / Tick rate = 1/10
 *******************************************************************************
 */

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc)
{
	static int prev_slow_spot, slow_sum, fast_sum;

	uint32_t slow_spot = TimeCounter(Get_ctr1, 0);
	uint32_t fast_spot = TimeCounter(Get_ctr2, 0);

	if (slow_sum == 0)
	{
		slow_sum=1;
		TimeCounter(Launch_ctr1, 0);
		prev_slow_spot=0;
	}
	else if (slow_spot > (PSU_FAULT_RST_DLY + prev_slow_spot))
	{
		//reset both integrators when delay since last undervoltage is too large
		TimeCounter(Launch_ctr1, 0);
		prev_slow_spot=0;
		slow_sum=1;
		fast_sum=0;
	}
	else
	{
		slow_sum++;
		if (slow_spot > PSU_FAULT_LONG_DLY)
		//after long integration period, check number of undervoltage events
		{
			if ((slow_sum * 10) > (8 * slow_spot / 100)) 		//multiply by 10 to avoid using a float;
			//if more than 8 undervoltage per second
			{
				Supervision_Event = Vdd_drop;
			}
		}
		prev_slow_spot = slow_spot;
	}

	if (fast_sum == 0)
		{
			fast_sum=1;
			TimeCounter(Launch_ctr2, 0);
		}
	else
	{
		fast_sum++;
		if (fast_spot > PSU_FAULT_SHRT_DLY)
		//after short integration period , check number of undervoltage events
		{
			if ((fast_sum) > ((PSU_FAULT_SHRT_DLY/10)-1))
			{
				//Set fault if there's close to continuous undervoltage detection during the integration period (PSU_FAULT_SHRT_DLY)
				Supervision_Event = PSU_Fault;
			}
			//Restart a new integration period
			fast_sum=1;
			TimeCounter(Launch_ctr2, 0);

		}
	}
}

