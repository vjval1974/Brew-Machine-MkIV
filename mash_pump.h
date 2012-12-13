/*
 * mash_pump.h
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

#ifndef MASH_PUMP_H_
#define MASH_PUMP_H_

#define MASH_PUMP_PORT GPIOC
#define MASH_PUMP_PIN GPIO_Pin_3

void vMashPumpInit(void);
void vMashPumpApplet(int init);
int iMashPumpKey(int xx, int yy);

#endif /* MASH_PUMP_H_ */
