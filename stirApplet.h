/*
 * stirApplet.h
 *
 *  Created on: 14/12/2014
 *      Author: brad
 */

#ifndef STIRAPPLET_H_
#define STIRAPPLET_H_

void vStirApplet(int init);
int iStirKey(int xx, int yy);
#include "FreeRTOS.h"
#include "semphr.h"
extern xSemaphoreHandle xStirAppletRunningSemaphore;

#endif /* STIRAPPLET_H_ */
