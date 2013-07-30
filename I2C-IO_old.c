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
#define I2C_SDA_PIN GPIO_Pin_8
#define I2C_CLK_PIN GPIO_Pin_9

xQueueHandle xI2C_SendQueue;



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
   // GPIO_SetBits(GPIOB, GPIO_Pin_8);
   // GPIO_SetBits(GPIOB, GPIO_Pin_9);
    I2C_Cmd(I2C1, ENABLE);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // Enable GPIOB Clock
    GPIO_PinRemapConfig( GPIO_Remap_I2C1, ENABLE );// Map TIM3_CH3 and CH4 to Step Pins
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);


    I2C_DeInit(I2C1);
    I2C_StructInit(&I2C_InitStructure);
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_ClockSpeed = 50;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStructure);

    I2C_ITConfig(I2C2, I2C_IT_EVT, ENABLE);

    xI2C_SendQueue = xQueueCreate(5, sizeof(uint16_t));
    //I2C_GenerateSTART(I2C1, ENABLE);
    //while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));
    //I2C_Send7bitAddress(I2C1, 0x70<<1, I2C_Direction_Transmitter);
    //while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));
    //while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
  return;

}

void vI2C_Send(char address, char data)
{
  I2C_GenerateSTART(I2C1, ENABLE);

  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));

  I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

  I2C_SendData(I2C1, data);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

//  I2C_GenerateSTOP(I2C1, ENABLE);
//
//  /*stop bit flag*/
//
//  while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF));

}
void vI2C_Receive(char address, char  * data)
{
  char received = 0;


  I2C_AcknowledgeConfig(I2C1, ENABLE);
  /* Test on BUSY Flag */
  //while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY));
  /* Enable the I2C peripheral */

  I2C_GenerateSTART(I2C1, ENABLE);

  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB));

  I2C_Send7bitAddress(I2C1, 0x71, I2C_Direction_Transmitter);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

//  printf("\r\nReceiver mode checked\r\n");

 received = I2C_ReceiveData(I2C1);

  printf("Ready to receive\r\n");
  //while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));

  I2C_GenerateSTOP(I2C1, ENABLE);

    /*stop bit flag*/

   // while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF));





  *data = received;

}
void vI2C_TestTask(void *pvParameters)
{
  uint16_t uTestMessage;
  uint8_t uAddress, uData, uRData = 0;
  uAddress = 0x70;

  for (;;)
    {
      uData = 0xFF;
      uTestMessage = (uint16_t)(uAddress << 8) & 0xFF00;
      uTestMessage = uTestMessage | (uint16_t)(uData & 0x00FF);
      //vI2C_Receive(uAddress, &uRData);

      printf("rec = %x\r\n", uRData);
      printf("sending message:\r\n");
      printf("message = %x\r\n", uTestMessage);
      printf("address = %x\r\n", uAddress);
      printf("data = %x\r\n", uData);

      xQueueSendToBack(xI2C_SendQueue, &uTestMessage, portMAX_DELAY);
      printf("Message Sent\r\n");

      vTaskDelay(1000);

    }

}

void vI2C_SendTask(void *pvParameters)
{
  portBASE_TYPE xStatus;
  uint16_t message;
  uint8_t uAddress = 0x00, uData = 0x00;
for (;;)
  {
    xStatus = xQueueReceive(xI2C_SendQueue, &message, portMAX_DELAY);
    if (xStatus == pdPASS)
      {
        printf("Message Received:\r\n");
        printf("Message =  %x\r\n", message);
        uAddress = message>>8;
        uData = (uint8_t)message;
        printf("RAddress =  %x\r\n", uAddress);
        printf("RData =  %x\r\n", uData);

        vI2C_Send(uAddress, uData);

        //vTaskDelay(100);
      }
    else printf("fail\r\n");

      taskYIELD();
  }
}



