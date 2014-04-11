/*
 * parameters.c
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */


#include "parameters.h"


struct Parameters BrewParameters;

// BREW PARAMETERS
void vParametersInit(void)
{
  BrewParameters.fHLTMaxLitres = 22.0; // This is the max amount that can be drained
  BrewParameters.fStrikeTemp = 75.9;
  BrewParameters.fMashOutTemp = 99.4;
  BrewParameters.fSpargeTemp = 75.6;
  BrewParameters.fStrikeLitres = 15.5;
  BrewParameters.fMashOutLitres = 14.0;
  BrewParameters.fSpargeLitres = 9.57;
  BrewParameters.iMashTime = 45;
  BrewParameters.iPumpTime1 = 5;
  BrewParameters.iStirTime1 = 15;
  BrewParameters.iPumpTime2 = 5;
  BrewParameters.iStirTime2 = 0;
  BrewParameters.uiBoilTime = 60;
  BrewParameters.uiBringToBoilTime = 25;
  BrewParameters.uiHopTimes[0] = 60;
  BrewParameters.uiHopTimes[1] = 45;
  BrewParameters.uiHopTimes[2] = 30;
  BrewParameters.uiHopTimes[3] = 10;
  BrewParameters.uiHopTimes[4] = 5;
  BrewParameters.uiHopTimes[5] = 1;

}

//void vParametersInit(void)
//{
//  BrewParameters.fHLTMaxLitres = 22.0;
//  BrewParameters.fStrikeTemp = 43.0;
//  BrewParameters.fMashOutTemp = 44.0;
//  BrewParameters.fSpargeTemp = 45.0;
//  BrewParameters.fStrikeLitres = 15.0;
//  BrewParameters.fMashOutLitres = 15.0;
//  BrewParameters.fSpargeLitres = 3.0;
//  BrewParameters.iMashTime = 5;
//  BrewParameters.iPumpTime1 = 5;
//  BrewParameters.iStirTime1 = 15;
//  BrewParameters.iPumpTime2 = 5;
//  BrewParameters.iStirTime2 = 0;
//  BrewParameters.uiBoilTime = 20;
//  BrewParameters.uiBringToBoilTime = 20;
//  BrewParameters.uiHopTimes[0] = 3;
//  BrewParameters.uiHopTimes[1] = 3;
//  BrewParameters.uiHopTimes[2] = 3;
//  BrewParameters.uiHopTimes[3] = 2;
//  BrewParameters.uiHopTimes[4] = 2;
//  BrewParameters.uiHopTimes[5] = 1;
//
//}

// CLEAN PARAMETERS
//void vParametersInit(void)
//{
//  BrewParameters.fHLTMaxLitres = 22.0;
//  BrewParameters.fStrikeTemp = 43.0;
//  BrewParameters.fMashOutTemp = 70.0;
//  BrewParameters.fSpargeTemp = 70.0;
//  BrewParameters.fStrikeLitres = 15.0;
//  BrewParameters.fMashOutLitres = 15.0;
//  BrewParameters.fSpargeLitres = 20.0;
//  BrewParameters.iMashTime = 5;
//  BrewParameters.iPumpTime1 = 5;
//  BrewParameters.iStirTime1 = 15;
//  BrewParameters.iPumpTime2 = 5;
//  BrewParameters.iStirTime2 = 0;
//  BrewParameters.uiBoilTime = 20;
//  BrewParameters.uiBringToBoilTime = 10;
//  BrewParameters.uiHopTimes[0] = 3;
//  BrewParameters.uiHopTimes[1] = 3;
//  BrewParameters.uiHopTimes[2] = 3;
//  BrewParameters.uiHopTimes[3] = 2;
//  BrewParameters.uiHopTimes[4] = 2;
//  BrewParameters.uiHopTimes[5] = 1;
//
//}
