/*
 * boil.h
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */

#ifndef BOIL_H_
#define BOIL_H_
#include "queue.h"
void vBoilInit(void);
void vBoilApplet(int init);
int iBoilKey(int xx, int yy);
extern xQueueHandle xBoilQueue;

#endif /* BOIL_H_ */
