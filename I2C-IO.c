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

#define I2C_IO_PORT GPIOB
#define I2C_SDA_PIN GPIO_Pin_8 // these are back to front.. when changed it doesnt work for some reason.. too tired to work out
#define I2C_CLK_PIN GPIO_Pin_9

xQueueHandle xI2C_SendQueue;

static void vI2C_Send(char address, char data);


void vI2C_Init(void){

  GPIO_InitTypeDef GPIO_InitStructure;
  I2C_InitTypeDef I2C_InitStructure;


    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin =  I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;// Alt Function - Open Drain
    GPIO_Init( I2C_IO_PORT, &GPIO_InitStructure );

    GPIO_InitStructure.GPIO_Pin =  I2C_CLK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;// Alt Function - Open Drain
    GPIO_Init( I2C_IO_PORT, &GPIO_InitStructure );

    I2C_Cmd(I2C1, DISABLE);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // Enable GPIOB Clock
    GPIO_PinRemapConfig( GPIO_Remap_I2C1, ENABLE );// Remap the I2C1 pins.
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);


    I2C_DeInit(I2C1);
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
    xI2C_SendQueue = xQueueCreate(20, sizeof(uint16_t));

    vI2C_Send(I2C_SLAVE_ADDRESS0, 0x00);

  return;

}

static void vI2C_Send(char address, char data)
{
  portENTER_CRITICAL();

  while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY));

  I2C_GenerateSTART(I2C1, ENABLE);

  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));

  I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

  I2C_SendData(I2C1, data);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

  I2C_GenerateSTOP(I2C1, ENABLE);

  while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF));

  while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY));

  portEXIT_CRITICAL();

}


static void vI2C_Receive(char address, char  * data)
{
  char received = 0;
  portENTER_CRITICAL();

  I2C_AcknowledgeConfig(I2C1, DISABLE); // We are receiving only 1 byte from the PCF8574

  I2C_GenerateSTART(I2C1, ENABLE);

  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));

  I2C_Send7bitAddress(I2C1, address, I2C_Direction_Receiver);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

  while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));

  received = I2C_ReceiveData(I2C1);

  I2C_GenerateSTOP(I2C1, ENABLE);

  while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF));

  while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY));

  *data = received;

  portEXIT_CRITICAL();
}

void vI2C_TestTask(void *pvParameters)
{
  uint16_t uTestMessage = 0x0000;
  uint8_t uAddress, uData, uRData = 3, uDirection = 0;
  uAddress = 0x70;
  static char flag = 0;
  for (;;)
    {
      uData = 0x03;
      uTestMessage = (uint16_t)(uAddress << 8) & 0xFF00;
      uTestMessage = uTestMessage | (uint16_t)(uData & 0x000F);
      if (uDirection)
          uTestMessage |= 1<<4;
      vI2C_Receive(uAddress, &uRData);
      //printf("\r\n******I2C Test Task Start\r\n");
      //printf("received = %x\r\n", uRData);
      //printf("sending message:\r\n");
      //printf("message = %x\r\n", uTestMessage);
      //printf("address = %x\r\n", uAddress);
      //printf("data = %x\r\n", uData);
      if (flag == 0){
          vPCF_SetBits(1, I2C_SLAVE_ADDRESS0);
          flag= 1;
      }
      else
        {
          vPCF_ResetBits(1, I2C_SLAVE_ADDRESS0);
          flag = 0;
        }
      vTaskDelay(4);
      //xQueueSendToBack(xI2C_SendQueue, &uTestMessage, portMAX_DELAY);
      //xQueueSendToBack(xI2C_SendQueue, &uTestMessage+1, portMAX_DELAY);
      //xQueueSendToBack(xI2C_SendQueue, &uTestMessage+2, portMAX_DELAY);
      //printf("Message Sent\r\n");
      //printf("\r\n******I2C Test Task Start\r\n");
      vTaskDelay(5);

    }

}

void vI2C_SendTask(void *pvParameters)
{
  portBASE_TYPE xStatus = pdFAIL;
  uint16_t message;
  uint8_t uAddress = 0x00, uBitNum = 0x00, uCurrent = 0x00, uToSend = 0x00, uDirection = 0xFF;
for (;;)
  {
    xStatus = xQueueReceive(xI2C_SendQueue, &message, portMAX_DELAY);
    if (xStatus == pdPASS)
      {
 //       I2C_SoftwareResetCmd(I2C1, ENABLE);
        //printf("Message Received:\r\n");
        //printf("Message =  %x\r\n", message);
        portENTER_CRITICAL();
        uAddress = message>>8;
        vI2C_Receive(uAddress, &uCurrent);
        uBitNum = (uint8_t)message&(0x0F);
        //printf("RAddress =  %x\r\n", uAddress);
        if (uBitNum > 7 || uBitNum <= 0)
          uBitNum = 0;

        //printf("uBitNum =  %d\r\n", uBitNum);
        //printf("uCurrent =  %d\r\n", uCurrent);
       // printf("Direction =  %d\r\n", ((uint8_t)message&0xF0)>>4);
        uDirection = ((uint8_t)message&0xF0)>>4;
        if (uDirection  == 0)
          uToSend = uCurrent&=~(1<<uBitNum);
        else
          uToSend = uCurrent |= 1<<uBitNum;
       // printf("utoSend =  %x\r\n", uToSend);
        //vTaskDelay(100);
        vI2C_Send(uAddress, uToSend);
        portEXIT_CRITICAL();
        //vTaskDelay(100);
        //vTaskDelay(100);
      }
    else printf("fail\r\n");

      taskYIELD();
  }
}

void vPCF_SetBits(uint8_t bitnum, uint8_t add){
  uint16_t uMessage = 0x00;

  uMessage = (uint16_t)(add << 8) & 0xFF00;
  uMessage = uMessage | (uint16_t)(bitnum & 0x000F);
  uMessage |= 1<<4;
  //printf("Bit number = %d\r\n", bitnum);
  //vTaskDelay(10);
  xQueueSendToBack(xI2C_SendQueue, &uMessage, portMAX_DELAY);

}

void vPCF_ResetBits(uint8_t bitnum, uint8_t add){
  uint16_t uMessage = 0x00;

  uMessage = (uint16_t)(add << 8) & 0xFF00;
  uMessage = uMessage | (uint16_t)(bitnum & 0x000F);
  //uMessage |= 1<<4;
  //printf("Bit number = %d\r\n", bitnum);
  //vTaskDelay(10);
  xQueueSendToBack(xI2C_SendQueue, &uMessage, portMAX_DELAY);

}




