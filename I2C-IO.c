/*
 * I2C-IO.c
 *
 *  Created on: Jul 24, 2013
 *      Author: brad
 */


#include <stdint.h>
#include <stdio.h>

#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "I2C-IO.h"
#include "console.h"

#define I2C_IO_PORT GPIOB
#define I2C_SDA_PIN GPIO_Pin_8 // these are back to front.. when changed it doesnt work for some reason.. too tired to work out
#define I2C_CLK_PIN GPIO_Pin_9 // No theyre not.. its only used in init... swap and try it!

#define I2C_POWER_PORT GPIOA
#define I2C_POWER_PIN GPIO_Pin_1

#define FAIL -1
#define PASS 1

xQueueHandle xI2C_SendQueue;

static int iI2C_Send(char address, char data);


void vI2C_Init(void)
{
	portENTER_CRITICAL(); //added in the case that this is interrupted by a call to send on another task. (somehow)
	static int first = 1;
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStructure;

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = I2C_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD; // Alt Function - Open Drain
	GPIO_Init(I2C_IO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = I2C_CLK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD; // Alt Function - Open Drain
	GPIO_Init(I2C_IO_PORT, &GPIO_InitStructure);

	I2C_Cmd(I2C1, DISABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // Enable GPIOB Clock
	GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE); // Remap the I2C1 pins.
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

	I2C_DeInit(I2C1 );
	I2C_StructInit(&I2C_InitStructure);
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_ClockSpeed = 10000; //This Doesnt work.. ???
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x00;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_Init(I2C1, &I2C_InitStructure);
	I2C_Cmd(I2C1, ENABLE);

	I2C_ITConfig(I2C1, I2C_IT_EVT, ENABLE);

	// Creates a queue for the send task.
	if (first == 1)
		xI2C_SendQueue = xQueueCreate(20, sizeof(uint16_t));
	first = 0;

	// set up power pin
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	  GPIO_InitStructure.GPIO_Pin =  I2C_POWER_PIN;
	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	  GPIO_Init( I2C_POWER_PORT, &GPIO_InitStructure );
	  GPIO_SetBits(I2C_POWER_PORT, I2C_POWER_PIN); //turn on

	portEXIT_CRITICAL();
}

 void vI2C_RecyclePower()
{
	  GPIO_ResetBits(I2C_POWER_PORT, I2C_POWER_PIN); // turn off
	vTaskDelay(2000);
	  GPIO_SetBits(I2C_POWER_PORT, I2C_POWER_PIN); // turn on
	  vTaskDelay(1000);
	  vI2C_Init();
	  vConsolePrint("I2C reset and init'd\r\n\0");
}

static int iI2C_Send(char address, char data)
{
	long unsigned int i = 0, j;
	int retval = 0;
	portENTER_CRITICAL();
	i = 0;
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	I2C_GenerateSTART(I2C1, ENABLE);

	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter );

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;
	I2C_SendData(I2C1, data);

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	I2C_GenerateSTOP(I2C1, ENABLE);

	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	portEXIT_CRITICAL();
	return 1;
}
//typedef enum
//{
//	I2C_RECEIVE_ERROR_NO_RESPONSE,
//	I2C_RECEIVE_ERROR_RECEIVER_MODE_NOT_ACKNOWLEDGED,
//	I2C_RECEIVE_ERROR_MASTER_BYTE_RECEIVED_EVENT_NOT_RECEIVED,
//	I2C_RECEIVE_ERROR
//};

static int iI2C_Receive(char address, char * data)
{
	char received = 0;
	long unsigned int i = 0;
	portENTER_CRITICAL();

	I2C_AcknowledgeConfig(I2C1, DISABLE); // We are receiving only 1 byte from the PCF8574

	I2C_GenerateSTART(I2C1, ENABLE);
	i = 0; // to be safe
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	I2C_Send7bitAddress(I2C1, address, I2C_Direction_Receiver );

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;
	received = I2C_ReceiveData(I2C1 );

	I2C_GenerateSTOP(I2C1, ENABLE);

	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY ))
	{
		i++;
		if (i >= 7200)
		{
			portEXIT_CRITICAL();
			return -1;

		}
	}
	i = 0;

	*data = received;

	portEXIT_CRITICAL();
	return 1;
}

void vI2C_TestTask(void *pvParameters)
{
	uint16_t uTestMessage = 0x0000;
	uint8_t uAddress, uData, uRData = 3, uDirection = 0;
	uAddress = 0x70;
	int em = 0;
	char input1;
	static char flag = 0;
	vTaskDelay(5000);
	for (;;)
	{
		uData = 0x03;
		uTestMessage = (uint16_t) (uAddress << 8) & 0xFF00;
		uTestMessage = uTestMessage | (uint16_t) (uData & 0x000F);
		if (uDirection)
			uTestMessage |= 1 << 4;
		vConsolePrint("I2C Test Task Start\r\n\0");
		fflush(stdout);

		printf("===================\r\n\0");
		fflush(stdout);
		em = iI2C_Receive(uAddress, &uRData);
		if (em == -1)
		{
			printf("failed %d", em);
			//xQueueSendToBack(xI2C_SendQueue, &uTestMessage, 100);
			printf("failed, sending again\r\n\0");
			fflush(stdout);
		}

		printf("received = %x\r\n\0", uRData);
		input1 = cI2cGetInput(PORTV, 1);
		printf("pin 1 low? %d\r\n\0", input1);
		input1 = cI2cGetInput(PORTV, 2);
		printf("pin 2 low? %d\r\n\0", input1);
		input1 = cI2cGetInput(PORTV, 3);
		printf("pin 3 low? %d\r\n\0", input1);
		input1 = cI2cGetInput(PORTV, 4);
		printf("pin 4 low? %d\r\n\0", input1);

		vConsolePrint("\r\n\0******I2C Test Task END\r\n\0");

		fflush(stdout);
		vTaskDelay(1000);

	}

}

void vI2C_SendTask(void *pvParameters)
{
	portBASE_TYPE xStatus = pdFAIL;
	int iRCVStatus = PASS;
	int iRcvResetStatus = FAIL;
	int iSndResetStatus = PASS;
	int iSNDStatus = PASS;
	uint16_t message;
	static uint8_t cnt = 0;
	uint8_t uAddress = 0x00, uBitNum = 0x00, uCurrent = 0x00, uToSend = 0x00, uDirection = 0xFF;
	static char pcPrintBuf[80];

	for (;;)
	{
		xStatus = xQueueReceive(xI2C_SendQueue, &message, portMAX_DELAY);
		if (xStatus == pdPASS)
		{
			uAddress = message >> 8;
			uBitNum = (uint8_t) message & (0x0F);
			if (uBitNum > 7 || uBitNum <= 0)
				uBitNum = 0;
			uDirection = ((uint8_t) message & 0xF0) >> 4;

			// RECEIVE FROM
			// vConsolePrint("Trying to RECEIVE I2C\r\n\0");


				iRCVStatus = iI2C_Receive(uAddress, &uCurrent);
				while ((iRCVStatus == FAIL) )
				{
					// vConsolePrint("Receiving failed, trying again\r\n\0");
					vTaskDelay(100);
					vI2C_Init();
					vTaskDelay(100);
					iRCVStatus = iI2C_Receive(uAddress, &uCurrent);
					if (cnt++ >= 10)
					{
						//reset the bus
						cnt = 0;
						vConsolePrint("Fatal I2C Error, Recycling Power\r\n\0");
					    vI2C_RecyclePower();
					}


				}


			if (uDirection == 1)
				uToSend = uCurrent &= ~(1 << uBitNum);
			else
				uToSend = uCurrent |= 1 << uBitNum;

			cnt = 0;

			iSNDStatus = iI2C_Send(uAddress, uToSend);
			while ((iSNDStatus == FAIL) )
			{
				vConsolePrint("Sending failed, trying again\r\n\0");
				vTaskDelay(300);        // this MUST stay or errors occur in sending;
				vI2C_Init();
				vTaskDelay(300);
				iSNDStatus = iI2C_Send(uAddress, uToSend);
				if(cnt++ >= 10)
				{
					cnt = 0;
					vConsolePrint("Fatal I2C Error, Recycling Power\r\n\0");
					vI2C_RecyclePower();
				}

			}

			cnt = 0;

			vTaskDelay(100);
		}
		else
			vConsolePrint("fail\r\n\0");

		taskYIELD();
	}
}

// SET BITS TAKES THAT LINE DOWN TO 0V..
void vPCF_ResetBits(uint8_t bitnum, uint8_t add)
{
	uint16_t uMessage = 0x00;

	uMessage = (uint16_t) (add << 8) & 0xFF00;
	uMessage = uMessage | (uint16_t) (bitnum & 0x000F);

	if (xI2C_SendQueue == NULL )
	{
		vConsolePrint("FAIL! I2C Task has been deleted\r\n\0");
		return;
	}
	xQueueSendToBack(xI2C_SendQueue, &uMessage, portMAX_DELAY);

}

void vPCF_SetBits(uint8_t bitnum, uint8_t add)
{
	uint16_t uMessage = 0x00;

	uMessage = (uint16_t) (add << 8) & 0xFF00;
	uMessage = uMessage | (uint16_t) (bitnum & 0x000F);

	uMessage |= 1 << 4;

	if (xI2C_SendQueue == NULL )
	{
		vConsolePrint("FAIL! I2C Task has been deleted\r\n\0");
		return;
	}

	xQueueSendToBack(xI2C_SendQueue, &uMessage, portMAX_DELAY);

}

char cI2cGetInput(char port, char pin)
{
	char data = 0;
	portENTER_CRITICAL();
	vI2C_Init();
	char read = iI2C_Receive(port, &data);
	read = iI2C_Receive(port, &data);
	iI2C_Send(port, 0xFF);
	portEXIT_CRITICAL();

	read = iI2C_Receive(port, &data);
	if (read == -1)
	{
		return ERROR;
	}

	if (((1 << (pin - 1)) & ~data))
	{
		return TRUE;
	}
	else
		return FALSE;
}


