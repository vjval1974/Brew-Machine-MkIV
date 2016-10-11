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
#include "main.h"


xQueueHandle xBoilValveQueue;
xTaskHandle xBoilValveTaskHandle, xBoilValveAppletDisplayHandle;
xSemaphoreHandle xAppletRunningSemaphore;

BoilValveState xBoilValveState = BOIL_VALVE_STOPPED;

BoilValveState xGetBoilValveState(void)
{
  return xBoilValveState;
}

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

  xBoilValveQueue = xQueueCreate(5,sizeof(BoilValveMessage));

  if (xBoilValveQueue == NULL)
    {
      vConsolePrint("Couldn't Create Boil Valve Queue\r\n\0");

    }
  else
    vConsolePrint("Created Boil Valve Queue\r\n\0");
}


typedef struct
{
	GPIO_TypeDef *  Port;
	uint16_t Pin;
} Input ;

Input BoilValveClosedPin = { BOIL_VALVE_CLOSED_PORT, BOIL_VALVE_CLOSED_PIN };
Input BoilValveOpenedPin = { BOIL_VALVE_OPENED_PORT, BOIL_VALVE_OPENED_PIN };

PinState xGetPinState( Input input)
{
	if (debounce(input.Port, input.Pin) == 1)
		return PIN_STATE_HIGH;
	return PIN_STATE_LOW;
}


void vBoilValveFunc(BoilValveCommand command)
{

  switch (command)
  {
  case BOIL_VALVE_CMD_OPEN:
    {
      // need to turn relay 2 off which sets up for REV direction
      vPCF_ResetBits(BOIL_VALVE_PIN1, BOIL_VALVE_PORT);
      vPCF_SetBits(BOIL_VALVE_PIN2, BOIL_VALVE_PORT);
      break;
    }
  case BOIL_VALVE_CMD_CLOSE:
    {
      // need to turn relay 1 off which sets up for REV direction
      vPCF_ResetBits(BOIL_VALVE_PIN2, BOIL_VALVE_PORT);
      vPCF_SetBits(BOIL_VALVE_PIN1, BOIL_VALVE_PORT);
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

char BoilValveStateText[3][16] =
{
	"Open",
	"Close",
	"Stop",
};

void vTaskBoilValve(void * pvParameters)
{
  // Create Message structures in memory

  BoilValveCommand xLastCommand;
  BoilValveMessage xMessage;

  BrewMessage xToSend;
  //static int iComplete = 0;

  BoilValveCommand xCurrentCommand = BOIL_VALVE_CMD_STOP;
  xToSend.ucFromTask = BOIL_VALVE_TASK;
  xToSend.xCommand = BREW_STEP_COMPLETE;
  static int iCommandState = 0;
  char buf[40];
  xLastCommand = xCurrentCommand;
  uint8_t uValveClosedLimit = 0xFF, uValveOpenedLimit = 0xFF; //neither on or off.

  for (;;)
    {
      if(xQueueReceive(xBoilValveQueue, &xMessage, 50) != pdPASS)
            {
              xMessage.xCommand = xLastCommand;
            }
          else // received successfully
            {
        	  if (!&xMessage.iBrewStep)
        		  xMessage.iBrewStep = 0;

              vConsolePrint("BoilValve: received message\r\n\0");
              sprintf(buf, "BoilValve: %s, Step: %d\r\n\0", BoilValveStateText[((int)xMessage.xCommand)], (xMessage.iBrewStep));
              vConsolePrint(buf);
              xLastCommand = xMessage.xCommand;
              xCurrentCommand = xMessage.xCommand;

              xToSend.iBrewStep = xMessage.iBrewStep;
              iCommandState = 0;

            }


      switch(xBoilValveState)
      {
      case BOIL_VALVE_OPENED:
        {
          if (xCurrentCommand == BOIL_VALVE_CMD_CLOSE)
            {
              vBoilValveFunc(BOIL_VALVE_CMD_CLOSE);
              xBoilValveState = BOIL_VALVE_CLOSING;
            }
          else if (xCurrentCommand == BOIL_VALVE_CMD_OPEN)
            {
              iCommandState = 1;

            }
          break;
        }
      case BOIL_VALVE_CLOSED:
        {
          if (xCurrentCommand== BOIL_VALVE_CMD_OPEN)
            {
              vBoilValveFunc(BOIL_VALVE_CMD_OPEN);
              xBoilValveState = BOIL_VALVE_OPENING;
            }
          else if (xCurrentCommand == BOIL_VALVE_CMD_CLOSE)
            {
              iCommandState = 1;

            }
          break;
        }
      case BOIL_VALVE_OPENING:
        {
          if (xCurrentCommand== BOIL_VALVE_CMD_STOP)
            {
              vBoilValveFunc(BOIL_VALVE_CMD_STOP);
              vConsolePrint("BoilValve STOP while Opening \r\n\0");
              xBoilValveState = BOIL_VALVE_STOPPED;
              iCommandState = 1;

            }
          else if (xCurrentCommand== BOIL_VALVE_CMD_CLOSE)
            {
              if (xGetPinState(BoilValveClosedPin) == PIN_STATE_HIGH)
              {
                  vBoilValveFunc(BOIL_VALVE_CMD_CLOSE);
                  xBoilValveState = BOIL_VALVE_CLOSING;
                }
              else
                {
                  xBoilValveState = BOIL_VALVE_CLOSED;
                  iCommandState = 1;

                }

            }
          else
            {
        	  if (xGetPinState(BoilValveOpenedPin) == PIN_STATE_LOW)
        	  {
                  vBoilValveFunc(BOIL_VALVE_CMD_STOP);
                  vConsolePrint("BoilValve STOP limit Open \r\n\0");
                  xBoilValveState = BOIL_VALVE_OPENED;

                  iCommandState = 1;
                }
             // sprintf(buf, "Opening with open limit = %d\r\n\0", uValveClosedLimit);
             //               vConsolePrint(buf);
            }
          break;
        }
      case BOIL_VALVE_CLOSING:
        {
          if (xCurrentCommand== BOIL_VALVE_CMD_STOP)
            {
              vBoilValveFunc(BOIL_VALVE_CMD_STOP);
              vConsolePrint("BoilValve STOP while Closing \r\n\0");
              xBoilValveState = BOIL_VALVE_STOPPED;

              iCommandState = 1;
            }
          else if (xCurrentCommand== BOIL_VALVE_CMD_OPEN)
            {
              if (xGetPinState(BoilValveClosedPin) == PIN_STATE_LOW)
              {
                  xBoilValveState = BOIL_VALVE_OPENED;

                  iCommandState = 1;
                }
              else
                {
                  vBoilValveFunc(BOIL_VALVE_CMD_OPEN);
                  xBoilValveState = BOIL_VALVE_OPENING;
                }

            }
          else
            {
        	  if (xGetPinState(BoilValveClosedPin) == PIN_STATE_LOW)
        	  {

                  vBoilValveFunc(BOIL_VALVE_CMD_STOP);
                  vConsolePrint("BoilValve STOP limit Closing \r\n\0");
                  xBoilValveState = BOIL_VALVE_CLOSED;
                  iCommandState = 1;

                }
            }

          break;
        }
      case BOIL_VALVE_STOPPED:
        {

          if (xCurrentCommand == BOIL_VALVE_CMD_OPEN)
            {

              if (xGetPinState(BoilValveOpenedPin) == PIN_STATE_HIGH)
               {
                  vBoilValveFunc(BOIL_VALVE_CMD_OPEN);
                  xBoilValveState = BOIL_VALVE_OPENING;
                }
              else
                {
                  iCommandState = 1;

                  xBoilValveState = BOIL_VALVE_OPENED;
                }
            }
          else if (xCurrentCommand== BOIL_VALVE_CMD_CLOSE)
            {
        	  if (xGetPinState(BoilValveClosedPin) == PIN_STATE_HIGH)
        	  {
                  vBoilValveFunc(BOIL_VALVE_CMD_CLOSE);
                  xBoilValveState = BOIL_VALVE_CLOSING;
                }
              else{
                  iCommandState = 1;

                  xBoilValveState = BOIL_VALVE_CLOSED;
              }
            }
          xCurrentCommand = BOIL_VALVE_STOPPED;

          break;
        }
      default:
        {
          vBoilValveFunc(BOIL_VALVE_CMD_STOP); //stop the  on an instant.
          xBoilValveState = BOIL_VALVE_STOPPED;
          vConsolePrint("Boil Valve in incorrect state!\r\n\0");
          break;

        }
      }// Switch

      if (iCommandState == 1 && xMessage.ucFromTask == BREW_TASK)
             {
              const int iTest = 40;
              vConsolePrint("BoilValve: Sending Step Complete message\r\n\0");
              xToSend.xCommand = BREW_STEP_COMPLETE;
               xQueueSendToBack(xBrewTaskReceiveQueue, &xToSend, 10000);
               iCommandState  = 0;
               xBoilValveState = BOIL_VALVE_STOPPED;
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
      switch (xBoilValveState)
      {
      case BOIL_VALVE_OPENING:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"BOIL_VALVE_OPENING");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case BOIL_VALVE_CLOSING:
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
      case BOIL_VALVE_OPENED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"BOIL_VALVE_OPENED");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case BOIL_VALVE_CLOSED:
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
      case BOIL_VALVE_STOPPED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"BOIL_VALVE_STOPPED");
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

	BoilValveMessage xMessageStatic;
	xMessageStatic.iBrewStep = 0;
	int iOpen= BOIL_VALVE_CMD_OPEN, iClose = BOIL_VALVE_CMD_CLOSE, iStop = BOIL_VALVE_CMD_STOP;
	xMessageStatic.xCommand= BOIL_VALVE_CMD_STOP;

	if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
	{
		xMessageStatic.xCommand = BOIL_VALVE_CMD_OPEN;
		xQueueSendToBack( xBoilValveQueue, &xMessageStatic, 0 );
	}
	else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
	{
		xMessageStatic.xCommand =BOIL_VALVE_CMD_CLOSE;
		xQueueSendToBack( xBoilValveQueue, &xMessageStatic, 0 );

	}
	else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
	{
		xMessageStatic.xCommand = BOIL_VALVE_CMD_STOP;
		xQueueSendToBack( xBoilValveQueue, &xMessageStatic, portMAX_DELAY );

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






