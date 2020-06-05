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
 * Use some parts of common.c, module of ST AN2557 software package
 *
 * *****************************************************************************
*/


/**
  ******************************************************************************
  * @file    IAP/src/common.c 
  * @author  MCD Application Team
  * @version V3.3.0
  * @date    10/15/2010
  * @brief   This file provides all the common functions.
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

/* Includes ------------------------------------------------------------------*/
#include "neatamp.h"
#include "amp_board.h"
#include "ymodem.h"
#include "usart.h"
#include "gpio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern const char Board_ID[];
extern const char Version[];


const char msg1[]={"Command list"};
const char msg2[]={"Memory usage"};
const char msg3[]={"Preset to load"};
const char msg4[]={"Preset to play"};
const char msg5[]={"Preset to erase"};
const char msg6[]={"Download preset"};
const char msg7[]={"Status"};
const char msg8[]={"Out Crossbar L&R"};
const char msg9[]={"Out Crossbar L&Sub"};
const uint8_t serial_cmd[]={'?','m','l','p','e','d','s','b','c'};
const char *Serial_cmd_desc[]={msg1,msg2,msg3,msg4,msg5,msg6,msg7,msg8,msg9};
uint8_t cmd_qty = sizeof(serial_cmd);

const char bad_cmd[]={"Invalid command"};
const char *config_or_filter[]={" Config#     "," Filter Set# "};
const char a2cancel[]={"Press 'a' to cancel\r\n"};
const char textleft[]={"Left"};
const char textright[]={"Right"};
const char textsub[]={"Sub"};
const char textlr[]={"L&R"};
const char textlsub[]={"L&Sub"};


const char prompt={'>'};
const char crlf[]={0xD,0xA,0x00};
const char clrscr[]={0x1B,0x5B,0x32,0x4A,0x00};		//VT100 ED2 Code
const char home[]={0x1B,0x5B,0x48,0x00};			//VT100 cursorhome Code

const uint8_t Cmd_Bloc_Swap[]={
			0x07, 0x00,			//first 2 bytes are a uint16_t number of register/data couple in the array
			0x00, 0x00,
			0x7f, 0x8c,
			0x00, 0x23,
			0x14, 0x00,
			0x15, 0x00,
			0x16, 0x00,
			0x17, 0x01,
		};
const uint8_t Cmd_Bloc_Switch2b8c[]={
			0x02, 0x00,			//first 2 bytes are a uint16_t number of register/data couple in the array
			0x00, 0x00,
			0x7f, 0x8c,
		};
const uint8_t Cmd_Bloc_Switch2b00[]={
			0x02, 0x00,			//first 2 bytes are a uint16_t number of register/data couple in the array
			0x00, 0x00,
			0x7f, 0x00,
		};
const uint8_t Cmd_Bloc_Switch2p1e[]={
			0x01, 0x00,			//first 2 bytes are a uint16_t number of register/data couple in the array
			0x00, 0x1E,
		};
const uint8_t Cmd_Bloc_SubOn[]={
			0x07, 0x00,			//first 2 bytes are a uint16_t number of register/data couple in the array
			0x00, 0x00,
			0x7f, 0x8c,
			0x00, 0x1d,
			0x28, 0x00,
			0x29, 0x00,
			0x2a, 0x00,
			0x2b, 0x00,
	};


volatile Serial_Eventtd Serial_Event;
volatile Supervision_Eventtd Supervision_Event;
Ui_Eventtd Ui_Event;
Amp_Statusttd Amp_Status;

bool wait4command;

uint8_t rx_serial;
extern uint8_t file_name[];
extern uint8_t tas_status[];
extern uint8_t tas_status_siz;
extern volatile uint16_t ADC_Values[];

extern union {
	uint8_t page1[EEPROM_PAGESIZE];
	board_user_param_td boards_param;
	} user_param;

/* config_memory and filterset_memory store the preset memory for the TAS.
 *
 * There're 5 presets for the config and 5 for the filterset. They're organized the same way.
 * Each preset is defined by a record containing its name and its size in byte.
 * [name on 13 bytes][size in 2 bytes]. size is uint16_t.
 * 5 records are stacked : record0 at @0, record1 at @00+RECORDSIZE, etc..
 * After 5th record, at 5*RECORDSIZE, is stored Usage on a byte. Each bit of Usage is a flag.
 * Flag=1 -> corresponding preset has been loaded and is usable.
 * bit0 is for record0, bit4 for record4,...Memory #1 is record0
 * config memory is stored in pages 2 and 3 of EEPROM.
 * filterset memory is stored in pages 4 and 5 of EEPROM.
 *
 */
uint8_t config_memory[STORAGESIZE];
uint8_t filterset_memory[STORAGESIZE];

uint8_t filterset2load, config2load;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*
 *******************************************************************************************************
 * Function continuously called by main loop to handle serial communication events
 * Launch either a new command seek or a file upload
 *******************************************************************************************************
*/
void Manage_serial_event()
{
	uint32_t status,temp,temp1;
	static enum
	{
		Left_out,
		Right_out,
		LR_out,
		Sub_out,
		LSub_out,
	} out_crossbar = Right_out;
	char str[19];
	float gvdd;
	uint32_t coeff[6];

	switch(Serial_Event){
	case CommandLine:
		if (wait4command)
		{
			SerialPutChar(prompt);
			wait4command=false;								//allow following code to be executed only one time
			__HAL_UART_CLEAR_FLAG(&huart1,UART_FLAG_ORE);
			temp=USART1->RDR;								//clear RXNE and empty buffer
			HAL_UART_Receive_IT(&huart1, &rx_serial, 1);
		}
		break;

	case Download:
		Serial_Download();									//launch file upload
		Serial_Event = CommandLine;							//restore command line on way back from upload
		wait4command=true;
		break;

	case Memory2load:
		Display_memory();
		Serial_PutString("Choose preset to download to.\r\n\n");
		Serial_PutString(a2cancel);
		do
			{
				Serial_PutString("Choose Config memory (1-5) : ");
				__HAL_UART_CLEAR_FLAG(&huart1,UART_FLAG_ORE);
				temp=USART1->RDR;								//clear RXNE and empty buffer
				status=GetIntegerInput(&temp);
			}
		while ((temp <1 || temp >PRESETQTY ) && status);
		Serial_PutString(crlf);
		if (status) config2load = (uint8_t)temp;
		do
			{
				Serial_PutString("Choose Filter Set memory (1-5) : ");
				__HAL_UART_CLEAR_FLAG(&huart1,UART_FLAG_ORE);
				temp=USART1->RDR;								//clear RXNE and empty buffer
				status=GetIntegerInput(&temp);
			}
		while ((temp <1 || temp >PRESETQTY ) && status);
		Serial_PutString(crlf);
		if (status) filterset2load = (uint8_t)temp;
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		wait4command=true;
		break;

	case Memory2erase:
		Display_memory();
		Serial_PutString("Press C# or F# (C1, F5,...) to delete a preset\r\n");
		Serial_PutString("Everything else will cancel : ");
		GetInputString(str);
		if (str[0]=='C')
		{
			if (str[1] > '0' || str[1] < '6')
			{
				temp = str[1]-0x30;
				if (temp != user_param.boards_param.m_config)
				{
					config_memory[USAGE_IDX] &= ~(0x01 << (temp-1));
					Save_settings();
					EEPROM_Update_checksum();
				}
			}
		}
		else if (str[0]=='F')
		{
			if (str[1] > '0' || str[1] < '6')
			{
				temp = str[1]-0x30;
				if (temp != user_param.boards_param.m_filterset)
				{
					filterset_memory[USAGE_IDX] &= ~(0x01 << (temp-1));
					Save_settings();
					EEPROM_Update_checksum();
				}
			}
		}
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		wait4command=true;
		break;

	case Memory2play:
		Display_memory();
		Serial_PutString("Choose setup to use for TAS\r\n\n");
		do
			{
				Serial_PutString("Choose Config memory (1-5) : ");
				status=GetIntegerInput(&temp);
			}
		while ((temp <1 || temp >PRESETQTY ) && status);

		Serial_PutString(crlf);

		if (!((config_memory[USAGE_IDX] >> (temp-1)) & 0x01)) status=0;			//if preset choosed is empty, choice is canceled
		if (status)
			{
				do
					{
						Serial_PutString("Choose Filter Set memory (1-5) : ");
						status=GetIntegerInput(&temp1);
					}
				while ((temp1 <1 || temp1 >PRESETQTY ) && status);
				if (!((filterset_memory[USAGE_IDX] >> (temp1-1)) & 0x01)) status=0;	//if preset choosed is empty, choice is canceled
			}

		if (status)
		{
			user_param.boards_param.m_config = temp;
			user_param.boards_param.m_filterset = temp1;
			Serial_PutString(crlf);
			Save_settings();
			EEPROM_Update_checksum();
			TAS_Off();
			TAS_WR_Preset(user_param.boards_param.m_config, 'C');				//Apply config to TAS
			TAS_WR_Preset(user_param.boards_param.m_filterset, 'F');
			TAS_Send_cmdbloc(Cmd_Bloc_Swap);
			TAS_Set_Volume(User_Vol);						//Apply current volume
			TAS_On();
		}
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		wait4command=true;
		break;

	case Status:
		TAS_Get_Status();
		for (temp=0; temp < tas_status_siz; temp += 2)
		{
			Serial_PutString("Reg 0x");
			snprintf(str,12,"%2.2x: 0x%2.2x \r\n" , tas_status[temp], tas_status[temp+1]);
			Serial_PutString(str);
		}
		snprintf(str,18,"Volume:   0x%.2x\r\n", User_Vol );
		Serial_PutString(str);
		snprintf(str,18,"Balance:  0x%.2x\r\n", User_Bal );
		Serial_PutString(str);
		Serial_PutString("Pot:     0x");
		snprintf(str,7,"%.3x\r\n" , ADC_Values[0]);
		Serial_PutString(str);
		Serial_PutString("GVDD:  ");
		gvdd = ADC_Values[1]*0.003743;
		temp = (uint32_t)gvdd;
		temp1 = (uint32_t)(100*(gvdd-temp));
		snprintf(str,14,"%.2u.%.2u V\r\n",temp, temp1);		//allow not to use snprintf with a float
		Serial_PutString(str);
		snprintf(str,10,"Xbar:    ");
		switch(out_crossbar)
		{
			case Left_out:
			strcat(str,textleft);
			break;
			case Right_out:
			strcat(str,textright);
			break;
			case LR_out:
			strcat(str,textlr);
			break;
			case LSub_out:
			strcat(str,textlsub);
			break;
			case Sub_out:
			strcat(str,textsub);
			break;
		}
		Serial_PutString(str);
		Serial_PutString(crlf);
		GPIO_Int_Off(FAULT_Pin);
		HAL_GPIO_TogglePin(TAS_RST_GPIO_Port, TAS_RST_Pin);
		HAL_Delay(5);
		GPIO_Int_On(FAULT_Pin);
		Amp_Status = (HAL_GPIO_ReadPin(TAS_RST_GPIO_Port, TAS_RST_Pin)) ?  ItRocks : Ok_NoOut;
		wait4command=true;
		Serial_Event = CommandLine;
		Serial_PutString(crlf);

		//debug : enable Sub
		//TAS_Send_cmdbloc(Cmd_Bloc_SubOn);
		//TAS_Send_cmdbloc(Cmd_Bloc_Swap);

		break;

	case Not_a_command:
		SerialPutChar(rx_serial);
		Serial_PutString(crlf);
		Serial_PutString(bad_cmd);
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		wait4command=true;
		break;
		
	case CommandList:
		Serial_PutString("Key...Command");
		Serial_PutString(crlf);
		for (int i=0; i < cmd_qty;i++)
		{
			Serial_Draw_line(2, '.');
			SerialPutChar(serial_cmd[i]);
			Serial_Draw_line(3, 0x2E);
			SerialPutString(Serial_cmd_desc[i]);
			Serial_PutString(crlf);
		}
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		wait4command=true;
		break;
		
	case MemoryDisplay:
		Display_memory();
		Serial_Event = CommandLine;
		wait4command=true;
		break;

	case OutXbar1:
	case OutXbar2:
		if (Serial_Event == OutXbar1)
		{
			switch (out_crossbar)
			{
			case Left_out:
				out_crossbar = Right_out;
				break;
			case Right_out:
				out_crossbar = LR_out;
				break;
			default :
				out_crossbar = Left_out;
			}
		}
		else
		{
			switch (out_crossbar)
			{
			case Left_out:
				out_crossbar = Sub_out;
				break;
			case Sub_out:
				out_crossbar = LSub_out;
				break;
			default :
				out_crossbar = Left_out;
			}
		}
		Serial_PutString("  OutXBar <--  ");
		for(temp=0; temp < sizeof(coeff)/sizeof(coeff[0]); temp++) coeff[temp]=0;
		TAS_Send_cmdbloc(Cmd_Bloc_Switch2b8c);					//Change to Book 0x8C
		TAS_Send_cmdbloc(Cmd_Bloc_Switch2p1e);					//Change to Page 0x1E
				switch(out_crossbar)
		{
			case Left_out:
			Serial_PutString(textleft);
			coeff[0x00]=0x00008000;								//0x00800000 in little endian as coeff are stored in big endian
			break;
			case Right_out:
			Serial_PutString(textright);
			coeff[0x04]=0x00008000;
			break;
			case LR_out:
			Serial_PutString(textlr);
			coeff[0x00]=0x00008000;
			coeff[0x04]=0x00008000;
			break;
			case LSub_out:
			Serial_PutString(textlsub);
			coeff[0x00]=0x00008000;
			coeff[0x05]=0x00008000;
			break;
			case Sub_out:
			Serial_PutString(textsub);
			coeff[0x05]=0x00008000;
			break;
		}
		TAS_Write_Coeff(OUT_CROSSBAR_REG, coeff, 6);
		TAS_Send_cmdbloc(Cmd_Bloc_Swap);
		Serial_Event = CommandLine;
		Serial_PutString(crlf);
		Serial_PutString(crlf);
		wait4command=true;

	default:
		Serial_Event = CommandLine;
		wait4command=true;
		break;
	}
}
/*
 *******************************************************************************************************
 * Function continuously called by main loop to handle supervision events
 *
 *******************************************************************************************************
*/

void Manage_supervision_event()
{
	switch(Supervision_Event)
	{
	case Clip_otw_active:
		if (TimeCounter(Get_ctr3, 0) > (10 * CLIP_TRESHOLD))
		{
			//Over Temperature condition detected on rising edge of CLIP_OTW_Pin
			if (TimeCounter(Get_tmr3,0)==0)
				{

				if (User_Vol > 1)
					{
						User_Vol = User_Vol - 2;
						TAS_Set_Volume(User_Vol);
					}
					TimeCounter(Launch_tmr3, VOL_DEC_SPEED * 20);
					Ui_Event = Transient_Error;
				}
		}
		break;

	case Clipping:
		Ui_Event = Flash_on_clip;
		Supervision_Event = No_Supervision_event;
		break;

	case Vdd_drop:
		if (TimeCounter(Get_tmr3,0)==0)
			{
			if (User_Vol > 1)
				{
					User_Vol --;
					TAS_Set_Volume(User_Vol);
				}
				TimeCounter(Launch_tmr3, VOL_DEC_SPEED);
				Ui_Event = Transient_Error;
				Supervision_Event = No_Supervision_event;
			}
		break;

	case TAS_Fault:
	case I2C_Fault:
	case PSU_Fault:
		TAS_Off();
		HAL_GPIO_WritePin(DAC_MUTE_GPIO_Port, DAC_MUTE_Pin, RESET);
		Ui_Event = Fault;
		break;

	case No_Supervision_event:
		//Nothing
		break;
	}
}

/*
 *******************************************************************************************************
 * Function continuously called by main loop to handle User Interface events
 *
 *******************************************************************************************************
*/

void Manage_UI_event()
{
	static bool flash=false;
	static Amp_Statusttd Prev_Status;

	ENC_event_td Enc_test;
	//static uint16_t previous_pot_value;

	// Manage amp volume.
	// User_volume is within a 0->99 range
	// 0 = mute; 99 = max volume
	Enc_test=ENC_Manage();
	if (Enc_test != still)
		  {
			if (Enc_test == left)
				{
				 if (User_Vol > VOLUME_STEP ) User_Vol -= VOLUME_STEP;
				 else User_Vol=0;
				}
			else
				{
				if (User_Vol < 100-VOLUME_STEP ) User_Vol += VOLUME_STEP;
				else User_Vol = 100;
				}
			TAS_Set_Volume(User_Vol);
		  }
	switch (Ui_Event)
		{
		case Push_button_pressed:								//pressing button store current volume
				user_param.boards_param.m_volume = User_Vol;
				Save_settings();
				EEPROM_Update_checksum();
				Serial_PutString("\r\n * Current volume stored * \r\n\n");
				Ui_Event = Ack;
				wait4command=true;
				GPIO_Int_On(PUSH_BUT_Pin);						//Re enable interrupts
				//break;			//no break : continue on Ack
		case Ack:
			if ((flash==true) && (TimeCounter(Get_tmr1,0)==0))	//end of flash duration
			{
				flash=false;
				Amp_Status=Prev_Status;
				Ui_Event=No_UI_event;
			}
			else if (flash==false)								//start flash and launch timer
			{
				flash=true;
				Prev_Status=Amp_Status;
				if ((Amp_Status==Green) || (Amp_Status==Ok_NoOut)) Amp_Status=Blue;
				else Amp_Status=Green;
				TimeCounter(Launch_tmr1, FLASH_DURATION);
			}
			break;

		case Transient_Error:
			if ((flash==true) && (TimeCounter(Get_tmr1,0)==0))
			{
				flash=false;
				Amp_Status=Prev_Status;
				Ui_Event=No_UI_event;
			}
			else if (flash==false)
			{
				flash=true;
				Prev_Status=Amp_Status;
				Amp_Status=Red;
				TimeCounter(Launch_tmr1, 2*FLASH_DURATION);
			}
			break;

		case Flash_on_clip:
			if ((flash==true) && (TimeCounter(Get_tmr1,0)==0))
			{
				flash=false;
				Amp_Status=Prev_Status;
				Ui_Event=No_UI_event;
			}
			else if (flash==false)
			{
				flash=true;
				Prev_Status=Amp_Status;
				Amp_Status=Green;
				TimeCounter(Launch_tmr1, FLASH_DURATION/4);
			}
			break;

		case Fault:
			{
				HAL_GPIO_WritePin(DAC_MUTE_GPIO_Port, DAC_MUTE_Pin, RESET);
				HAL_GPIO_WritePin(TAS_RST_GPIO_Port, TAS_RST_Pin, RESET);
				Amp_Status = Shutdown;
			}
		break;

		case No_UI_event:
			//Nothing
			break;
		}

	//Refresh Led
	switch (Amp_Status)
		{
		case ItRocks:
		case Blue:
			GPIOB->ODR &= ~MASK_LED;
			GPIOB->ODR |= BLUE_LED;
			break;

		case White:
			GPIOB->ODR &= ~MASK_LED;
			GPIOB->ODR |= WHITE_LED;
			break;

		case Black:
			GPIOB->ODR &= ~MASK_LED;
			break;

		case Ok_NoOut:
		case Green:
			GPIOB->ODR &= ~MASK_LED;
			GPIOB->ODR |= GREEN_LED;
			break;

		case Red:
			GPIOB->ODR &= ~MASK_LED;
			GPIOB->ODR |= RED_LED;
			break;

		case Shutdown:
			GPIOB->ODR &= ~MASK_LED;
			GPIOB->ODR |= RED_LED;
			//Endless loop
			while(1)
			{
				HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
				HAL_GPIO_TogglePin(MCU_LED_GPIO_Port, MCU_LED_Pin);
				HAL_Delay(100);
			}
			break;
		}

	}


/*
 *******************************************************************************************************
 * Display content and usage of the presets memories
 *
 *******************************************************************************************************
*/
void Display_memory(void)
{
	uint32_t Usedflag;
	char c;
for (int i=0; i<2; i++)
	{
		Serial_PutString(config_or_filter[i]);
		Serial_PutString(" Free  Name\r\n");
		Serial_Draw_line(18+strlen(config_or_filter[i]), '-');
		Serial_PutString(crlf);

		Usedflag = (i==0) ? config_memory[USAGE_IDX] : filterset_memory[USAGE_IDX];
		for (int j=1; j <= PRESETQTY; j++)
		{
			Serial_Draw_line(5, ' ');
			SerialPutChar(j + 0x30);
			Serial_Draw_line(strlen(config_or_filter[i])-4, ' ');
			c = (Usedflag & 1) ? 'N':'Y';
			SerialPutChar(c);
			if (c=='N')					//Display name if Usedflag is true
			{
				Serial_Draw_line(4, ' ');
				if (i==0)
				{
					Serial_PutString(&config_memory[(j-1)*RECORDSIZE]);
					if (j == user_param.boards_param.m_config)
					{
						Serial_PutString("  ");
						Serial_PutString(" - Active");
					}
				}
				else
				{
					Serial_PutString(&filterset_memory[(j-1)*RECORDSIZE]);
					if (j == user_param.boards_param.m_filterset)
					{
						Serial_PutString("  ");
						Serial_PutString(" - Active");
					}
				}
			}
			Usedflag >>=1;
			Serial_PutString(crlf);
		}
		Serial_PutString(crlf);
	}
}
/*
 *******************************************************************************************************
 * Save in EEPROM one page of memory containing Config or Filterset data
 * Is called during file upload process
 * pagenumber is between 0 and the number of pages of a memory
 * memory is between 1 and the qty of memories (8)
 * type is ASCII F or C
 * buffer is where the data to save stand
 *******************************************************************************************************
*/
void Save_data_page(int pagenumber, uint8_t memory, int type, uint8_t* buffer)
{
	uint16_t page;
	if ( type=='C')
		{
			page = CONFIG1_PAADDR + ((memory-1) * CONFIG_SIZE) + pagenumber;
		}
	else if (type=='F')
		{
			page = FILTERSET1_PAADDR + ((memory-1) * FILTERSET_SIZE) + pagenumber;
		}
	EEPROM_WR_Page(page, buffer);
}

void Save_preset_memories(void)
{
	EEPROM_WR_Page(2, config_memory);
	EEPROM_WR_Page(3, config_memory+EEPROM_PAGESIZE);
	EEPROM_WR_Page(4, filterset_memory);
	EEPROM_WR_Page(5, filterset_memory+EEPROM_PAGESIZE);
}

void Save_settings(void)
{
	EEPROM_WR_Page(1,user_param.page1);
}

void Update_preset_name(uint8_t memory, char* name, int type)
{
	uint32_t usage;
	if (type == 'C')
		{
			memcpy(&config_memory[(memory-1)*RECORDSIZE],name,NAMESIZE);
			usage = 1 << (memory-1);
			config_memory[USAGE_IDX] |= (uint8_t)usage;
		}
	if (type == 'F')
		{
			memcpy(&filterset_memory[(memory-1)*RECORDSIZE],name,NAMESIZE);
			usage = 1 << (memory-1);
			filterset_memory[USAGE_IDX] |= (uint8_t)usage;
		}
}

void Update_preset_size(uint8_t memory, uint16_t* size, int type)
{
	if (type == 'C')
		{
			memcpy(&config_memory[((memory-1)*RECORDSIZE)+NAMESIZE], size, 2);
		}
	if (type == 'F')
		{
			memcpy(&filterset_memory[((memory-1)*RECORDSIZE)+NAMESIZE], size, 2);
		}
}


void Get_preset_size(uint8_t memory, uint16_t* size, int type)
	{
		if (type == 'C')
			{
				memcpy(size, &config_memory[((memory-1)*RECORDSIZE)+NAMESIZE],2);
			}
		if (type == 'F')
			{
				memcpy(size, &filterset_memory[((memory-1)*RECORDSIZE)+NAMESIZE],2);
			}
	}

void TAS_Send_cmdbloc(uint8_t* cmd)
{
	uint16_t cmdsize = *cmd | (*(cmd+1)<<8);
	for (int i=1; i <= cmdsize; i++)
	{
		TAS_Write_Register(&cmd[2*i]);
	}
}
/*
 * **************************************************************************************************
 * Set the playback volume using TAS Gain control register 0x3D & 0x3E in B0-P0
 *
 * Volume is between 0 and 99
 * 		0 is mute and 99 is Maximum gain, set by MAXGAINSETTING
 * Balance is between 0 and 40
 * 		0 is left, 20 is center, 40 is right *
 *
 * **************************************************************************************************
 */

void TAS_Set_Volume (uint8_t vol)
{
	uint8_t leftv, rightv;
	float slope,k=1;
	uint8_t cmddta[2];

	if (vol > 99) vol=99;

	k= (User_Bal > BAL_CENTER) ? (2 - User_Bal/BAL_CENTER) : 1;
	leftv = k * vol;
	k= (User_Bal < BAL_CENTER) ? (User_Bal/BAL_CENTER) : 1;
	rightv = k * vol;
	slope = (MAXGAINSETTING-255) / (float)99;

	TAS_Send_cmdbloc(Cmd_Bloc_Switch2b00);
	cmddta[0]=0x3D;
	cmddta[1]=255+slope*rightv;
	TAS_Write_Register(cmddta);
	cmddta[0]=0x3E;
	cmddta[1]=255+slope*leftv;
	TAS_Write_Register(cmddta);
}


/*
 *******************************************************************************************************
 * Initialize serial communication
 *******************************************************************************************************
*/
void Serial_Init(void)
{
	Serial_PutString(crlf);
	Serial_PutString(clrscr);
	Serial_PutString(home);
	Serial_PutString(Board_ID);
	Serial_PutString(Version);
	Serial_PutString(crlf);
	Serial_PutString(crlf);

	filterset2load = 0;
	config2load = 0;

	Serial_Event = CommandLine;
	wait4command=true;
}


/**
  * @brief  Download a file via serial port
  * @param  None
  * @retval None
  */
void Serial_Download(void)
{

	uint8_t Number[6]; 				//Max file size will be lower than 99999
	int32_t Size;
	uint16_t size,i;
	char Str[16];

  if ((filterset2load==0) || (config2load==0))
	  {
		  Serial_PutString("use 'l' command first.\n\n\r");
	  }
  else
	  {
	  SerialPutString("Load Config in #");
	  SerialPutChar(0x30+config2load);
	  Serial_PutString(crlf);
	  SerialPutString("Load Filter Set in #");
	  SerialPutChar(0x30+filterset2load);
	  Serial_PutString(crlf);
	  SerialPutString("Waiting for YMODEM upload. Press 'a' to abort\n\r");
	  Size = Ymodem_Receive();
	  Serial_PutString(crlf);
	  Serial_PutString(crlf);
	  Serial_PutString(crlf);
	  if (Size > 0)
	  {
		  Ui_Event = Ack;						//flash led to ack
		  SerialPutString("\n\n\r TAS3251 setup file successfully loaded\n\r");
		  Serial_Draw_line(37,'-');
		  SerialPutString("\r\n Name: ");
		  SerialPutString(file_name);
		  SerialPutString("\n\r Size: ");
		  snprintf(Str, 14, "%u Bytes\r\n", Size);
		  SerialPutString(Str);

		  SerialPutString(" Size of Config data : ");
		  Get_preset_size(config2load, &size, 'C');
		  snprintf(Str, 14,"%u Bytes\r\n", size);
		  SerialPutString(Str);

		  SerialPutString(" Size of FilterSet data : ");
		  Get_preset_size(filterset2load, &size, 'F');
		  snprintf(Str, 14,"%u Bytes\r\n", size);
		  SerialPutString(Str);
		  Serial_Draw_line(37,'-');
		  SerialPutString(crlf);
	  }
		  else if (Size == -1)
		  {
			SerialPutString("\n\n\rFile too big\n\r");
		  }
		  else if (Size == -2)
		  {
			SerialPutString("\n\n\rVerification failed\n\r");
		  }
		  else if (Size == -3)
		  {
			SerialPutString("\r\n\nAborted\n\r");
		  }
		  else
		  {
			SerialPutString("\n\rReceive error\n\r");
		  }
	  }
}


/**
  * @brief  Test to see if a key has been pressed on the Terminal
  * @param  key: The key pressed
  * @retval 1: Correct
  *         0: Error
  */
uint32_t SerialKeyPressed(uint8_t *key)
{

	if (__HAL_UART_GET_FLAG(&huart1,UART_FLAG_RXNE) != RESET)
	  {
		*key = (uint8_t)USART1->RDR;
		return 1;
	  }
	  else
	  {
		return 0;
	  }
}

/**
  * @brief  Get a key from the Terminal
  * @param  None
  * @retval The Key Pressed
  */
uint8_t GetKey(void)
{
  uint8_t key = 0;

  /* Waiting for user input */
  while (1)
  {
    if (SerialKeyPressed((uint8_t*)&key)) break;
  }
  return key;

}

/**
  * @brief  Print a character on the HyperTerminal
  * @param  c: The character to be printed
  * @retval None
  */
void SerialPutChar(uint8_t c)
{
  HAL_UART_Transmit(&huart1, &c, 1, 100);
  }

/**
  * @brief  Print a string on the HyperTerminal
  * @param  s: The string to be printed
  * @retval None
  */
void Serial_PutString(uint8_t *s)
{
  while (*s != '\0')
  {
    SerialPutChar(*s);
    s++;
  }
}

void Serial_Draw_line(uint8_t nbr, char c)
{
	while (nbr > 0)
	{
		SerialPutChar(c);
		nbr--;
	}
}

/**
  * @brief  Get an integer from the HyperTerminal
  * @param  num: The integer
  * @retval 1: Correct
  *         0: Error
  */
uint32_t GetIntegerInput(int32_t * num)
{
	uint8_t inputstr[16];
	__HAL_UART_CLEAR_FLAG(&huart1,UART_FLAG_ORE);
	inputstr[0]=USART1->RDR;								//clear RXNE and empty buffer
	while (1)
	{
		GetInputString(inputstr);
		if (inputstr[0] == '\0') continue;
		if ((inputstr[0] == 'a' || inputstr[0] == 'A') && inputstr[1] == '\0')
		{
			SerialPutString("User Cancelled \r\n");
			return 0;
		}

		if (Str2Int(inputstr, num) == 0)
		{
			SerialPutString("Error, Input again: \r\n");
		}
		else
		{
			return 1;
		}
	}
}

/**
  * @brief  Get Input string from the HyperTerminal
  * @param  buffP: The input string
  * @retval None
  */
void GetInputString (uint8_t * buffP)
{
  uint32_t bytes_read = 0;
  uint8_t c = 0;
  do
  {

	  c = GetKey();
    if (c == '\r')
      break;
    if (c == '\b') /* Backspace */
    {
      if (bytes_read > 0)
      {
        SerialPutString("\b \b");
        bytes_read --;
      }
      continue;
    }
    if (bytes_read >= CMD_STRING_SIZE )
    {
      SerialPutString("Too long input\r\n");
      bytes_read = 0;
      continue;
    }
    if (c >= 0x20 && c <= 0x7E)
    {
      buffP[bytes_read++] = c;
      SerialPutChar(c);
    }
  }
  while (1);
  SerialPutString(("\n\r"));
  buffP[bytes_read] = '\0';
}

/**
  * @brief  Convert an Integer to a string
  * @param  str: The string
  * @param  intnum: The intger to be converted
  * @retval None
  */

uint8_t HexStr2Byte (uint8_t *inputstr)
{
	return ((CONVERTHEX(inputstr[0]) << 4) + CONVERTHEX(inputstr[1]));
}

/**
  * @brief  Convert a string to an integer
  * @param  inputstr: The string to be converted
  * @param  intnum: The integer value
  * @retval 1: Correct
  *         0: Error
  */
uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
  uint32_t i = 0, res = 0;
  uint32_t val = 0;

  if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
  {
    if (inputstr[2] == '\0')
    {
      return 0;
    }
    for (i = 2; i < 11; i++)
    {
      if (inputstr[i] == '\0')
      {
        *intnum = val;
        /* return 1; */
        res = 1;
        break;
      }
      if (ISVALIDHEX(inputstr[i]))
      {
        val = (val << 4) + CONVERTHEX(inputstr[i]);
      }
      else
      {
        /* return 0, Invalid input */
        res = 0;
        break;
      }
    }
    /* over 8 digit hex --invalid */
    if (i >= 11)
    {
      res = 0;
    }
  }
  else /* max 10-digit decimal input */
  {
    for (i = 0;i < 11;i++)
    {
      if (inputstr[i] == '\0')
      {
        *intnum = val;
        /* return 1 */
        res = 1;
        break;
      }
      else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
      {
        val = val << 10;
        *intnum = val;
        res = 1;
        break;
      }
      else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
      {
        val = val << 20;
        *intnum = val;
        res = 1;
        break;
      }
      else if (ISVALIDDEC(inputstr[i]))
      {
        val = val * 10 + CONVERTDEC(inputstr[i]);
      }
      else
      {
        /* return 0, Invalid input */
        res = 0;
        break;
      }
    }
    /* Over 10 digit decimal --invalid */
    if (i >= 11)
    {
      res = 0;
    }
  }

  return res;
}
