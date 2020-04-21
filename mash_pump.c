/*
 * mash_pump.c
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
#include "mash_pump.h"
#include "console.h"
#include "brew.h"
#include "crane.h"


MashPumpState_t MashPumpState = MASH_PUMP_STOPPED;

MashPumpState_t GetMashPumpState()
{
	return MashPumpState;
}

void vMashPumpInit(void)
{

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = MASH_PUMP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
	GPIO_Init(MASH_PUMP_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(MASH_PUMP_PORT, MASH_PUMP_PIN ); //pull low

}
int OkToPump() // crane has gotta be at the bottom, unless valve is opened.
{
	CraneState craneState = xGetCraneState();
	if((ThisBrewState.xRunningState == RUNNING && (craneState == CRANE_AT_BOTTOM || xGetValveState(&valves[MASH_VALVE]) == VALVE_OPENED)) ||	ThisBrewState.xRunningState == IDLE	)
	{
		return 1;
	}
	return 0;
}


void vMashPump(MashPumpCommand command)
{

	if (command == START_MASH_PUMP && MashPumpState != MASH_PUMP_PUMPING)
	{
		if (OkToPump()) // this could have issues if called from setup step. Test!
		{
			vConsolePrint("Mash Pump Starting\r\n\0");
			GPIO_SetBits(MASH_PUMP_PORT, MASH_PUMP_PIN );
			MashPumpState = MASH_PUMP_PUMPING;
		}
		else
		{
			vConsolePrint("Mash Pump NOT OK to pump\r\n\0");
		}

	}
	else if (command == STOP_MASH_PUMP && MashPumpState != MASH_PUMP_STOPPED)
	{
		vConsolePrint("Mash Pump Stopping\r\n\0");
		GPIO_ResetBits(MASH_PUMP_PORT, MASH_PUMP_PIN );
		MashPumpState = MASH_PUMP_STOPPED;
	}
}

void vToggleMashPump()
{

	if(MashPumpState != MASH_PUMP_PUMPING)
	{
		//printf("MPStart\r\n");
		vMashPump(START_MASH_PUMP);
	}
	else
		//printf("MPStop\r\n");
		vMashPump(STOP_MASH_PUMP);
}

