/*
 * boil_valve.c
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "Flow1.h"
#include "drivers/ds1820.h"
#include "I2C-IO.h"
#include "console.h"
#include "message.h"
#include "boil_valve.h"
#include "io_util.h"
#include "brew.h"


xQueueHandle xBoilValveQueue;
xTaskHandle xBoilValveTaskHandle, xBoilValveAppletDisplayHandle;
xSemaphoreHandle xAppletRunningSemaphore;


// Boil Valve States
#define OPENED 10
#define CLOSED 11
#define OPENING 12
#define CLOSING 13
#define STOPPED 14

void vBoilValveInit(void)
{

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  BOIL_VALVE_CLOSED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init( BOIL_VALVE_CLOSED_PORT, &GPIO_InitStructure );

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  BOIL_VALVE_OPENED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init( BOIL_VALVE_OPENED_PORT, &GPIO_InitStructure );

  vSemaphoreCreateBinary(xAppletRunningSemaphore);

  xBoilValveQueue = xQueueCreate(5,sizeof(struct GenericMessage *));

  if (xBoilValveQueue == NULL)
    {
      vConsolePrint("Couldn't Create Boil Valve Queue\r\n");

    }
  else
    vConsolePrint("Created Boil Valve Queue\r\n");
}


void vBoilValveFunc(int Command)
{

  switch (Command)
  {
  case OPEN:
    {
      // need to turn relay 2 off which sets up for REV direction
      vPCF_ResetBits(BOIL_VALVE_PIN2, BOIL_VALVE_PORT); //pull low
      vPCF_SetBits(BOIL_VALVE_PIN1, BOIL_VALVE_PORT); //pull low
      break;
    }
  case CLOSE:
    {
      // need to turn relay 1 off which sets up for REV direction
      vPCF_ResetBits(BOIL_VALVE_PIN1, BOIL_VALVE_PORT); //pull low
      vPCF_SetBits(BOIL_VALVE_PIN2, BOIL_VALVE_PORT); //pull low
      break;
    }

  default:
    {
      vPCF_ResetBits(BOIL_VALVE_PIN1, BOIL_VALVE_PORT); //pull low
      vPCF_ResetBits(BOIL_VALVE_PIN2, BOIL_VALVE_PORT); //pull low
      break;
    }
  }


}


int iBoilValveState = STOPPED;

void vTaskBoilValve(void * pvParameters)
{
  // Create Message structures in memory
  static struct GenericMessage * xMessage, *xLastMessage, *xToSend;
  xLastMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  xMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  xToSend = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
    static int iComplete = 0;
  uint8_t limit = 0xFF, limit1 = 0xFF; //neither on or off.
  static int iC = STOP;
  xToSend->ucFromTask = BOIL_VALVE_TASK;
  xToSend->ucToTask = BREW_TASK;
  xToSend->pvMessageContent = (void *)&iComplete;
  static int iCommandState = 0;
  char buf[40];
  xLastMessage->pvMessageContent = &iC;
  uint8_t uIn1 = 0xFF, uIn2 = 0xFF; //neither on or off.


  for (;;)
    {
      if(xQueueReceive(xBoilValveQueue, &xMessage, 50) != pdPASS)
            {
              xMessage->pvMessageContent = xLastMessage->pvMessageContent;
            }
          else // received successfully
            {
              vConsolePrint("BoilValve: received message\r\n");
              sprintf(buf, "BoilValve: xMessage = %d, Step: %d\r\n", *((int *)xMessage->pvMessageContent), xMessage->uiStepNumber);
              vConsolePrint(buf);
              xLastMessage->pvMessageContent = xMessage->pvMessageContent;
              iC = *((int*)xMessage->pvMessageContent); //casts to int * and then dereferences.
              iComplete = 0;
              xToSend->uiStepNumber = xMessage->uiStepNumber;
              iCommandState = 0;
            }


      switch(iBoilValveState)
      {
      case OPENED:
        {
          if (iC == CLOSE)
            {
              vBoilValveFunc(CLOSE);
              iBoilValveState = CLOSING;
            }
          else if (iC == OPEN)
            {
              iCommandState = 1;
              iComplete = STEP_COMPLETE;
            }
          break;
        }
      case CLOSED:
        {
          if (iC== OPEN)
            {
              vBoilValveFunc(OPEN);
              iBoilValveState = OPENING;
            }
          else if (iC == CLOSE)
            {
              iCommandState = 1;
              iComplete = STEP_COMPLETE;
            }
          break;
        }
      case OPENING:
        {
          if (iC== STOP)
            {
              vBoilValveFunc(STOP);
              vConsolePrint("BoilValve STOP while Opening \r\n");
              iBoilValveState = STOPPED;
              iCommandState = 1;
              iComplete = STEP_COMPLETE;
            }
          else if (iC== CLOSE)
            {
              uIn1 = debounce(BOIL_VALVE_CLOSED_PORT, BOIL_VALVE_CLOSED_PIN);
              if (uIn1 != 0)
                {
                  vBoilValveFunc(CLOSE);
                  iBoilValveState = CLOSING;
                }
              else
                {
                  iBoilValveState = CLOSED;
                  iCommandState = 1;
                  iComplete = STEP_COMPLETE;
                }

            }
          else
            {
              uIn1 = debounce(BOIL_VALVE_OPENED_PORT, BOIL_VALVE_OPENED_PIN);
              if (uIn1 == 0)
                {
                  vBoilValveFunc(STOP);
                  vConsolePrint("BoilValve STOP limit Open \r\n");
                  iBoilValveState = OPENED;
                  iComplete = STEP_COMPLETE;
                  iCommandState = 1;
                }
             // sprintf(buf, "Opening with open limit = %d\r\n", uIn1);
             //               vConsolePrint(buf);
            }
          break;
        }
      case CLOSING:
        {
          if (iC== STOP)
            {
              vBoilValveFunc(STOP);
              vConsolePrint("BoilValve STOP while Closing \r\n");
              iBoilValveState = STOPPED;
              iComplete = STEP_COMPLETE;
              iCommandState = 1;
            }
          else if (iC== OPEN)
            {
              uIn1 = debounce(BOIL_VALVE_OPENED_PORT, BOIL_VALVE_OPENED_PIN);
              if (uIn1 == 0)
                {
                  iBoilValveState = OPENED;
                  iComplete = STEP_COMPLETE;
                  iCommandState = 1;
                }
              else
                {
                  vBoilValveFunc(OPEN);
                  iBoilValveState = OPENING;
                }

            }
          else// if (xMessage->cDirection == CLOSE)
            {

              uIn1 = debounce(BOIL_VALVE_CLOSED_PORT, BOIL_VALVE_CLOSED_PIN);
              if (uIn1 == 0)
                {
                  //vTaskDelay(200);
                  vBoilValveFunc(STOP);
                  vConsolePrint("BoilValve STOP limit Closing \r\n");
                  iBoilValveState = CLOSED;
                  iCommandState = 1;
                  iComplete = STEP_COMPLETE;
                }
            }

          break;
        }
      case STOPPED:
        {

          if (iC == OPEN)
            {
              uIn1 = debounce(BOIL_VALVE_OPENED_PORT, BOIL_VALVE_OPENED_PIN);
              if (uIn1 != 0)
                {
                  vBoilValveFunc(OPEN);
                  iBoilValveState = OPENING;
                }
              else
                {
                  iCommandState = 1;
                  iComplete = STEP_COMPLETE;
                  iBoilValveState = OPENED;
                }
            }
          else if (iC== CLOSE)
            {
              uIn1 = debounce(BOIL_VALVE_CLOSED_PORT, BOIL_VALVE_CLOSED_PIN);
              if (uIn1 != 0)
                {
                  vBoilValveFunc(CLOSE);
                  iBoilValveState = CLOSING;
                }
              else{
                  iCommandState = 1;
                  iComplete = STEP_COMPLETE;
                  iBoilValveState = CLOSED;
              }
            }
          iC = STOPPED;

          break;
        }
      default:
        {
          vBoilValveFunc(STOP); //stop the  on an instant.
          iBoilValveState = STOPPED;
          vConsolePrint("Boil Valve in incorrect state!\r\n");
          break;

        }
      }// Switch
      if (iCommandState == 1 && xMessage->ucFromTask == BREW_TASK)
             {
              const int iTest = 40;
              vConsolePrint("BoilValve: Sending Step Complete message\r\n");
              xToSend->pvMessageContent = (void *)&iTest;
               xQueueSendToBack(xBrewTaskReceiveQueue, &xToSend, 10000);
               iCommandState  = 0;
               iBoilValveState = STOPPED;
             }


    }// Task Loop
}//func

// ================================================================================================

void vBoilValveAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (iBoilValveState)
      {
      case OPENING:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"OPENING");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case CLOSING:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"CLOSING");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case OPENED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"OPENED");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case CLOSED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"CLOSED");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case STOPPED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"STOPPED");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      default:
        {
          break;
        }
      }

      tog = tog ^ 1;

      xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
      vTaskDelay(500);


    }
}

#define UP_X1 0
#define UP_Y1 30
#define UP_X2 150
#define UP_Y2 100
#define UP_W (UP_X2-UP_X1)
#define UP_H (UP_Y2-UP_Y1)

#define DN_X1 0
#define DN_Y1 105
#define DN_X2 150
#define DN_Y2 175
#define DN_W (DN_X2-DN_X1)
#define DN_H (DN_Y2-DN_Y1)

#define ST_X1 155
#define ST_Y1 30
#define ST_X2 300
#define ST_Y2 175
#define ST_W (ST_X2-ST_X1)
#define ST_H (ST_Y2-ST_Y1)

#define BAK_X1 200
#define BAK_Y1 190
#define BAK_X2 315
#define BAK_Y2 235
#define BAK_W (BAK_X2-BAK_X1)
#define BAK_H (BAK_Y2-BAK_Y1)

void vBoilValveApplet(int init){
  if (init)
    {
      lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, Red);
      lcd_fill(UP_X1+1, UP_Y1+1, UP_W, UP_H, Blue);
      lcd_DrawRect(DN_X1, DN_Y1, DN_X2, DN_Y2, Red);
      lcd_fill(DN_X1+1, DN_Y1+1, DN_W, DN_H, Blue);
      lcd_DrawRect(ST_X1, ST_Y1, ST_X2, ST_Y2, Cyan);
      lcd_fill(ST_X1+1, ST_Y1+1, ST_W, ST_H, Red);
      lcd_DrawRect(BAK_X1, BAK_Y1, BAK_X2, BAK_Y2, Cyan);
      lcd_fill(BAK_X1+1, BAK_Y1+1, BAK_W, BAK_H, Magenta);
      lcd_printf(10,1,12,  "MANUAL Boil Valve");
      lcd_printf(8,4,2, "UP");
      lcd_printf(8,8,2, "DN");
      lcd_printf(26,6,4, "STOP");
      lcd_printf(30, 13, 4, "Back");

      xTaskCreate( vBoilValveAppletDisplay,
          ( signed portCHAR * ) "Boil_display",
          configMINIMAL_STACK_SIZE +500,
          NULL,
          tskIDLE_PRIORITY + 1,
          &xBoilValveAppletDisplayHandle );
    }

}



int iBoilValveKey(int xx, int yy){

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  static struct GenericMessage * pxMessage;
  pxMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  int iOpen= OPEN, iClose = CLOSE, iStop = STOP;
  pxMessage->pvMessageContent = &iStop;

  if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
    {
      pxMessage->pvMessageContent = &iOpen;
      xQueueSendToBack( xBoilValveQueue, &pxMessage, 0 );
    }
  else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
    {
      pxMessage->pvMessageContent = &iClose;
      xQueueSendToBack( xBoilValveQueue, &pxMessage, 0 );

    }
  else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
    {
      pxMessage->pvMessageContent = &iStop;
      xQueueSendToBack( xBoilValveQueue, &pxMessage, portMAX_DELAY );


    }
  else if (xx > BAK_X1 && yy > BAK_Y1 && xx < BAK_X2 && yy < BAK_Y2)
    {

      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xBoilValveAppletDisplayHandle != NULL)
        {
          vTaskDelete(xBoilValveAppletDisplayHandle);
          vTaskDelay(100);
          xBoilValveAppletDisplayHandle = NULL;
        }
      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}






