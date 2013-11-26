/*
 * parameters.h
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

struct Parameters {
  //HLT
  float fHLTMaxLitres; // Maximum amount of litres at high level probe
  float fStrikeTemp;
  float fMashOutTemp;
  float fSpargeTemp;
  float fStrikeLitres;
  float fMashOutLitres;
  float fSpargeLitres;

  //Mash
  int iMashTime; // Minutes
  int iPumpTime1; // How long to pump for at the start of the mash
  int iPumpTime2; // "             "             end       "
  int iStirTime1; // "           stir     "       start     "
  int iStirTime2; // "            "               end

  //Boil
  unsigned char ucBoilTime;
  unsigned char ucHopTimes[6];

};

extern struct Parameters BrewParameters;

void vParametersInit(void);


#endif /* PARAMETERS_H_ */
