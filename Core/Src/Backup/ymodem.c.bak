/*
 * *****************************************************************************
 * NeatAmp software
 *
 * ymodem.c
 *
 * Author :  AIM65
 * Date : 12 Avr 2020
 * (c) AIM65
 *
 * Based on ymodem.c,  module of ST AN2557 software package
 * Do not work with "Tera TERM" terminal emulator; work fine with "PuTTY" : www.extraputty.com
 *
 * *****************************************************************************
*/

/**
  ******************************************************************************
  * @file    IAP/src/ymodem.c 
  * @author  MCD Application Team
  * @version V3.3.0
  * @date    10/15/2010
  * @brief   This file provides all the software functions related to the ymodem 
  *          protocol.
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
#include "ymodem.h"
#include "neatamp.h"
#include "amp_board.h"
#include <stdbool.h>


/* Private typedef -----------------------------------------------------------*/
enum parser_status_en{
	Searching_cmd,
	Parsing_cmd,
	Found_cmd,
};

enum command_lst_en{			//declaration order must be the same as uint8_t Parser_Tag[CMD_QTY][CMD_SIZE+1]
	CONFIGNAME,
	FILTERNAME,
	WRITECOEF,					//used by 2.0 speaker config
	WRITECOEFALIAS,				//used by 2.1 & 1.1 speaker config
	SUBONSTART,
	CKTASSTART,
	CFGREG,
	CONFIRMCFGREG,
	REGADDR,
	SWAPCOMMAND,
	EMPTYLINE,
	NOCOMMAND,
};

enum file_process_st_en{
	wait4data,
	load_ppc3_config,
	Confirmload_ppc3_config,
	load_ppc3_filter,
	Drop_ppc3_swap,
};

/* Private define ------------------------------------------------------------*/

#define CR					0x0D
#define LF					0x0A

#define TAG_SIZE 			4		//size in char of tag to detect in the .h file
#define TAG_QTY 			10		//number of know tag to detect


#define LINE_ACQUIRE_SIZE 	20

#define SEARCH				0
#define NOMATCH				1
#define MATCH				2
#define ONGOING				3


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t file_name[FILE_NAME_LENGTH];
//uint16_t PageSize = MAX_FILE_SIZE;
//uint EraseCounter = 0x0;
//uint NbrOfPage = 0;

uint8_t Parser_Tag[TAG_QTY][TAG_SIZE+1]={
		{'@','c','f','n',},			//config name
		{'@','f','l','n',},			//filter name
		{'/','/','w','r',},			//detect write coeff from ppc3 file (used by 2.0 speaker config PPC3 .h file)
		{'/','/','C','o',},			//detect write coeff from ppc3 file (used by 1.1 or 2.1 speakar config PPC3 .h file)
		{'Y','M',' ','h',},			//detect start of data chunk aimed at setting on subwoofer channel
		{'/','/','S','a',},			//detect start of data chunk aimed at setting TAS clocking
		{'c','f','g','_',},			//cfg_reg registers from ppc3 file
		{'/','p','r','o',},			//Confirm cfg_reg to avid false detection
		{'{',' ','0','x',},			//register address from ppc3 file
		{'/','/','s','w',}			//swap command
		};

enum parser_status_en parser_state, current_char_parsed, parse_activity;

enum {lineparse,cr,eol,eoel} lineparser_status;

extern uint8_t filterset2load, config2load;


/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Receive byte from sender
  * @param  c: Character
  * @param  timeout: Timeout
  * @retval 0: Byte received
  *         -1: Timeout
  */
static  int Receive_Byte (uint8_t *c, uint timeout)
{
  while (timeout-- > 0)
  {
    if (SerialKeyPressed(c) == 1)
    {
      return 0;
    }
  }
  return -1;
}

/**
  * @brief  Send a byte
  * @param  c: Character
  * @retval 0: Byte sent
  */
static uint Send_Byte (uint8_t c)
{
  SerialPutChar(c);
  return 0;
}

/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  timeout
  * @param  length
  *     	 0: end of transmission
  *    		-1: abort by sender
  *    		>0: packet length
  * @retval 0: normally return
  *        -1: timeout or packet error
  *         1: abort by user
  */
static int Receive_Packet (uint8_t *data, int *length, uint timeout)
{
  uint16_t i, packet_size;
  uint8_t c;
  *length = 0;
  if (Receive_Byte(&c, timeout) != 0)
  {
    return -1;
  }
  switch (c)
  {
    case SOH:
      packet_size = PACKET_SIZE;
      break;
    case STX:
      packet_size = PACKET_1K_SIZE;
      break;
    case EOT:
      return 0;
    case CA:
      if ((Receive_Byte(&c, timeout) == 0) && (c == CA))
      {
        *length = -1;
        return 0;
      }
      else
      {
        return -1;
      }
    case ABORT1:
    case ABORT2:
      return 1;
    default:
      return -1;
  }
  *data = c;
  for (i = 1; i < (packet_size + PACKET_OVERHEAD); i ++)
  {
    if (Receive_Byte(data + i, timeout) != 0)
    {
      return -1;
    }
  }
  if (data[PACKET_SEQNO_INDEX] != ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
  {
    return -1;
  }
  *length = packet_size;
  return 0;
}

/**
  * @brief  Receive a file using the ymodem protocol
  * @param
  * @retval The size of the file
  */
int Ymodem_Receive (void)
{
  uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD], file_size[FILE_SIZE_LENGTH], *file_ptr;
  int packet_length, session_done, file_done, packets_received, errors, session_begin;
  int size=0;
  int x,y, byte_ctr, in_pckt_idx, linelength, inline_idx = 0;
  int eeprom_flt_page_ctr = 0, eeprom_cfg_page_ctr = 0, inpage_flt_idx = 0, inpage_cfg_idx = 0 ;
  uint8_t i2creg, i2cdta, type, write_cfg_buffer[EEPROM_PAGESIZE], write_flt_buffer[EEPROM_PAGESIZE], mem2load;
  uint16_t config_bytes,filterset_bytes;
  bool first_coeff_line = false;

  char linebuffer[LINE_ACQUIRE_SIZE];			//no need to store the whole 80 char line
  char instring[NAMESIZE];


  enum command_lst_en command;
  enum file_process_st_en process_status = wait4data;



  /* Initialize FlashDestination variable */
  //

  for (session_done = 0, errors = 0, session_begin = 0; ;)
  {
    for (packets_received = 0, file_done = 0; ;)
    {
      	switch (Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT))
      {
        case 0:
          errors = 0;
          switch (packet_length)
          {
            /* Abort by sender */
            case -1:
              Send_Byte(ACK);
              return 0;
            /* End of transmission */
            case 0:
              Send_Byte(ACK);
              file_done = 1;
              break;
            /* Normal packet */
            default:
              if ((packet_data[PACKET_SEQNO_INDEX] & 0xff) != (packets_received & 0xff))
              {
                Send_Byte(NAK);
              }
              else
              {
                if (packets_received == 0)
                {
                  /* Filename packet */
                  if (packet_data[PACKET_HEADER] != 0)
                  {
                    /* Filename packet has valid data */
                    for (x = 0, file_ptr = packet_data + PACKET_HEADER; (*file_ptr != 0) && (x < FILE_NAME_LENGTH);)
                    {
                      file_name[x++] = *file_ptr++;
                    }
                    file_name[x++] = '\0';
                    for (x = 0, file_ptr ++; (*file_ptr != ' ') && (x < FILE_SIZE_LENGTH);)
                    {
                      file_size[x++] = *file_ptr++;
                    }
                    file_size[x++] = '\0';
                    Str2Int(file_size, &size);

                    /* Test the size of the image to be sent */
                    /* Image size is greater than Flash size */
                    if (size > (MAX_PPC3_FILE_SIZE - 1))
                    {
                      /* End session */
                      Send_Byte(CA);
                      Send_Byte(CA);
                      return -1;
                    }
                    /* Here Ymodem header packet has been received 		*/
                    /* File name and size are known .					*/
                    /* Set initial values of the command parser			*/
                    /* ACK packet and proceed to transfer 				*/
                    config_bytes=0;
                    filterset_bytes=0;
                    eeprom_flt_page_ctr=0;
                    eeprom_cfg_page_ctr=0;
                    inpage_flt_idx=0;				//Initialize the index used to store filter data in the write buffer of eeprom
                    inpage_cfg_idx=0;				//Initialize the index used to store config data in the write buffer of eeprom
                    byte_ctr=0;						//initialize byte counter used by file processor here-under
                    lineparser_status = eol;		//Initialize the state machine used to load entire lines in linebuffer
                    process_status = wait4data;		//Initialize the state machine used to process commands in the file
                    instring[NAMESIZE-1]='\0';
                    memcpy(instring,file_name,NAMESIZE-1);				//file name stored in EEPROM will be truncated to NAMESIZE-1
                    Update_preset_name(config2load, instring,'C');		//Set default name
                    Update_preset_name(filterset2load, instring,'F');	//Set default name

                    Send_Byte(ACK);					//ack packet
                    Send_Byte(CRC16);
                  }
                  /* Filename packet is empty, end session */
                  else
                  {
                    Send_Byte(ACK);
                    file_done = 1;
                    session_done = 1;
                    break;
                  }
                }

                /* Handle here Data packet */
                else
                {
                 	for (in_pckt_idx = PACKET_HEADER; in_pckt_idx < packet_length + PACKET_HEADER; in_pckt_idx++)	//go through all char from packet
                  	{
						if (lineparser_status == eol || lineparser_status == eoel)				//eol : end of line, eoel : end of empty line
							{
								inline_idx=0;													//start a new line
								lineparser_status = lineparse;
							}
						if (packet_data[in_pckt_idx] == CR)
							{
								lineparser_status = cr;
							}
						else
							if (lineparser_status == cr && packet_data[in_pckt_idx] == LF)
							{
								lineparser_status = (inline_idx == 1) ? eoel : eol;				//line acquired correctly but dropped if empty

							}
						if (inline_idx < LINE_ACQUIRE_SIZE && lineparser_status == lineparse)	//load line in buffer (firsts chars only)
							{
								linebuffer[inline_idx] = packet_data[in_pckt_idx];
							}
						inline_idx++;
						byte_ctr++;
						if (inline_idx == 80) inline_idx=0;		//avoid too long line as valid data of TI PPC3 file are at the beginning of the line

						if ((lineparser_status != eol) && ((lineparser_status != eoel))) continue;	//back to main for loop in order to acquire a full line

						/* Once line is acquired, process it to seek for commands */
						linelength = inline_idx - 2;    					//remove the two last char acquired : CR LF
						if (lineparser_status == eoel)
						{
							command = EMPTYLINE;
						}
						else if (linelength < TAG_SIZE)						//line with length < size of a command couldn't contain a command
						{
							command = NOCOMMAND;
						}
						else
						{													//here,parse the line for registered Tag
							command = NOCOMMAND;
							for (inline_idx=0 ; inline_idx < linelength-TAG_SIZE+1; inline_idx++)	//test each char of the buffer
							{
								for (y=0; y < TAG_QTY; y++)	Parser_Tag[y][TAG_SIZE] = SEARCH;		//Restart a search, set search flag of each command in the array
								for (x=0; x < TAG_SIZE; x++)										//test for each char of the line if a command begin
								{
									for (y=0; y < TAG_QTY; y++)										//Test the char across the command array
									{
										if(Parser_Tag[y][TAG_SIZE] == SEARCH)
										{
											if (linebuffer[inline_idx+x] == Parser_Tag[y][x])		//is the character part of a registered command ?
											{
												if ( x == TAG_SIZE-1)								//is last character of command reach ?
												{
													command = y;									//last character matched, command identified
												}
											}
											else Parser_Tag[y][TAG_SIZE] = NOMATCH;
										}
									}
								if (command != NOCOMMAND) break;						// a command is identified, exit from 2nd loop
								}
							if (command != NOCOMMAND) break;							// a command is identified, exit from 1st loop
							}
						}
						inline_idx += TAG_SIZE;											//point to next character in the line

						/* Check if a command as been identified, in order to process it or seek for a new line */
						x=0;
						switch (command)
						{
							case CONFIGNAME:										// '@cfn' command. Set the name of the Config
								y = linelength-(inline_idx+1);
								if (y > 1)
								{
									for (x=0; (x < y) && (x < NAMESIZE-1); x++)		//size of config and filterset names is NAMESIZE
									{
										instring[x] = linebuffer[inline_idx+x+1];	//remove the space between the command and the data
									}
								}
								instring[x]='\0';
								Update_preset_name(config2load, instring,'C');
								break;

							case FILTERNAME:										// '@fln' Tag. Set the name of the FilterSet
								y = linelength-(inline_idx+1);
								if (y > 1)
								{
									for (x=0; (x < y) && (x < NAMESIZE-1); x++)		//size of config and filterset names is NAMESIZE
									{
										instring[x] = linebuffer[inline_idx+x+1];	//remove the space between the command and the data
									}
								}
								instring[x]='\0';
								Update_preset_name(filterset2load, instring,'F');
								break;

							case SUBONSTART:										// 'YM h' Tag
							case WRITECOEFALIAS :									// '//Co' Tag
								process_status = load_ppc3_filter;
								first_coeff_line = true;
								break;

							case WRITECOEF:											// '//wr' Tag
								process_status = load_ppc3_filter;
								break;

							case CKTASSTART:										// '//Sa' Tag
								if (process_status == load_ppc3_filter) process_status= load_ppc3_config;
								break;

							case CFGREG:											// 'cfg_' command
								if ((process_status != wait4data) && (process_status != load_ppc3_config)) break;
								process_status = Confirmload_ppc3_config;
								break;

							case CONFIRMCFGREG:
								if (process_status == Confirmload_ppc3_config) process_status = load_ppc3_config;
								else process_status = wait4data;
								break;

							case REGADDR:											// '{  0' command
								if (!((process_status == load_ppc3_config) || (process_status == load_ppc3_filter))) break;
								// get to data from the line buffer and convert them in two bytes, one i2creg, second i2cdta
								memcpy  (instring, &linebuffer[inline_idx],2);		//Get first byte which is register
								i2creg = HexStr2Byte ((uint8_t*) instring);
								memcpy  (instring, &linebuffer[inline_idx+6],2);	//Get second byte which is data
								i2cdta = HexStr2Byte ((uint8_t*) instring);

								if (process_status == load_ppc3_config)
									{
										config_bytes +=2;								//count the number of config bytes to store
										mem2load = config2load;
										type = 'C';
										write_cfg_buffer[inpage_cfg_idx] = i2creg;		//transfer processed data to the eeprom write buffer
										write_cfg_buffer[inpage_cfg_idx+1] = i2cdta;
										inpage_cfg_idx +=2;
										if (inpage_cfg_idx == EEPROM_PAGESIZE)			//check if end of page reached
										{
											Save_data_page(eeprom_cfg_page_ctr, mem2load, type, write_cfg_buffer);
											inpage_cfg_idx=0;
											eeprom_cfg_page_ctr++;
										}
									}
									else
										{
										filterset_bytes +=2;							//count the number of filter bytes to store
										mem2load = filterset2load;
										type = 'F';
										write_flt_buffer[inpage_flt_idx] = i2creg;		//transfer processed data to the eeprom write buffer
										write_flt_buffer[inpage_flt_idx+1] = i2cdta;
										inpage_flt_idx +=2;
										if (inpage_flt_idx == EEPROM_PAGESIZE)			//check if end of page reached
										{
											Save_data_page(eeprom_flt_page_ctr, mem2load, type, write_flt_buffer);
											inpage_flt_idx=0;
											eeprom_flt_page_ctr++;
										}
										}
								break;

							case EMPTYLINE:		//Emptyline is end of FilterSet bloc or end of drop swap
								if (process_status == Drop_ppc3_swap) process_status = load_ppc3_config;	//back to upper level command
								else if (process_status == load_ppc3_filter)
									{
									if (first_coeff_line != true) process_status = load_ppc3_config;
									else first_coeff_line = false;
									}
								else if (process_status == Confirmload_ppc3_config) process_status = wait4data;
								break;

							case SWAPCOMMAND:										//  '//sw' command
								if ((process_status == load_ppc3_config) || (process_status == load_ppc3_filter))
								{													//Swap command has to be dropped from memory
									process_status = Drop_ppc3_swap;				//because it is executed by the software
								}													//after a load filterset operation
								break;

							case NOCOMMAND:
								if (process_status == Confirmload_ppc3_config) process_status = wait4data;
								break;
						}

				/* End of the for loop which handle data packet */
                }
                Send_Byte(ACK);									//Ack the received packet

                }
                packets_received ++;
                session_begin = 1;
           	  }
      	  }
          break;
        case 1:
          Send_Byte(CA);
          Send_Byte(CA);
          return -3;
        default:
          if (session_begin > 0)
          {
            errors ++;
          }
          if (errors > MAX_ERRORS)
          {
            Send_Byte(CA);
            Send_Byte(CA);
            return 0;
          }
          Send_Byte(CRC16);		//continuously send C while waiting transfer to start
          break;
      }
      if (file_done != 0)
      {
        break;
      }
    }
    if (session_done != 0)
    {
      break;
    }
  }
//Terminate transfer
//write to eeprom the last page of the 2 buffers : config and filter set

	if (inpage_flt_idx != 0)							//Check if it remains some data in buffer
	{
		for (x = inpage_flt_idx; x < EEPROM_PAGESIZE; x++)
			{
				write_flt_buffer[x] = 0;				//fill end of buffer with zeroes
			}
		mem2load = filterset2load;
		type = 'F';
		Save_data_page(eeprom_flt_page_ctr, mem2load, type, write_flt_buffer);
	}
	if (inpage_cfg_idx != 0)								//Check if it remains some data in buffer
	{
		type = 'C';
		for (x = inpage_cfg_idx; x < EEPROM_PAGESIZE; x++)
			{
				write_cfg_buffer[x] = 0;				//fill end of buffer with zeroes
			}
		mem2load = config2load;
		Save_data_page(eeprom_cfg_page_ctr, mem2load, type, write_cfg_buffer);
	}

	Update_preset_size(filterset2load, &filterset_bytes, 'F');
	Update_preset_size(config2load, &config_bytes, 'C');
	Save_preset_memories();
	EEPROM_Update_checksum();
  return (int)size;
}

