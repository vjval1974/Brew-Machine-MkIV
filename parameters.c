/*
 * parameters.c
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */


#include "parameters.h"


struct Parameters BrewParameters;


void vParametersInit(void)
{
  BrewParameters.fHLTMaxLitres = 23.5;
  BrewParameters.fStrikeTemp = 78.0;
  BrewParameters.fMashOutTemp = 100.0;
  BrewParameters.fSpargeTemp = 88.0;
  BrewParameters.fStrikeLitres = 14.0;
  BrewParameters.fMashOutLitres = 10.0;
  BrewParameters.fSpargeLitres = 9.0;
  BrewParameters.iMashTime = 60;
  BrewParameters.iPumpTime1 = 5;
  BrewParameters.iStirTime1 = 15;
  BrewParameters.iPumpTime2 = 5;
  BrewParameters.iStirTime2 = 0;
  BrewParameters.ucBoilTime = 90;
  BrewParameters.ucHopTimes[0] = 90;
  BrewParameters.ucHopTimes[1] = 60;
  BrewParameters.ucHopTimes[2] = 45;
  BrewParameters.ucHopTimes[3] = 30;
  BrewParameters.ucHopTimes[4] = 15;
  BrewParameters.ucHopTimes[5] = 5;

}
