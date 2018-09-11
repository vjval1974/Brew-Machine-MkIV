/*
 * stir.c
 *
 *  Created on: Aug 24, 2013
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
#include "I2C-IO.h"
#include "stir.h"
#include "stirApplet.h"
#include "console.h"
#include "brew.h"
#include "crane.h"

volatile StirState xStirState;

void vStirInit(void)
{
	// The stirrer motor is on the i2c output board.
	vPCF_ResetBits(STIR_PIN, STIR_PORT); //pull low
	xStirState = STIR_STOPPED;
	vSemaphoreCreateBinary(xStirAppletRunningSemaphore);
}

int OkToStir() // gotta be in auto and either at the bottom or driving down for start of mash.
{
	CraneState craneState = xGetCraneState();
	if((ThisBrewState.xRunningState == RUNNING && (craneState == CRANE_DRIVING_DOWN_IN_INCREMENTS || craneState == CRANE_AT_BOTTOM)) ||	ThisBrewState.xRunningState == IDLE	)
	{
		return 1;
	}
	return 0;
}

void vStir(StirState state)
{

	if (state == STIR_DRIVING && xGetStirState() != STIR_DRIVING)
	{
		if (OkToStir())
		{
			vConsolePrint("Starting Stirrer\r\n\0");
			vPCF_SetBits(STIR_PIN, STIR_PORT);
		}
		else
		{
			vConsolePrint("Stir: NOT OK to stir!\r\n\0");
		}

	}
	else if ( state == STIR_STOPPED && xGetStirState() != STIR_STOPPED )
	{
		vConsolePrint("Stopping Stirrer\r\n\0");
		vPCF_ResetBits(STIR_PIN, STIR_PORT);
	}
	xStirState = state;

}

StirState xGetStirState(void)
{
	return xStirState;
}





