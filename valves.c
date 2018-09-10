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
#include "macros.h"
#include "button.h"
#include "mash_pump.h"
#include "chiller_pump.h"

void vValvesAppletDisplay(void *pvParameters);
void vValvesApplet(int init);
static void UpdateValveButtons();


xTaskHandle xValvesTaskHandle = NULL, xValvesAppletDisplayHandle = NULL;
xSemaphoreHandle xValvesAppletRunningSemaphore;


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


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = MASH_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(MASH_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(MASH_VALVE_PORT, MASH_VALVE_PIN ); //pull low


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = INLET_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(INLET_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(INLET_VALVE_PORT, INLET_VALVE_PIN ); //pull low


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = CHILLER_VALVE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(CHILLER_VALVE_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN ); //pull low


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
			if (ucGetHltValveState() == VALVE_OPENED)
			{
				vSetBoilFlowMeasuringState(MEASURING_FLOW);
			}
			else vSetBoilFlowMeasuringState(NOT_MEASURING_FLOW);
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
#define TOGGLE_HLT_VALVE_Y1 20
#define TOGGLE_HLT_VALVE_X2 70
#define TOGGLE_HLT_VALVE_Y2 80


#define TOGGLE_MASH_VALVE_X1 75
#define TOGGLE_MASH_VALVE_Y1 20
#define TOGGLE_MASH_VALVE_X2 145
#define TOGGLE_MASH_VALVE_Y2 80

#define TOGGLE_MASH_PUMP_X1 150
#define TOGGLE_MASH_PUMP_Y1 20
#define TOGGLE_MASH_PUMP_X2 220
#define TOGGLE_MASH_PUMP_Y2 80


#define TOGGLE_INLET_VALVE_X1 0
#define TOGGLE_INLET_VALVE_Y1 85
#define TOGGLE_INLET_VALVE_X2 70
#define TOGGLE_INLET_VALVE_Y2 145


#define TOGGLE_CHILLER_VALVE_X1 75
#define TOGGLE_CHILLER_VALVE_Y1 85
#define TOGGLE_CHILLER_VALVE_X2 145
#define TOGGLE_CHILLER_VALVE_Y2 145

#define TOGGLE_BOIL_PUMP_X1 150
#define TOGGLE_BOIL_PUMP_Y1 85
#define TOGGLE_BOIL_PUMP_X2 220
#define TOGGLE_BOIL_PUMP_Y2 145

#define RESET_FLOW_1_X1 225
#define RESET_FLOW_1_Y1 20
#define RESET_FLOW_1_X2 318
#define RESET_FLOW_1_Y2 80

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235


int ToggleHltValve()
{
	vValveActuate(HLT_VALVE, TOGGLE_VALVE);
	return 0;
}

int ToggleMashValve()
{
	vValveActuate(MASH_VALVE, TOGGLE_VALVE);
	return 0;
}

int ToggleInletValve()
{
	vValveActuate(INLET_VALVE, TOGGLE_VALVE);
	return 0;
}
int ToggleChillerValve()
{
	vValveActuate(CHILLER_VALVE, TOGGLE_VALVE);
	return 0;
}

int ToggleBoilPump()
{
	vToggleChillerPump();
	return 0;
}
int ToggleMashPump()
{
	vToggleMashPump();
	return 0;
}
int ResetFlow()
{
	vResetFlow1();
	return 0;
}

int Back()
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

static Button ValveButtons[] =
{
		{TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, "HLT", Red, Blue, ToggleHltValve, ""},
		{TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, "Mash", Red, Blue, ToggleMashValve, ""},
		{TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, "Inlet", Red, Blue, ToggleInletValve, ""},
		{TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, "Boil", Red, Blue, ToggleChillerValve, ""},
		{TOGGLE_MASH_PUMP_X1, TOGGLE_MASH_PUMP_Y1, TOGGLE_MASH_PUMP_X2, TOGGLE_MASH_PUMP_Y2, "B PUMP", Red, Blue, ToggleMashPump, ""},
		{TOGGLE_BOIL_PUMP_X1, TOGGLE_BOIL_PUMP_Y1, TOGGLE_BOIL_PUMP_X2, TOGGLE_BOIL_PUMP_Y2, "Boil", Red, Blue, ToggleBoilPump, ""},
		{RESET_FLOW_1_X1, RESET_FLOW_1_Y1, RESET_FLOW_1_X2, RESET_FLOW_1_Y2, "Flow Reset", Red, Blue, ResetFlow, ""},
		{BK_X1, BK_Y1, BK_X2, BK_Y2, "BACK", Red, Blue, Back, ""},
};
typedef enum
{
	HltButton,
	MashButton,
	InletButton,
	ChillerButton,
	MashPumpButton,
	BoilPumpButton,
	ResetFlowButton,
	BackButton
} ButtonEnum;

static int ValveButtonCount()
{
	return ARRAY_LENGTH(ValveButtons);
}

void UpdateButton(Button * button, uint16_t fillColor, uint16_t outlineColor, const char * text)
{
	button->fillColor = fillColor;
	button->outlineColor = outlineColor;
	button->stateText = text;
}

static uint16_t valveButtonBackground = Red;

static void UpdateValveButtons()
{

	ValveState hLTValveState = ucGetHltValveState();
	ValveState mashValveState = ucGetMashValveState();
	ValveState inletValveState = ucGetInletValveState();
	ValveState chillerValveState = ucGetChillerValveState();
	MashPumpState_t mashPumpState = GetMashPumpState();
	ChillerPumpState_t chillerPumpState = GetChillerPumpState();

		if (hLTValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[HltButton], Red, Black, "To Mash");
		else
			UpdateButton(&ValveButtons[HltButton], Blue, Red, "To HLT");

		if (mashValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[MashButton], Red, Black, "To Boil");
		else
			UpdateButton(&ValveButtons[MashButton], Blue, Red, "To Mash");

		if (inletValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[InletButton], Red, Black, "Filling");
		else
			UpdateButton(&ValveButtons[InletButton], Blue, Red, "Closed");

		if (chillerValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[ChillerButton], Red, Black, "To Ferm");
		else
			UpdateButton(&ValveButtons[ChillerButton], Blue, Red, "To Boil");

		if (mashPumpState == MASH_PUMP_PUMPING)
			UpdateButton(&ValveButtons[MashPumpButton], Red, Black, "Pumping");
		else
			UpdateButton(&ValveButtons[MashPumpButton], Blue, Red, "Off");

		if (chillerPumpState == CHILLER_PUMP_PUMPING)
			UpdateButton(&ValveButtons[BoilPumpButton], Red, Black, "Pumping");
		else
			UpdateButton(&ValveButtons[BoilPumpButton], Blue, Red, "Off");

		uint16_t old = lcd_background(valveButtonBackground);
		vDrawButtons(ValveButtons, ValveButtonCount() );
		lcd_background(old);



}

void vValvesApplet(int init){
	 UpdateValveButtons();
  if (init)
    {


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
	unsigned int uiDecimalPlaces = 3;
	float fHLTTemp = 0, fMashTemp = 0;

	for (;;)
	{

		xSemaphoreTake(xValvesAppletRunningSemaphore, portMAX_DELAY);
		//take the semaphore so that the key handler wont
		//return to the menu system until its returned

		fHLTTemp = ds1820_get_temp(HLT);
		fMashTemp = ds1820_get_temp(MASH);

		UpdateValveButtons();

		lcd_printf(1, 12, 20, "HLT = %d.%ddegC", (unsigned int) floor(fHLTTemp), (unsigned int) ((fHLTTemp - floor(fHLTTemp)) * pow(10, 3)));
		lcd_printf(1, 13, 20, "MASH = %d.%ddegC", (unsigned int) floor(fMashTemp), (unsigned int) ((fMashTemp - floor(fMashTemp)) * pow(10, 3)));
		lcd_printf(1, 14, 20, "Currently @ %d.%d l", (unsigned int) floor(fGetBoilFlowLitres()), (unsigned int) ((fGetBoilFlowLitres() - floor(fGetBoilFlowLitres())) * pow(10, 3)));


		xSemaphoreGive(xValvesAppletRunningSemaphore);
		//give back the semaphore as its safe to return now.
		vTaskDelay(500);

	}
}


int iValvesKey(int xx, int yy)
{
	int retVal = ActionKeyPress(ValveButtons, ValveButtonCount(), xx, yy);
	vTaskDelay(10);
	return retVal;
}


