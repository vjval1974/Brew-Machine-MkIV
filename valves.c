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
#include "boil_valve.h"

void vValvesAppletDisplay(void *pvParameters);
void vValvesApplet(int init);
static void UpdateValveButtons();
void vOnOpenHlt();
void vOnCloseHlt();

xTaskHandle xValvesTaskHandle = NULL, xValvesAppletDisplayHandle = NULL;
xSemaphoreHandle xValvesAppletRunningSemaphore;

Valve valves[] =
{
	{"HLT", GPIOB, GPIO_Pin_0, VALVE_STATE_NOT_DEFINED, vOnOpenHlt, vOnCloseHlt},
	{"MASH", GPIOB, GPIO_Pin_1, VALVE_STATE_NOT_DEFINED, NULL, NULL},
	{"INLET", GPIOB, GPIO_Pin_5, VALVE_STATE_NOT_DEFINED, NULL, NULL},
	{"CHILLER", GPIOC, GPIO_Pin_9, VALVE_STATE_NOT_DEFINED, NULL, NULL}
};

ValveState xGetValveState(Valve * valve)
{
	if (GPIO_ReadInputDataBit(valve->GPIO_Port, valve->GPIO_Pin) == 0)
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
void vOpenValve(Valve * valve)
{
	GPIO_WriteBit(valve->GPIO_Port, valve->GPIO_Pin, 1);
	valve->state = VALVE_OPENED;
}

void vCloseValve(Valve * valve)
{

	GPIO_WriteBit(valve->GPIO_Port, valve->GPIO_Pin, 0);
	valve->state = VALVE_CLOSED;
}
void vOnOpenHlt()
{
	printf("OnOpen Called\r\n");
	vSetBoilFlowMeasuringState(MEASURING_FLOW);
}

void vOnCloseHlt()
{
	printf("OnOpen Called\r\n");
	vSetBoilFlowMeasuringState(NOT_MEASURING_FLOW);
}

void vActuateValve(Valve * valve, ValveCommand command)
{

	if (command == TOGGLE_VALVE)
	{
		valve->state = xGetValveState(valve);
		printf("toggle State = %d\r\n", valve->state);
		valve->state == VALVE_OPENED ? vCloseValve(valve) : vOpenValve(valve);

	}
	else if (command == OPEN_VALVE)
	{
		vOpenValve(valve);
	}
	else
	{
		vCloseValve(valve);
	}
	if (xGetValveState(valve) == VALVE_OPENED)
	{
		if (valve->onOpen != NULL)
			valve->onOpen();
	}
	else
	{
		if (valve->onClose != NULL)
			valve->onClose();
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

#define TOGGLE_BOIL_VALVE_X1 225
#define TOGGLE_BOIL_VALVE_Y1 85
#define TOGGLE_BOIL_VALVE_X2 318
#define TOGGLE_BOIL_VALVE_Y2 145

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235

int ToggleBoilValve()
{
    vToggleBoilValve();
    return 0;
}
int ToggleHltValve()
{
	vActuateValve(&valves[HLT_VALVE], TOGGLE_VALVE);
	return 0;
}

int ToggleMashValve()
{
	vActuateValve(&valves[MASH_VALVE], TOGGLE_VALVE);
	return 0;
}

int ToggleInletValve()
{
	vActuateValve(&valves[INLET_VALVE], TOGGLE_VALVE);
	return 0;
}
int ToggleChillerValve()
{
	vActuateValve(&valves[CHILLER_VALVE], TOGGLE_VALVE);
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

static int Back()
{
	return BackFromApplet(xValvesAppletRunningSemaphore, xValvesAppletDisplayHandle);
}


#define valveButtonBackground  		NavyBlue
#define valveButtonFillColorOpen  	NavyBlue
#define valveButtonFillColorClosed  Orange
#define pumpButtonFillColorOn  		Dark_Green
#define pumpButtonFillColorOff  	NavyBlue
#define buttonOutlineColor 			White

static Button ValveButtons[] =
{
		{TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, "HLT", valveButtonFillColorClosed, buttonOutlineColor, ToggleHltValve, ""},
		{TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, "Mash", valveButtonFillColorClosed, buttonOutlineColor, ToggleMashValve, ""},
		{TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, "Inlet", valveButtonFillColorClosed, buttonOutlineColor, ToggleInletValve, ""},
		{TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, "Chiller", valveButtonFillColorClosed, buttonOutlineColor, ToggleChillerValve, ""},
		{TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, "Boil", valveButtonFillColorClosed, buttonOutlineColor, ToggleBoilValve, ""},
		{TOGGLE_MASH_PUMP_X1, TOGGLE_MASH_PUMP_Y1, TOGGLE_MASH_PUMP_X2, TOGGLE_MASH_PUMP_Y2, "MashPump", pumpButtonFillColorOff, buttonOutlineColor, ToggleMashPump, ""},
		{TOGGLE_BOIL_PUMP_X1, TOGGLE_BOIL_PUMP_Y1, TOGGLE_BOIL_PUMP_X2, TOGGLE_BOIL_PUMP_Y2, "BoilPump", pumpButtonFillColorOff, buttonOutlineColor, ToggleBoilPump, ""},
		{RESET_FLOW_1_X1, RESET_FLOW_1_Y1, RESET_FLOW_1_X2, RESET_FLOW_1_Y2, "Flow Rst", valveButtonBackground, buttonOutlineColor, ResetFlow, ""},
		{BK_X1, BK_Y1, BK_X2, BK_Y2, "BACK", Red, Blue, Back, ""}
};
typedef enum
{
	HltButton,
	MashButton,
	InletButton,
	ChillerButton,
	BoilButton,
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


static void UpdateValveButtons()
{

	ValveState hLTValveState = xGetValveState(&valves[HLT_VALVE]);
	ValveState mashValveState = xGetValveState(&valves[MASH_VALVE]);
	ValveState inletValveState = xGetValveState(&valves[INLET_VALVE]);
	ValveState chillerValveState = xGetValveState(&valves[CHILLER_VALVE]);
	MashPumpState_t mashPumpState = GetMashPumpState();
	ChillerPumpState_t chillerPumpState = GetChillerPumpState();
	BoilValveState boilValveState = xGetBoilValveState();

		if (hLTValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[HltButton], valveButtonFillColorOpen, buttonOutlineColor, "To Mash");
		else
			UpdateButton(&ValveButtons[HltButton], valveButtonFillColorClosed, buttonOutlineColor, "To HLT");

		if (mashValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[MashButton], valveButtonFillColorOpen, buttonOutlineColor, "To Boil");
		else
			UpdateButton(&ValveButtons[MashButton], valveButtonFillColorClosed, buttonOutlineColor, "To Mash");

		if (inletValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[InletButton], valveButtonFillColorOpen, buttonOutlineColor, "Filling");
		else
			UpdateButton(&ValveButtons[InletButton], valveButtonFillColorClosed, buttonOutlineColor, "Closed");

		if (chillerValveState == VALVE_OPENED)
			UpdateButton(&ValveButtons[ChillerButton], valveButtonFillColorOpen, buttonOutlineColor, "Open");
		else
			UpdateButton(&ValveButtons[ChillerButton], valveButtonFillColorClosed, buttonOutlineColor, "Closed");

		if (mashPumpState == MASH_PUMP_PUMPING)
			UpdateButton(&ValveButtons[MashPumpButton], pumpButtonFillColorOn, buttonOutlineColor, "Pumping");
		else
			UpdateButton(&ValveButtons[MashPumpButton], pumpButtonFillColorOff, buttonOutlineColor, "Off");

		if (chillerPumpState == CHILLER_PUMP_PUMPING)
			UpdateButton(&ValveButtons[BoilPumpButton], pumpButtonFillColorOn, buttonOutlineColor, "Pumping");
		else
			UpdateButton(&ValveButtons[BoilPumpButton], pumpButtonFillColorOff, buttonOutlineColor, "Off");

		if (boilValveState == BOIL_VALVE_OPENED)
		{
			UpdateButton(&ValveButtons[BoilButton], valveButtonFillColorOpen, White, pcGetBoilValveStateText(boilValveState));
		}
		else
			UpdateButton(&ValveButtons[BoilButton], valveButtonFillColorClosed, White, pcGetBoilValveStateText(boilValveState));

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


