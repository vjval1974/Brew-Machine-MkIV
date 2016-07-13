/*
 * MashWater.h
 *
 *  Created on: Jul 12, 2016
 *      Author: ubuntu
 */

#ifndef MASHWATER_H_
#define MASHWATER_H_


void MashTunFillingSetpointReached(float litresDelivered);

void MashTunPumpingOut();
void MashTunFinishedPumpingOut();
void MashWaterStateMachinePoll(); // dodgy but called from brew task atm
void printMashTunState(); // prints to console.

float fGetLitresCurrentlyInMashTun();
float fGetLitresCurrentlyInBoiler();

#endif /* MASHWATER_H_ */
