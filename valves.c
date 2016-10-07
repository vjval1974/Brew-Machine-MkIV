/*
 * valves.c
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

#include "valves.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>
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
#include "io_util.h"

void vValvesAppletDisplay(void *pvParameters);
void vValvesApplet(int init);

xTaskHandle xValvesTaskHandle = NULL, xValvesAppletDisplayHandle = NULL;
xSemaphoreHandle xValvesAppletRunningSemaphore;

static ValveState HLTValveState = VALVE_STATE_NOT_DEFINED;
static ValveState MashValveState = VALVE_STATE_NOT_DEFINED;
static ValveState InletValveState = VALVE_STATE_NOT_DEFINED;
static ValveState ChillerValveState = VALVE_STATE_NOT_DEFINED;

ValveState ucGetHltValveState()
{
	if (GPIO_ReadInputDataBit(HLT_VALVE_PORT, HLT_VALVE_PIN ) == 0)
	{
		return VALVE_CLOSED;
	}
	else
		return VALVE_OPENED;
}

ValveState ucGetMashValveState()
{
	if (GPIO_ReadInputDataBit(MASH_VALVE_PORT, MASH_VALVE_PIN ) == 0)
	{
		return VALVE_CLOSED;
	}
	else
		return VALVE_OPENED;
}

ValveState ucGetInletValveState()
{
	if (GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN ) == 0)
	{
		return VALVE_CLOSED;
	}
	else
		return VALVE_OPENED;
}

ValveState ucGetChillerValveState()
{
	if (GPIO_ReadInputDataBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN ) == 0)
	{
		return VALVE_CLOSED;
	}
	else
		return VALVE_OPENED;
}

void vValvesInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = HLT_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(HLT_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(HLT_VALVE_PORT, HLT_VALVE_PIN ); //pull low
	HLTValveState = VALVE_CLOSED;

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = MASH_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(MASH_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(MASH_VALVE_PORT, MASH_VALVE_PIN ); //pull low
	MashValveState = VALVE_CLOSED;

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = INLET_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(INLET_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(INLET_VALVE_PORT, INLET_VALVE_PIN ); //pull low
	InletValveState = VALVE_CLOSED;

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = CHILLER_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(CHILLER_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN ); //pull low
	ChillerValveState = VALVE_CLOSED;

	vSemaphoreCreateBinary(xValvesAppletRunningSemaphore);

}

void vValveActuate(unsigned char valve, ValveCommand command)
{
	uint8_t current = 0;

	switch (valve)
	{
		case HLT_VALVE:
			{
			if (command == TOGGLE_VALVE)
			{
				current = GPIO_ReadInputDataBit(HLT_VALVE_PORT, HLT_VALVE_PIN );
				GPIO_WriteBit(HLT_VALVE_PORT, HLT_VALVE_PIN, current ^ 1);

			}
			else if (command == OPEN_VALVE)
			{
				GPIO_WriteBit(HLT_VALVE_PORT, HLT_VALVE_PIN, 1);
			}
			else
			{
				GPIO_WriteBit(HLT_VALVE_PORT, HLT_VALVE_PIN, 0);
			}
			break;
		}
		case MASH_VALVE:
			{
			if (command == TOGGLE_VALVE)
			{
				current = GPIO_ReadInputDataBit(MASH_VALVE_PORT, MASH_VALVE_PIN );
				GPIO_WriteBit(MASH_VALVE_PORT, MASH_VALVE_PIN, current ^ 1);

			}
			else if (command == OPEN_VALVE)
			{
				GPIO_WriteBit(MASH_VALVE_PORT, MASH_VALVE_PIN, 1);
			}
			else
			{
				GPIO_WriteBit(MASH_VALVE_PORT, MASH_VALVE_PIN, 0);
			}
			break;
		}
		case INLET_VALVE:
			{
			if (command == TOGGLE_VALVE)
			{
				current = GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN );
				GPIO_WriteBit(INLET_VALVE_PORT, INLET_VALVE_PIN, current ^ 1);
			}
			else if (command == OPEN_VALVE)
			{
				GPIO_WriteBit(INLET_VALVE_PORT, INLET_VALVE_PIN, 1);
			}
			else
			{
				GPIO_WriteBit(INLET_VALVE_PORT, INLET_VALVE_PIN, 0);
			}
			break;
		}
		case CHILLER_VALVE:
			{
			if (command == TOGGLE_VALVE)
			{
				current = GPIO_ReadInputDataBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN );
				GPIO_WriteBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN, current ^ 1);
			}
			else if (command == OPEN_VALVE)
			{
				GPIO_WriteBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN, 1);
			}
			else
			{
				GPIO_WriteBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN, 0);
			}
			break;
		}
		default:
			{
			break;
		}
	}
}



#define TOGGLE_HLT_VALVE_X1 0
#define TOGGLE_HLT_VALVE_Y1 30
#define TOGGLE_HLT_VALVE_X2 90
#define TOGGLE_HLT_VALVE_Y2 100
#define TOGGLE_HLT_VALVE_W (TOGGLE_HLT_VALVE_X2-TOGGLE_HLT_VALVE_X1)
#define TOGGLE_HLT_VALVE_H (TOGGLE_HLT_VALVE_Y2-TOGGLE_HLT_VALVE_Y1)

#define TOGGLE_MASH_VALVE_X1 95
#define TOGGLE_MASH_VALVE_Y1 30
#define TOGGLE_MASH_VALVE_X2 180
#define TOGGLE_MASH_VALVE_Y2 100
#define TOGGLE_MASH_VALVE_W (TOGGLE_MASH_VALVE_X2-TOGGLE_MASH_VALVE_X1)
#define TOGGLE_MASH_VALVE_H (TOGGLE_MASH_VALVE_Y2-TOGGLE_MASH_VALVE_Y1)

#define TOGGLE_INLET_VALVE_X1 0
#define TOGGLE_INLET_VALVE_Y1 105
#define TOGGLE_INLET_VALVE_X2 90
#define TOGGLE_INLET_VALVE_Y2 175
#define TOGGLE_INLET_VALVE_W (TOGGLE_INLET_VALVE_X2-TOGGLE_INLET_VALVE_X1)
#define TOGGLE_INLET_VALVE_H (TOGGLE_INLET_VALVE_Y2-TOGGLE_INLET_VALVE_Y1)

#define TOGGLE_CHILLER_VALVE_X1 95
#define TOGGLE_CHILLER_VALVE_Y1 105
#define TOGGLE_CHILLER_VALVE_X2 180
#define TOGGLE_CHILLER_VALVE_Y2 175
#define TOGGLE_CHILLER_VALVE_W (TOGGLE_CHILLER_VALVE_X2-TOGGLE_CHILLER_VALVE_X1)
#define TOGGLE_CHILLER_VALVE_H (TOGGLE_CHILLER_VALVE_Y2-TOGGLE_CHILLER_VALVE_Y1)

#define RESET_FLOW_1_X1 185
#define RESET_FLOW_1_Y1 30
#define RESET_FLOW_1_X2 275
#define RESET_FLOW_1_Y2 100
#define RESET_FLOW_1_W (RESET_FLOW_1_X2-RESET_FLOW_1_X1)
#define RESET_FLOW_1_H (RESET_FLOW_1_Y2-RESET_FLOW_1_Y1)


#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)


void vValvesApplet(int init){
 
  HLTValveState = ucGetHltValveState();
  MashValveState = ucGetMashValveState();
  InletValveState = ucGetInletValveState();
  ChillerValveState = ucGetChillerValveState();

  if (init)
    {

	  lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
	  lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
	  lcd_printf(30, 13, 4, "Back");

      if (HLTValveState == VALVE_OPENED)
        {
          lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Red);
          lcd_printf(0,4,13,"HLT->MASH");
        }

      else
        {
          lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Green);
          lcd_printf(0,4,13,"HLT->HLT");
        }

      if (MashValveState == VALVE_OPENED)
        {
          lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Red);
          lcd_printf(12,4,13, "MASH->BOIL");
        }

      else
        {
          lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
          lcd_printf(12,4,13, "MASH->MASH");
        }


      if (InletValveState == VALVE_OPENED)
        {
          lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Red);
          lcd_printf(0,8,13, "INLET");
          lcd_printf(0,9,13, "OPENED");
        }

      else
        {
          lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
          lcd_printf(0,8,13, "INLET");
          lcd_printf(0,9,13, "CLOSED");
        }

      if (ChillerValveState == VALVE_OPENED)
         {
           lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Blue);
           lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Red);
           lcd_printf(12,8,13, "CHILLER");
           lcd_printf(12,9,13, "OPENED");
         }
       else
         {
           lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Cyan);
           lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Green);
           lcd_printf(12,8,13, "CHILLER");
           lcd_printf(12,9,13, "CLOSED");
         }


      xTaskCreate( vValvesAppletDisplay,
          ( signed portCHAR * ) "V_disp",
          configMINIMAL_STACK_SIZE + 200,
          NULL,
          tskIDLE_PRIORITY,
          &xValvesAppletDisplayHandle );
    }

}


void vValvesAppletDisplay(void *pvParameters)
{
	static char tog = 0; //toggles each loop

	unsigned int uiDecimalPlaces = 3;
	float fHLTTemp = 0, fMashTemp = 0;
	float fNumber = 54.3211;
	static ValveState hlt_last = VALVE_CLOSED, mash_last = VALVE_CLOSED, boil_last = VALVE_CLOSED, inlet_last = VALVE_CLOSED;
	static ValveState chiller_last = VALVE_CLOSED;
	lcd_DrawRect(RESET_FLOW_1_X1, RESET_FLOW_1_Y1, RESET_FLOW_1_X2, RESET_FLOW_1_Y2, Blue);
	lcd_fill(RESET_FLOW_1_X1 + 1, RESET_FLOW_1_Y1 + 1, RESET_FLOW_1_W, RESET_FLOW_1_H, Dark_Blue);
	lcd_printf(25, 3, 13, "RESET");
	lcd_printf(24, 4, 13, "Flow Tx 1");
	for (;;)
	{

		xSemaphoreTake(xValvesAppletRunningSemaphore, portMAX_DELAY);
		//take the semaphore so that the key handler wont
		//return to the menu system until its returned

		fHLTTemp = ds1820_get_temp(HLT);
		fMashTemp = ds1820_get_temp(MASH);

		HLTValveState = ucGetHltValveState();
		MashValveState = ucGetMashValveState();
		InletValveState = ucGetInletValveState();
		ChillerValveState = ucGetChillerValveState();
		if (HLTValveState != hlt_last ||
		    MashValveState != mash_last ||
		    InletValveState != inlet_last ||
		    ChillerValveState != chiller_last
		        )
		{
			if (HLTValveState == VALVE_OPENED)
			{
				lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Blue);
				lcd_fill(TOGGLE_HLT_VALVE_X1 + 1, TOGGLE_HLT_VALVE_Y1 + 1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Red);
				lcd_printf(0, 4, 13, "HLT->MASH");

			}

			else
			{
				lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Cyan);
				lcd_fill(TOGGLE_HLT_VALVE_X1 + 1, TOGGLE_HLT_VALVE_Y1 + 1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Green);
				lcd_printf(0, 4, 13, "HLT->HLT");
			}

			if (MashValveState == VALVE_OPENED)
			{
				lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Blue);
				lcd_fill(TOGGLE_MASH_VALVE_X1 + 1, TOGGLE_MASH_VALVE_Y1 + 1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Red);
				lcd_printf(12, 4, 13, "MASH->BOIL");
			}

			else
			{
				lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
				lcd_fill(TOGGLE_MASH_VALVE_X1 + 1, TOGGLE_MASH_VALVE_Y1 + 1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
				lcd_printf(12, 4, 13, "MASH->MASH");
			}

			if (InletValveState == VALVE_OPENED)
			{
				lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Blue);
				lcd_fill(TOGGLE_INLET_VALVE_X1 + 1, TOGGLE_INLET_VALVE_Y1 + 1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Red);
				lcd_printf(0, 8, 13, "INLET");
				lcd_printf(0, 9, 13, "OPENED");
			}

			else
			{
				lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
				lcd_fill(TOGGLE_INLET_VALVE_X1 + 1, TOGGLE_INLET_VALVE_Y1 + 1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
				lcd_printf(0, 8, 13, "INLET");
				lcd_printf(0, 9, 13, "CLOSED");
			}

			if (ChillerValveState == VALVE_OPENED)
			{
				lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Blue);
				lcd_fill(TOGGLE_CHILLER_VALVE_X1 + 1, TOGGLE_CHILLER_VALVE_Y1 + 1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Red);
				lcd_printf(12, 8, 13, "CHILLER");
				lcd_printf(12, 9, 13, "OPENED");
			}

			else
			{
				lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Cyan);
				lcd_fill(TOGGLE_CHILLER_VALVE_X1 + 1, TOGGLE_CHILLER_VALVE_Y1 + 1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Green);
				lcd_printf(12, 8, 13, "CHILLER");
				lcd_printf(12, 9, 13, "CLOSED");
			}

			hlt_last = HLTValveState;
			mash_last = MashValveState;
			inlet_last = InletValveState;
			chiller_last = ChillerValveState;
		}

		if (tog)
		{
			lcd_fill(1, 180, 180, 59, Black);

			lcd_printf(1, 12, 25, "HLT = %d.%ddegC", (unsigned int) floor(fHLTTemp), (unsigned int) ((fHLTTemp - floor(fHLTTemp)) * pow(10, 3)));
			lcd_printf(1, 13, 25, "MASH = %d.%ddegC", (unsigned int) floor(fMashTemp), (unsigned int) ((fMashTemp - floor(fMashTemp)) * pow(10, 3)));
			lcd_printf(1, 14, 25, "Currently @ %d.%d l", (unsigned int) floor(fGetBoilFlowLitres()), (unsigned int) ((fGetBoilFlowLitres() - floor(fGetBoilFlowLitres())) * pow(10, 3)));

		}
		else
		{
			lcd_fill(1, 180, 180, 59, Black);
		}

		tog = tog ^ 1;

		xSemaphoreGive(xValvesAppletRunningSemaphore);
		//give back the semaphore as its safe to return now.
		vTaskDelay(500);

	}
}

int iValvesKey(int xx, int yy)
{

	uint16_t window = 0;
	static uint8_t w = 5, h = 5;
	static uint16_t last_window = 0;

	if (xx > TOGGLE_HLT_VALVE_X1 + 1 && xx < TOGGLE_HLT_VALVE_X2 - 1 && yy > TOGGLE_HLT_VALVE_Y1 + 1 && yy < TOGGLE_HLT_VALVE_Y2 - 1)
	{
		vValveActuate(HLT_VALVE, TOGGLE_VALVE);

	}
	else if (xx > TOGGLE_MASH_VALVE_X1 + 1 && xx < TOGGLE_MASH_VALVE_X2 - 1 && yy > TOGGLE_MASH_VALVE_Y1 + 1 && yy < TOGGLE_MASH_VALVE_Y2 - 1)
	{
		vValveActuate(MASH_VALVE, TOGGLE_VALVE);

	}
	else if (xx > TOGGLE_INLET_VALVE_X1 + 1 && xx < TOGGLE_INLET_VALVE_X2 - 1 && yy > TOGGLE_INLET_VALVE_Y1 + 1 && yy < TOGGLE_INLET_VALVE_Y2 - 1)
	{
		vValveActuate(INLET_VALVE, TOGGLE_VALVE);
	}
	else if (xx > TOGGLE_CHILLER_VALVE_X1 + 1 && xx < TOGGLE_CHILLER_VALVE_X2 - 1 && yy > TOGGLE_CHILLER_VALVE_Y1 + 1 && yy < TOGGLE_CHILLER_VALVE_Y2 - 1)
	{
		vValveActuate(CHILLER_VALVE, TOGGLE_VALVE);
	}
	else if (xx > RESET_FLOW_1_X1 + 1 && xx < RESET_FLOW_1_X2 - 1 && yy > RESET_FLOW_1_Y1 + 1 && yy < RESET_FLOW_1_Y2 - 1)
	{
		vResetFlow1();
	}
	else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
	{
		//try to take the semaphore from the display applet. wait here if we cant take it.
		xSemaphoreTake(xValvesAppletRunningSemaphore, portMAX_DELAY);
		//delete the display applet task if its been created.
		if (xValvesAppletDisplayHandle != NULL )
		{
			vTaskDelete(xValvesAppletDisplayHandle);
			vTaskDelay(100);
			xValvesAppletDisplayHandle = NULL;
		}

		//return the semaphore for taking by another task.
		xSemaphoreGive(xValvesAppletRunningSemaphore);
		return 1;

	}

	vTaskDelay(10);
	return 0;

}


