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
  //Grind
  BrewParameters.iGrindTime = 8;

  //Mash
  BrewParameters.fHLTMaxLitres = 22.0; // This is the max amount that can be drained
  BrewParameters.fStrikeTemp = 76.8;
  BrewParameters.fMashOutTemp = 99.9;
  BrewParameters.fSpargeTemp = 75.6;
  BrewParameters.fCleanTemp = 70.0; // less than the temp of a new strike. For 2 brews.
  BrewParameters.fStrikeLitres = 15.87;
  BrewParameters.fMashOutLitres = 14.3;
  BrewParameters.fSpargeLitres = 7.02;
  BrewParameters.iMashTime = 45;
  BrewParameters.iPumpTime1 = 5;
  BrewParameters.iStirTime1 = 15;
  BrewParameters.iPumpTime2 = 5;
  BrewParameters.iStirTime2 = 0;

  //Pump
  BrewParameters.iPumpPrimingCycles = 3;
  BrewParameters.iPumpPrimingTime = 2;

  //Boil
  BrewParameters.uiBoilTime = 2;//60;
  BrewParameters.uiBringToBoilTime = 1;
  BrewParameters.uiHopTimes[0] = 1;
  BrewParameters.uiHopTimes[1] = 1;
  BrewParameters.uiHopTimes[2] = 1;
  BrewParameters.uiHopTimes[3] = 1;
  BrewParameters.uiHopTimes[4] = 1;
  BrewParameters.uiHopTimes[5] = 1;

  //Chill
  BrewParameters.uiSettlingRecircTime = 2; //mins
  BrewParameters.uiSettlingTime = 6; //mins
  BrewParameters.uiChillTime = 8; //mins
  BrewParameters.uiChillerPumpPrimingCycles = 3;
  BrewParameters.uiChillerPumpPrimingTime = 3; //seconds
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
