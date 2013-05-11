/*
 * Flow1.h
 *
 *  Created on: May 9, 2013
 *      Author: brad
 */

#ifndef FLOW1_H_
#define FLOW1_H_

void vFlow1Init(void);
void vResetFlow1Pulses(void);

void vTaskLitresDelivered( void * pvParameters );
int iFlow1Key(int xx, int yy);
void vFlow1Applet(int init);
float fGetFlow1Litres(void);

#endif /* FLOW1_H_ */
