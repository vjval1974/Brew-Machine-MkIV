/*
 * mill.c
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "mill.h"
#include "main.h"
#include "button.h"
#include "macros.h"

void vMillAppletDisplay(void *pvParameters);
void vMillApplet(int init);

xTaskHandle xMillTaskHandle = NULL, xMillAppletDisplayHandle = NULL;
xSemaphoreHandle xAppletRunningSemaphore;

volatile MillState xMillState = MILL_STOPPED;

MillState xGetGrainMillState()
{
	return xMillState;
}

void vMillInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = MILL_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(MILL_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(MILL_PORT, MILL_PIN ); //pull low
	xMillState = MILL_STOPPED;
	vSemaphoreCreateBinary(xAppletRunningSemaphore);
}

void vMill(MillState state)
{
	if (state == MILL_DRIVING)
		GPIO_WriteBit(MILL_PORT, MILL_PIN, 1);
	else
		GPIO_WriteBit(MILL_PORT, MILL_PIN, 0);
	xMillState = state;
}

int iMillStart()
{
	vMill(MILL_DRIVING);
	return 0;
}

int iMillStop()
{
	vMill(MILL_STOPPED);
	return 0;
}

#define START_MILL_X1 155
#define START_MILL_Y1 30
#define START_MILL_X2 300
#define START_MILL_Y2 100
#define START_MILL_W (START_MILL_X2-START_MILL_X1)
#define START_MILL_H (START_MILL_Y2-START_MILL_Y1)

#define STOP_MILL_X1 155
#define STOP_MILL_Y1 105
#define STOP_MILL_X2 300
#define STOP_MILL_Y2 175
#define STOP_MILL_W (STOP_MILL_X2-STOP_MILL_X1)
#define STOP_MILL_H (STOP_MILL_Y2-STOP_MILL_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

static int Back()
{
	return BackFromApplet(xAppletRunningSemaphore, xMillAppletDisplayHandle);
}

static Button MillButtons[] =
{
		{START_MILL_X1, START_MILL_Y1, START_MILL_X2, START_MILL_Y2, "Start", Blue, Green, iMillStart, ""},
		{STOP_MILL_X1, STOP_MILL_Y1, STOP_MILL_X2, STOP_MILL_Y2, "Stop", Blue, Red, iMillStop, ""},
		{BK_X1, BK_Y1, BK_X2, BK_Y2, "BACK", Cyan, Magenta, Back, ""},
};

static int ButtonCount()
{
	return ARRAY_LENGTH(MillButtons);
}

void vMillApplet(int init)
{
	lcd_text_xy(10 * 8, 0, "MANUAL MILL", White, Black);
	vDrawButtons(MillButtons, ButtonCount() );

	if (init)
	{
		//create a dynamic display task
		xTaskCreate( vMillAppletDisplay,
		    ( signed portCHAR * ) "Mill_disp",
		    configMINIMAL_STACK_SIZE + 500,
		    NULL,
		    tskIDLE_PRIORITY,
		    &xMillAppletDisplayHandle);
	}

}

void vMillAppletDisplay(void *pvParameters)
{
	static char tog = 0; //toggles each loop
	for (;;)
	{

		xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
		//take the semaphore so that the key handler wont
		//return to the menu system until its returned
		switch (xMillState)
		{
			case MILL_DRIVING:
				{
				if (tog)
				{
					lcd_fill(1, 220, 180, 29, Black);
					lcd_printf(1, 13, 15, "MILL DRIVING");
				}
				else
				{
					lcd_fill(1, 210, 180, 17, Black);
				}
				break;
			}
			case MILL_STOPPED:
				{
				if (tog)
				{
					lcd_fill(1, 210, 180, 29, Black);
					lcd_printf(1, 13, 11, "MILL STOPPED");
				}
				else
				{
					lcd_fill(1, 210, 180, 17, Black);
				}

				break;
			}
			default:
				{
				break;
			}
		}

		tog = tog ^ 1;
		xSemaphoreGive(xAppletRunningSemaphore);
		//give back the semaphore as its safe to return now.
		vTaskDelay(500);

	}
}

int iMillKey(int xx, int yy)
{

	int retVal = ActionKeyPress(MillButtons, ButtonCount(), xx, yy);
		vTaskDelay(10);
		return retVal;

}

