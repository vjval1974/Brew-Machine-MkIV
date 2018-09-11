/*
 * hop_dropper.c
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */
#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "adc.h"
#include "ds1820.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include "task.h"
#include "semphr.h"
#include "hop_dropper.h"
#include "I2C-IO.h"
#include "io_util.h"
#include "queue.h"
#include "console.h"
#include "main.h"
#include "parameters.h"

typedef enum
{
	HOP_DROPPER_STOP,
	HOP_DROPPER_DRIVE
} HopDropperCommand;

typedef enum
{
	STOPPED,
	DRIVING_NO_GAP,
	DRIVING_GAP
} HopDropperState;

volatile uint8_t uState = STOPPED;
xSemaphoreHandle xHopAppletRunningSemaphore;
xTaskHandle xHopDropperAppletDisplayHandle = NULL;
xQueueHandle xHopsQueue;

void vHopsInit(void)
{
	vPCF_ResetBits(HOP_DROPPER_DRIVE_PIN, HOP_DROPPER_DRIVE_PORT); //pull low
	GPIO_InitTypeDef GPIO_InitStructure;

	// Set up the input pin configuration for PE4
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = HOP_DROPPER_LIMIT_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(HOP_DROPPER_LIMIT_PORT, &GPIO_InitStructure);
	vSemaphoreCreateBinary(xHopAppletRunningSemaphore);
}

void vHopsDrive(HopDropperCommand cmd)
{
	if (cmd == HOP_DROPPER_DRIVE)
	{
		vPCF_SetBits(HOP_DROPPER_DRIVE_PIN, HOP_DROPPER_DRIVE_PORT); //pull low
		//hop_dropper_state = ON;
	}
	else
	{
		vPCF_ResetBits(HOP_DROPPER_DRIVE_PIN, HOP_DROPPER_DRIVE_PORT); //pull low
		//hop_dropper_state = OFF;
	}
}

void vTaskHops(void * pvParameters)
{
	uint8_t uNext = 0;

	uint8_t uPinState;

	portTickType xLastWakeTime;
	const portTickType xFrequency = 10;
	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime = xTaskGetTickCount();

	xHopsQueue = xQueueCreate(5, sizeof(uint8_t));
	if (xHopsQueue == NULL )
		vConsolePrint("Can't Create Hops Queue\r\n\0");

	for (;;)
	{
		if (xQueueReceive(xHopsQueue, &uNext, portMAX_DELAY) == pdPASS)
		{
			vConsolePrint("Received Message\r\n\0");
			vHopsDrive(HOP_DROPPER_DRIVE);
			uState = DRIVING_NO_GAP;
			while (uState != STOPPED)
			{
				switch (uState)
				{
					case DRIVING_NO_GAP:
						{
						if (uDebounce(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN, PIN_STATE_LOW))
						{
							uState = DRIVING_GAP;
							vConsolePrint("State = DRIVING_GAP\r\n\0");
						}

						break;

					}
					case DRIVING_GAP:
						{
						if (uDebounce(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN, PIN_STATE_HIGH))

						{
							vTaskDelay(BrewParameters.uiHopDropperStopDelayms); // delay for lining up.

							vHopsDrive(HOP_DROPPER_STOP);
							vConsolePrint("State = STOPPED\r\n\0");
							uState = STOPPED;
						}

						break;
					}
				}                      //switch
				vTaskDelayUntil(&xLastWakeTime, xFrequency);
			}                      //While..
		} //xQueueReceive
	} //inf loop
} // func

void vHopDropperAppletDisplay(void *pvParameters)
{
	static char tog = 0; //toggles each loop

	for (;;)
	{

		xSemaphoreTake(xHopAppletRunningSemaphore, portMAX_DELAY);
		//take the semaphore so that the key handler wont
		//return to the menu system until its returned
		switch (uState)
		{
			case DRIVING_NO_GAP:
				{
				if (tog)
				{
					lcd_fill(1, 220, 180, 29, Black);
					lcd_printf(1, 13, 15, "DRIVING ON CUP");
				}
				else
				{
					lcd_fill(1, 210, 180, 17, Black);
				}
				break;
			}
			case DRIVING_GAP:
				{
				if (tog)
				{
					lcd_fill(1, 210, 180, 29, Black);
					lcd_printf(1, 13, 11, "DRIVING IN GAP");
				}
				else
				{
					lcd_fill(1, 210, 180, 17, Black);
				}

				break;
			}
			case STOPPED:
				{
				if (tog)
				{
					lcd_fill(1, 210, 180, 29, Black);
					lcd_printf(1, 13, 11, "STOPPED");
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
		xSemaphoreGive(xHopAppletRunningSemaphore);
		//give back the semaphore as its safe to return now.
		vTaskDelay(200);
	}
}

#define HOPS_NEXT_X1 0
#define HOPS_NEXT_Y1 30
#define HOPS_NEXT_X2 150
#define HOPS_NEXT_Y2 150
#define HOPS_NEXT_W (HOPS_NEXT_X2-HOPS_NEXT_X1)
#define HOPS_NEXT_H (HOPS_NEXT_Y2-HOPS_NEXT_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)
void vHopDropperApplet(int init)
{
	if (init)
	{
		lcd_DrawRect(HOPS_NEXT_X1, HOPS_NEXT_Y1, HOPS_NEXT_X2, HOPS_NEXT_Y2, Red);
		lcd_fill(HOPS_NEXT_X1 + 1, HOPS_NEXT_Y1 + 1, HOPS_NEXT_W, HOPS_NEXT_H, Blue);

		lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
		lcd_fill(BK_X1 + 1, BK_Y1 + 1, BK_W, BK_H, Magenta);
		lcd_printf(10, 1, 18, "MANUAL Hops APPLET");
		lcd_printf(1, 3, 11, "Increment the");
		lcd_printf(1, 4, 11, "Hop Dropper to");
		lcd_printf(1, 5, 11, "it's next");
		lcd_printf(1, 6, 11, "position");
		lcd_printf(30, 13, 4, "Back");
		xTaskCreate( vHopDropperAppletDisplay,
		    ( signed portCHAR * ) "hlt_disp",
		    configMINIMAL_STACK_SIZE + 500,
		    NULL,
		    tskIDLE_PRIORITY+1,
		    &xHopDropperAppletDisplayHandle);
	}
}

int iHopDropperKey(int xx, int yy)
{
	uint16_t window = 0;
	static uint8_t w = 5, h = 5;
	static uint16_t last_window = 0;
	if (xx > HOPS_NEXT_X1 + 1 && xx < HOPS_NEXT_X2 - 1 && yy > HOPS_NEXT_Y1 + 1 && yy < HOPS_NEXT_Y2 - 1)
	{
		xQueueSendToBack(xHopsQueue, (void *)1, 0);
	}
	else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
	{
		//try to take the semaphore from the display applet. wait here if we cant take it.
		xSemaphoreTake(xHopAppletRunningSemaphore, portMAX_DELAY);
		//delete the display applet task if its been created.
		if (xHopDropperAppletDisplayHandle != NULL )
		{
			vTaskDelete(xHopDropperAppletDisplayHandle);
			vTaskDelay(100);
			xHopDropperAppletDisplayHandle = NULL;
		}
		vHopsDrive(HOP_DROPPER_STOP);
		vTaskDelay(100);
		//return the semaphore for taking by another task.
		xSemaphoreGive(xHopAppletRunningSemaphore);
		return 1;
	}
	vTaskDelay(10);
	return 0;
}

