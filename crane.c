//-------------------------------------------------------------------------
// Author: Brad Goold
// Date: 18 Feb 2012
// Email Address: W0085400@umail.usq.edu.au
// 
// Purpose:
// Pre:
// Post:
// RCS $Date$
// RCS $Revision$
// RCS $Source$
// RCS $Log$
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "I2C-IO.h"
#include "console.h"
#include "crane.h"
#include "brew.h"
#include "io_util.h"
#include "message.h"
#include "stir.h"
#include "main.h"
#include "button.h"
#include "macros.h"

volatile int8_t cs = CRANE_STOPPED;
CraneState xCraneState = CRANE_STOPPED;



xQueueHandle xCraneQueue = NULL;
xSemaphoreHandle xAppletRunningSemaphore;
xTaskHandle xCraneTaskHandle = NULL, xCraneAppletDisplayHandle = NULL;

void vTaskCrane(void * pvParameters);

CraneState xGetCraneState(void)
{
	if(cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN) == 1)
		return CRANE_AT_BOTTOM;
	if(cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN) == 1)
		return CRANE_AT_TOP;
  return xCraneState;
}

void vCraneInit(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
  vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low

  vSemaphoreCreateBinary(xAppletRunningSemaphore);

  xCraneQueue = xQueueCreate( 5, sizeof(  CraneMessage ) );

  if (xCraneQueue == NULL)
    {
      vConsolePrint("Crane Queue couldn't be created\r\n\0");
    }
  vConsolePrint("Crane Initialised\r\n\0");
}


void vCraneFunc(CraneCommand Command)
{
  switch (Command)
  {
  case CRANE_UP:
    {
      // need to turn relay 2 off which sets up for REV direction
      vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
      vPCF_SetBits(CRANE_PIN1, CRANE_PORT); //pull low
      break;
    }
  case CRANE_DOWN:
    {
      // need to turn relay 1 off which sets up for REV direction
      vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
      vPCF_SetBits(CRANE_PIN2, CRANE_PORT); //pull low
      break;
    }

  default:
    {
      vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
      vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
      break;
    }
  }

}


void vTaskCrane(void * pvParameters)
{
  BrewMessage xToSend;
  CraneMessage xMessage;
  CraneCommand xLastCommand = CRANE_STOP;
  static int iStep = 0;
  xToSend.ucFromTask = CRANE_TASK;
  xToSend.xCommand = BREW_STEP_COMPLETE;
  const int iTest = 40;
  static unsigned char ucDownIncrements = 0;
  uint8_t limit = 0xFF, limit1 = 0xFF; //neither on or off.
  static int xCurrentCommand = CRANE_STOP;
  static int iCommandState = 0;
  char buf[40];

  unsigned char failedI2cReads = 0;
  // testing vars
  char cCount = 0;
  char cLimitVal = 255;

  for (;;)
    {

      if(xQueueReceive(xCraneQueue, &xMessage, 50) != pdPASS)
        {
          xMessage.xCommand = xLastCommand;
        }
      else // received successfully
        {
          iStep = xMessage.iBrewStep;
          vConsolePrint("Crane Task has received a message\r\n\0\0");
          xLastCommand = xMessage.xCommand;

          xCurrentCommand = xMessage.xCommand;
          xToSend.iBrewStep = xMessage.iBrewStep;
          iCommandState = 0;
          ucDownIncrements = 0;
        }

      switch(xCraneState)
      {
      case CRANE_AT_TOP:
        {
          if (xCurrentCommand == CRANE_DOWN)
            {
              vCraneFunc(CRANE_DOWN);
              iCommandState = 0; // not done yet
              xCraneState = CRANE_DRIVING_DOWN;
            }
          else if (xCurrentCommand == CRANE_DOWN_INCREMENTAL)
            {
              xCraneState = CRANE_DRIVING_DOWN_IN_INCREMENTS;
            }
          else if (xCurrentCommand == CRANE_UP)
            {
              iCommandState = 1;
            }
          break;

        }
      case CRANE_AT_BOTTOM:
        {
          if (xCurrentCommand == CRANE_UP)
            {
              vCraneFunc(CRANE_UP);
              xCraneState = CRANE_DRIVING_UP;
              iCommandState = 0; // not done yet
            }
          else if (xCurrentCommand == CRANE_DOWN)
            {
              iCommandState = 1;
            }
          break;
        }
      case CRANE_DRIVING_UP:
        {
          if (xCurrentCommand == CRANE_STOP)
            {
              vCraneFunc(CRANE_STOP);
              xCraneState = CRANE_STOPPED;
              iCommandState = 1;

            }
          else if (xCurrentCommand == CRANE_DOWN)
            {
              limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

              if (limit == 255) // read failed
                {
                   vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
                }
              if (limit != 1)
                {
                  vCraneFunc(CRANE_DOWN);
                  xCraneState = CRANE_DRIVING_DOWN;
                }
              else
                {

                  xCraneState = CRANE_AT_BOTTOM;
                  iCommandState = 1;
                }

            }
          else
            {
              vPCF_SetBits(CRANE_UPPER_LIMIT_PIN, CRANE_UPPER_LIMIT_PORT); // yes the port and pin are reverse from read
              vTaskDelay(100);
              limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              while (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
                }

              if (limit == 1)
                {
                  vCraneFunc(CRANE_STOP);
                  xCraneState = CRANE_AT_TOP;

                  iCommandState = 1;
                }
            }
          break;
        }
      case CRANE_DRIVING_DOWN:
        {
          if (xCurrentCommand == CRANE_STOP)
            {
              vCraneFunc(CRANE_STOP);
              xCraneState = CRANE_STOPPED;

              iCommandState = 1;
            }
          else if (xCurrentCommand == CRANE_UP)
            {
              limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              if (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
                }
              if (limit == 1)
                {
                  xCraneState = CRANE_AT_TOP;

                  iCommandState = 1;
                }
              else
                {
                  vCraneFunc(CRANE_UP);
                  xCraneState = CRANE_DRIVING_UP;
                  iCommandState = 0;
                }

            }
          else// if (xMessage->cDirection == DN)
            {
              limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
                }
              if (limit == 1)
                {
                  vTaskDelay(200);
                  vCraneFunc(CRANE_STOP);
                  xCraneState = CRANE_AT_BOTTOM;
                  iCommandState = 1;

                 }
            }

          break;
        }
      case CRANE_DRIVING_DOWN_IN_INCREMENTS:
        {
          limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

          if (limit == ERROR) // read failed
            {
              vTaskDelay(200);
              limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
            }
          if(limit == FALSE)
            {
              vCraneFunc(CRANE_DOWN);
              vTaskDelay(200);
              vCraneFunc(CRANE_STOP);
              vTaskDelay(1500);
              ucDownIncrements++;
            }

          else if (limit == TRUE)
            {
              // Make sure we are down
              vCraneFunc(CRANE_DOWN);
              vTaskDelay(100);
              vCraneFunc(CRANE_STOP);
              xCraneState = CRANE_AT_BOTTOM;

              iCommandState = 1;
              ucDownIncrements = 0;
              vStir(STIR_STOPPED);
            }
          if (ucDownIncrements == 11)
            vStir(STIR_DRIVING);
          break;
        }
      case CRANE_STOPPED:
        {
          if (xCurrentCommand == CRANE_UP)
            {
              limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              if (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_UPPER_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
                }

              if (limit != 1)
                {
                  iCommandState = 0;
                  vCraneFunc(CRANE_UP);
                  xCraneState = CRANE_DRIVING_UP;
                }

              else
                {
                  iCommandState = 1;

                  xCraneState = CRANE_AT_TOP;
                }
            }
          else if (xCurrentCommand == CRANE_DOWN)
            {
              vPCF_SetBits(CRANE_LOWER_LIMIT_PIN, CRANE_LOWER_LIMIT_PORT); // yes the port and pin are reverse from read
              vTaskDelay(100);
              limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
                }
              if (limit != 1)
                {
                  iCommandState = 0;
                  vCraneFunc(CRANE_DOWN);
                  xCraneState = CRANE_DRIVING_DOWN;
                }
              else
                {
                  iCommandState =1;

                  xCraneState = CRANE_AT_BOTTOM;
                }
            }
          else if (xCurrentCommand == CRANE_DOWN_INCREMENTAL)
            {
              vPCF_SetBits(CRANE_LOWER_LIMIT_PIN, CRANE_LOWER_LIMIT_PORT); // yes the port and pin are reverse from read
              vTaskDelay(100);
              limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit == 255) // read failed
                {
                  vTaskDelay(200);
                  limit = cI2cGetInput(CRANE_LOWER_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
                }
              if (limit != 1)
                {
                  iCommandState = 0;
                  xCraneState = CRANE_DRIVING_DOWN_IN_INCREMENTS;
                }
              else
                {
                  iCommandState  = 1;

                  xCraneState = CRANE_AT_BOTTOM;
                }
            }
          xCurrentCommand = 14; // goes into undefined command state. shouldnt be needed.

          break;
        }
      default:
        {
          vCraneFunc(CRANE_STOP); //stop the crane on an instant.
          xCraneState = CRANE_STOPPED;
          break;

        }
      }// Switch

      if (iCommandState == 1 && xMessage.ucFromTask == BREW_TASK)
        {
          //vConsolePrint("Crane Command Complete, Sending Message\r\n\0\0");
          vTaskDelay(50);
          xToSend.xCommand = BREW_STEP_COMPLETE;
          xQueueSendToBack(xBrewTaskReceiveQueue, &xToSend, 10000);
          //xQueueSendToBack(xBrewTaskReceiveQueue, (void *)40, 100);
          iCommandState  = 0;
         xCraneState = CRANE_STOPPED;
        }

      //vTaskDelay(500);
    }// Task Loop
}//func



void vCraneAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (xCraneState)
      {
      case CRANE_DRIVING_UP:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"DRIVING UP");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case CRANE_DRIVING_DOWN:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"DRIVING DOWN");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case CRANE_AT_TOP:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"TOP");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case CRANE_AT_BOTTOM:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"BOTTOM");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case CRANE_STOPPED:
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

static int Back()
{
	return BackFromApplet(xAppletRunningSemaphore, xCraneAppletDisplayHandle);
}

static void vSendCommandToCrane(CraneCommand cmd)
{
	CraneMessage pxMessage;
	pxMessage.xCommand = cmd;
	xQueueSendToBack( xCraneQueue, &pxMessage, 0 );
}

static int iCraneUp()
{
	vSendCommandToCrane(CRANE_UP);
	return 0;
}

static int iCraneDown()
{
	vSendCommandToCrane(CRANE_DOWN);
	return 0;
}

static int iCraneStop()
{
	vSendCommandToCrane(CRANE_STOP);
	return 0;
}

static Button CraneButtons[] =
{
		{UP_X1, UP_Y1, UP_X2, UP_Y2, "Up", Blue, Green, iCraneUp, ""},
		{DN_X1, DN_Y1, DN_X2, DN_Y2, "Down", Green, Blue, iCraneDown, ""},
		{ST_X1, ST_Y1, ST_X2, ST_Y2, "Stop", Cyan, Red, iCraneStop, ""},
		{BK_X1, BK_Y1, BK_X2, BK_Y2, "BACK", Cyan, Magenta, Back, ""},
};

static int ButtonCount()
{
	return ARRAY_LENGTH(CraneButtons);
}

void vCraneApplet(int init){
	lcd_printf(10,0,12,  "Crane");
	vDrawButtons(CraneButtons, ButtonCount() );
  if (init)
    {
      xTaskCreate( vCraneAppletDisplay,
          ( signed portCHAR * ) "Crane_display",
          configMINIMAL_STACK_SIZE +500,
          NULL,
          tskIDLE_PRIORITY + 1,
          &xCraneAppletDisplayHandle );
    }

}



int iCraneKey(int xx, int yy){

	int retVal = ActionKeyPress(CraneButtons, ButtonCount(), xx, yy);
	vTaskDelay(10);
	return retVal;

}


