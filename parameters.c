/*
 * parameters.c
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */


#include "parameters.h"


struct Parameters BrewParameters;


//void vParametersInit(void)
//{
//  BrewParameters.fHLTMaxLitres = 23.5;
//  BrewParameters.fStrikeTemp = 78.0;
//  BrewParameters.fMashOutTemp = 100.0;
//  BrewParameters.fSpargeTemp = 88.0;
//  BrewParameters.fStrikeLitres = 14.0;
//  BrewParameters.fMashOutLitres = 16.0;
//  BrewParameters.fSpargeLitres = 8.0;
//  BrewParameters.iMashTime = 60;
//  BrewParameters.iPumpTime1 = 5;
//  BrewParameters.iStirTime1 = 15;
//  BrewParameters.iPumpTime2 = 5;
//  BrewParameters.iStirTime2 = 0;
//  BrewParameters.uiBoilTime = 90;
//  BrewParameters.uiBringToBoilTime = 30;
//  BrewParameters.uiHopTimes[0] = 90;
//  BrewParameters.uiHopTimes[1] = 60;
//  BrewParameters.uiHopTimes[2] = 45;
//  BrewParameters.uiHopTimes[3] = 30;
//  BrewParameters.uiHopTimes[4] = 15;
//  BrewParameters.uiHopTimes[5] = 5;
//
//}

void vParametersInit(void)
{
  BrewParameters.fHLTMaxLitres = 23.5;
  BrewParameters.fStrikeTemp = 28.0;
  BrewParameters.fMashOutTemp = 38.0;
  BrewParameters.fSpargeTemp = 40.0;
  BrewParameters.fStrikeLitres = 3.0;
  BrewParameters.fMashOutLitres = 3.0;
  BrewParameters.fSpargeLitres = 3.0;
  BrewParameters.iMashTime = 60;
  BrewParameters.iPumpTime1 = 5;
  BrewParameters.iStirTime1 = 15;
  BrewParameters.iPumpTime2 = 5;
  BrewParameters.iStirTime2 = 0;
  BrewParameters.uiBoilTime = 6;
  BrewParameters.uiBringToBoilTime = 3;
  BrewParameters.uiHopTimes[0] = 6;
  BrewParameters.uiHopTimes[1] = 5;
  BrewParameters.uiHopTimes[2] = 4;
  BrewParameters.uiHopTimes[3] = 4;
  BrewParameters.uiHopTimes[4] = 3;
  BrewParameters.uiHopTimes[5] = 2;

}
