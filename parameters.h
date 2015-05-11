/*
 * parameters.h
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

struct Parameters {
  //Mill
  int iGrindTime; //mins

  //Pump
  int iPumpPrimingCycles; // amount of cycles to prime the pump
  int iPumpPrimingTime; //seconds

  //HLT
  float fHLTMaxLitres; // Maximum amount of litres at high level probe
  float fStrikeTemp;
  float fMashOutTemp;
  float fSpargeTemp;
  float fCleanTemp;
  float fStrikeLitres;
  float fMashOutLitres;
  float fSpargeLitres;

  //Mash
  int iMashTime; // Minutes
  int iPumpTime1; // How long to pump for at the start of the mash
  int iPumpTime2; // "             "             end       "
  int iStirTime1; // "           stir     "       start     "
  int iStirTime2; // "            "               end


  //Mash out
    int iMashOutTime; // Minutes
    int iMashOutPumpTime1; // How long to pump for at the start of the mash out
    int iMashOutPumpTime2; // "             "             end       "
    int iMashOutStirTime1; // "           stir     "       start     "
    int iMashOutStirTime2; // "            "               end

    //Mash out
       int iSpargeTime; // Minutes
       int iSpargePumpTime1; // How long to pump for at the start of the sparge
       int iSpargePumpTime2; // "             "             end       "
       int iSpargeStirTime1; // "           stir     "       start     "
       int iSpargeStirTime2; // "            "               end

  //Boil
  unsigned int uiBoilTime;
  unsigned int uiBringToBoilTime;
  unsigned int uiHopTimes[6];

  //Chill etc
  unsigned int uiSettlingTime; //settling time after the boil (for hop steep) mins
  unsigned int uiSettlingRecircTime; //recirc for valve sterilisation. mins
  unsigned int uiChillerPumpPrimingCycles;
  unsigned int uiChillerPumpPrimingTime;
  unsigned int uiChillTime; //time it takes to run through the chiller. mins

};

extern struct Parameters BrewParameters;

int iParametersKey(int xx, int yy);
void vParametersApplet(int init);
void vParametersInit(void);


#endif /* PARAMETERS_H_ */
