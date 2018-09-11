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

void printMashTunState(); // prints to console.
void printLitresCurrentlyInBoiler();

float fGetLitresCurrentlyInMashTun();
float fGetLitresCurrentlyInBoiler();

void MashTunHasBeenDrained();
void WaterAddedToMashTun(float waterToAdd);
void vClearMashAndBoilLitres();
#endif /* MASHWATER_H_ */
