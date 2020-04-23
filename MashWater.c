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


typedef float Litres;

const char * MashTunWaterStates[] =
    {
        "MashTunDry",
        "MashTunWet"
     };




static char buf[50];

// Note: The HLT should take parameters for the amount of drain litres .. giving the responsibility of the caller to
// 		state the amount of litres required.
// ====================================================================================================================
//TODO: calcs show that witha 20.79l strike and 8.5kg of grain that -9.0something of water resides in the mash. Investigate

typedef enum
{
	MashTunDry,
	MashTunWet
} MashTunWaterState;

Litres WaterInMash = 0.0;
Litres WaterInBoiler = 0.0;

static MashTunWaterState xMashTunWaterState = MashTunDry;

// adding water
void WaterAddedToMashTun(float waterToAdd)
{
	if (xMashTunWaterState == MashTunDry)
	{
		WaterInMash = waterToAdd - (BrewParameters.fGrainWeightKilos * fWaterLossToMash_LitresPerKilo);
		xMashTunWaterState = MashTunWet;
	}
	else
		WaterInMash += waterToAdd;
}

// Pumping Out
void MashTunHasBeenDrained()
{
	WaterInBoiler += WaterInMash;
	WaterInMash = 0.0;
	//LitresCurrentlyInBoiler = LitresCurrentlyInMashTun;
	//LitresCurrentlyInMashTun = 0.0;
}
float fGetLitresCurrentlyInMashTun()
{
	return WaterInMash;
}

float fGetLitresCurrentlyInBoiler()
{
	return WaterInBoiler;
}

void printMashTunState()
{
	sprintf(buf, "MashTunState = %s, %dml \r\n\0", MashTunWaterStates[xMashTunWaterState], (int) (WaterInMash * 1000));
	vConsolePrint(buf);
	vTaskDelay(50);
}

void printLitresCurrentlyInBoiler()
{
	sprintf(buf, "Boiler contains, %dml \r\n\0", (int) (WaterInBoiler * 1000));
	vConsolePrint(buf);
	vTaskDelay(50);
}

void vClearMashAndBoilLitres()
{
	WaterInBoiler = 0.0;
	WaterInMash = 0.0;
}
// ====================================================================================================================
// ====================================================================================================================
