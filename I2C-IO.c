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

#define FAIL -1
#define PASS 1

xQueueHandle xI2C_SendQueue;

static int iI2C_Send(char address, char data);


void vI2C_Init(void){
  static int first = 1;
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
    if (first == 1)
      xI2C_SendQueue = xQueueCreate(20, sizeof(uint16_t));
    first=0;
//    iI2C_Send(I2C_SLAVE_ADDRESS0, 0xFF);
   // printf("I2C address %x Initialised with value %x", I2C_SLAVE_ADDRESS0, 0xFF);

  return;

}

static int iI2C_Send(char address, char data)
{
  long unsigned int i = 0, j;
  int retval = 0;
  portENTER_CRITICAL();
  i = 0;
   while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
i = 0;

  I2C_GenerateSTART(I2C1, ENABLE);

  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  I2C_Send7bitAddress(I2C1, address, I2C_Direction_Transmitter);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;
  I2C_SendData(I2C1, data);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  I2C_GenerateSTOP(I2C1, ENABLE);

  while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  portEXIT_CRITICAL();
  return 1;
}


static int iI2C_Receive(char address, char  * data)
{
  char received = 0;
  long unsigned int i = 0;
  portENTER_CRITICAL();

  I2C_AcknowledgeConfig(I2C1, DISABLE); // We are receiving only 1 byte from the PCF8574

  I2C_GenerateSTART(I2C1, ENABLE);
  i = 0; // to be safe
  while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  I2C_Send7bitAddress(I2C1, address, I2C_Direction_Receiver);

  while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;
  received = I2C_ReceiveData(I2C1);

  I2C_GenerateSTOP(I2C1, ENABLE);

  while(I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF))
    {
      i ++;
      if (i == 7200)
        {
          portEXIT_CRITICAL();
          return -1;

        }
    }
  i = 0;

  while (I2C_GetFlagStatus(I2C1,I2C_FLAG_BUSY))
    {
      i ++;
      if (i == 7200)
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
      uTestMessage = (uint16_t)(uAddress << 8) & 0xFF00;
      uTestMessage = uTestMessage | (uint16_t)(uData & 0x000F);
      if (uDirection)
          uTestMessage |= 1<<4;
      vConsolePrint("I2C Test Task Start\r\n");
         fflush(stdout);

      printf("===================\r\n");
     fflush(stdout);
      em = iI2C_Receive(uAddress, &uRData);
      if ( em == -1){
          printf("failed %d", em);
          //xQueueSendToBack(xI2C_SendQueue, &uTestMessage, 100);
          printf("failed, sending again\r\n");
          fflush(stdout);
        }


//
//      printf("\r\n******I2C Test Task Start\r\n");
      printf("received = %x\r\n", uRData);
      input1 = cI2cGetInput(PORTV, 1);
      printf("pin 1 low? %d\r\n", input1);
      input1 = cI2cGetInput(PORTV, 2);
           printf("pin 2 low? %d\r\n", input1);
           input1 = cI2cGetInput(PORTV, 3);
                printf("pin 3 low? %d\r\n", input1);
                input1 = cI2cGetInput(PORTV, 4);
                     printf("pin 4 low? %d\r\n", input1);
//      printf("sending message:\r\n");
//      printf("message = %x\r\n", uTestMessage);
//      printf("address = %x\r\n", uAddress);
//      printf("data = %x\r\n", uData);


//  if (flag == 0){
//          vPCF_SetBits(0, I2C_SLAVE_ADDRESS0);
//          flag= 1;
//      }
//      else
//        {
//          vPCF_ResetBits(0, I2C_SLAVE_ADDRESS0);
//          flag = 0;
//        }

  vConsolePrint("\r\n******I2C Test Task END\r\n");
    //printf("=========================\r\n");
    fflush(stdout);
    vTaskDelay(1000);
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
  int iRCVStatus = PASS;
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
        uAddress = message>>8;
        uBitNum = (uint8_t)message&(0x0F);
        if (uBitNum > 7 || uBitNum <= 0)
          uBitNum = 0;
        uDirection = ((uint8_t)message&0xF0)>>4;

        // RECEIVE FROM
        // vConsolePrint("Trying to RECEIVE I2C\r\n");
        iRCVStatus = iI2C_Receive(uAddress, &uCurrent);
        while((iRCVStatus == FAIL) && (cnt < 10))
          {
            vConsolePrint("Receiving failed, trying again\r\n");
            vTaskDelay(100);
            vI2C_Init();
            vTaskDelay(100);
            iRCVStatus = iI2C_Receive(uAddress, &uCurrent);
            cnt++;
          }
          if (cnt >= 10)
            {
              vConsolePrint("Fatal I2C Error, deleting task\r\n");

              vQueueDelete(xI2C_SendQueue);
              xI2C_SendQueue = NULL;
              vTaskDelete(NULL);
              taskYIELD();
            }

          if (uDirection  == 1)
            uToSend = uCurrent&=~(1<<uBitNum);
          else
            uToSend = uCurrent |= 1<<uBitNum;

        cnt = 0;

       //sprintf(pcPrintBuf, "utoSend =  %x\r\n", uToSend);
       //sprintf(pcPrintBuf, "message =  %x\r\n", message);
       //vConsolePrint(pcPrintBuf);

       iSNDStatus = iI2C_Send(uAddress, uToSend);
       while((iSNDStatus == FAIL) && (cnt < 10))
         {
           vConsolePrint("Sending failed, trying again\r\n");
           vTaskDelay(100);// this MUST stay or errors occur in sending;
           vI2C_Init();
           vTaskDelay(100);
           iSNDStatus = iI2C_Send(uAddress, uToSend);
           cnt++;

         }
       if (cnt >= 10)

         {
           vConsolePrint("Fatal I2C Error, deleting task\r\n");

           vQueueDelete(xI2C_SendQueue);
           xI2C_SendQueue = NULL;
           vTaskDelete(NULL);
           taskYIELD();
         }
       cnt = 0;


        vTaskDelay(100);
      }
    else vConsolePrint("fail\r\n");

      taskYIELD();
  }
}

// SET BITS TAKES THAT LINE DOWN TO 0V..
void vPCF_ResetBits(uint8_t bitnum, uint8_t add){
  uint16_t uMessage = 0x00;

  uMessage = (uint16_t)(add << 8) & 0xFF00;
  uMessage = uMessage | (uint16_t)(bitnum & 0x000F);
  //printf("addr from pcf = %x\r\n", add);

  //printf("Bit number = %d\r\n", bitnum);
  //vTaskDelay(10);
  if (xI2C_SendQueue ==  NULL)
    {
      vConsolePrint("FAIL! I2C Task has been deleted\r\n");
      return;
    }
  xQueueSendToBack(xI2C_SendQueue, &uMessage, portMAX_DELAY);

}

void vPCF_SetBits(uint8_t bitnum, uint8_t add){
  uint16_t uMessage = 0x00;

  uMessage = (uint16_t)(add << 8) & 0xFF00;
  uMessage = uMessage | (uint16_t)(bitnum & 0x000F);
  //printf("addr from pcf = %x\r\n", add);
  uMessage |= 1<<4;
  //printf("Bit number = %d\r\n", bitnum);
  //vTaskDelay(10);
  if (xI2C_SendQueue ==  NULL)
      {
        vConsolePrint("FAIL! I2C Task has been deleted\r\n");
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
  //vTaskDelay(50);
  read = iI2C_Receive(port, &data);
  if (read == -1)
    {
      return ERROR;
    }
 // printf("~data = %x\r\n", ~data);
  if (((1<<(pin-1)) & ~data)) {
        return TRUE;
  }
  else return FALSE;
}


