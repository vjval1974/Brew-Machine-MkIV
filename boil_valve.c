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

char BoilValveCmdText[3][16] =
{
	"Open",
	"Close",
	"Stop",
};

char BoilValveStateText[5][16] =
{
	"Opened",
	"Closed",
	"Opening",
	"Closing",
	"Stopped"
};

char * pcGetBoilValveStateText(BoilValveState state)
{
	return BoilValveStateText[state];
}

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
              sprintf(buf, "BoilValve: %s, Step: %d\r\n\0", BoilValveCmdText[((int)xMessage.xCommand)], (xMessage.iBrewStep));
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
void vToggleBoilValve()
{
	BoilValveMessage xMessageStatic;
	xMessageStatic.iBrewStep = 0;
	xMessageStatic.xCommand= BOIL_VALVE_CMD_STOP;

	if (xGetBoilValveState() != BOIL_VALVE_OPENED)
	{
		xMessageStatic.xCommand = BOIL_VALVE_CMD_OPEN;
		xQueueSendToBack( xBoilValveQueue, &xMessageStatic, 0 );
	}
	else
	{
		xMessageStatic.xCommand = BOIL_VALVE_CMD_CLOSE;
	    xQueueSendToBack( xBoilValveQueue, &xMessageStatic, 0 );
	}
}
