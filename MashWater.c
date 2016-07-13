/*
 * MashWater.c
 *
 *  Created on: Jul 12, 2016
 *      Author: ubuntu
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "brew.h"
#include "hlt.h"
#include "main.h"
#include "MashWater.h"
#include "console.h"
#include "parameters.h"

const float fWaterLossToMash_LitresPerKilo = 1.13; // litres per kilo soaked into the grains
const float fLitresRemainingInMashAfterPumpOut = 2.50; //this cant be pumped out by the pump.
/*
 * example:
 * 5kg of grain.
 * Mash in with 15l of water.
 * Water Loss to grain = (1.13 * 5.0)
 * Water Loss to mash tun = 2.50
 * Pump out real estimate = 15 - (2.5 + (1.13 * 5.0))
 * 						  = 15 - 8.15 = 6.85
 *
 * Litres currently in Mash tun = 2.5
 * Drain for Sparge 1 with 10l
 * Pump out 10l goes to boil 2.5 stays
 * Drain for Sparge 2 with 10l
 * Pump out 10l goes to boil 2.5 stays
 * Therefore complete loss to mash tun and grain = 8.15l
 */


typedef float MashTunLitres;
typedef enum
{
	MASH_TUN_EMPTY,
	MASH_TUN_FILLING_FIRST_TIME,
	MASH_TUN_CONTAINS_WATER,
	MASH_TUN_FILLING,
	MASH_TUN_PUMPING_OUT,
	MASH_TUN_EMPTY_WET

} MashTunWaterState;

const char * MashTunWaterStates[] =
{
	"Empty",
	"Filling First",
	"Contains Water",
	"Filling",
	"Pumping Out",
	"Empty Wet",
};



MashTunLitres LitresCurrentlyInMashTun = 0.0;
MashTunLitres LitresCurrentlyInBoiler = 0.0;
MashTunWaterState WaterState = MASH_TUN_EMPTY;
volatile bool IsMashTunCompletedFilling= FALSE;
volatile bool IsMashTunPumpingOut = FALSE;
volatile bool WaterLostToGrainHasBeenSubtracted = FALSE;
volatile bool WaterLostToMashTunHasBeenSubtracted = FALSE;

MashTunLitres CalculateWaterLossFromGrainWeight()
{
	return fWaterLossToMash_LitresPerKilo * BrewParameters.fGrainWeightKilos;
}

void MashTunFillingSetpointReached(float litresDelivered)
{
	LitresCurrentlyInMashTun += litresDelivered;
	IsMashTunCompletedFilling = TRUE;
}

void MashTunPumpingOut()
{
	IsMashTunPumpingOut = TRUE;
}

void MashTunFinishedPumpingOut()
{
	IsMashTunPumpingOut = FALSE;
}



void MashWaterStateMachinePoll()
{
	static MashTunLitres LitresToSubtractForGrainWaterLoss = 0.0;
	static MashTunLitres LitresToSubtractForMashTunLoss = 0.0;
	switch (WaterState)
	{
		case MASH_TUN_EMPTY:
		{
			if (BrewState.ucHLTState == HLT_STATE_DRAIN)
			{
				WaterState = MASH_TUN_FILLING_FIRST_TIME;
			}
			break;
		}
		case MASH_TUN_FILLING_FIRST_TIME:
		{
			if (IsMashTunCompletedFilling == TRUE)
			{
				WaterState = MASH_TUN_CONTAINS_WATER;
				IsMashTunCompletedFilling  = FALSE;
			}
			break;
		}
		case MASH_TUN_CONTAINS_WATER:
		{
			if (LitresCurrentlyInMashTun <= 0.2)
			{
				//Something went wrong here
				vConsolePrint("Mash tun has no water, but state is ContainsWater\r\n");
			}
			if (WaterLostToGrainHasBeenSubtracted == FALSE)
			{
				LitresToSubtractForGrainWaterLoss = CalculateWaterLossFromGrainWeight();
				WaterLostToGrainHasBeenSubtracted = TRUE;
				LitresCurrentlyInMashTun -= LitresToSubtractForGrainWaterLoss;
			}
			if (BrewState.ucHLTState == HLT_STATE_DRAIN)
			{
				WaterState = MASH_TUN_FILLING;
			}
			if (IsMashTunPumpingOut == TRUE)
			{
				WaterState = MASH_TUN_PUMPING_OUT;

			}
			break;
		}
		case MASH_TUN_FILLING:
		{
			if (IsMashTunCompletedFilling == TRUE)
			{
				WaterState = MASH_TUN_CONTAINS_WATER;
				IsMashTunCompletedFilling  = FALSE;
			}
			break;
		}
		case MASH_TUN_PUMPING_OUT:
		{
			if (IsMashTunPumpingOut == FALSE)
			{
				WaterState = MASH_TUN_EMPTY_WET;
				if (WaterLostToMashTunHasBeenSubtracted == FALSE)
				{
					LitresCurrentlyInBoiler += LitresCurrentlyInMashTun - fLitresRemainingInMashAfterPumpOut;
					WaterLostToMashTunHasBeenSubtracted = TRUE;
				}
				else
					LitresCurrentlyInBoiler += LitresCurrentlyInMashTun;

				LitresCurrentlyInMashTun = 0.0; // disregarding the 2.5l that we cant pump out.
			}
			break;
		}
		case MASH_TUN_EMPTY_WET:
		{

			if (BrewState.ucHLTState == HLT_STATE_DRAIN)
			{
				WaterState = MASH_TUN_FILLING;
			}
			break;
		}
	}

}

static char buf [50];

void printMashTunState()
{
	sprintf(buf, "MashTunState = %s, %dml \r\n", MashTunWaterStates[WaterState], (int)(LitresCurrentlyInMashTun*1000));
	vConsolePrint(buf);
}

float fGetLitresCurrentlyInMashTun()
{
	return LitresCurrentlyInMashTun;
}

float fGetLitresCurrentlyInBoiler()
{
	return LitresCurrentlyInBoiler;
}
