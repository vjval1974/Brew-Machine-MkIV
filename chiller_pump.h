/*
 * chiller_pump.h
 *
 *  Created on: Aug 5, 2013
 *      Author: brad
 */

#ifndef CHILLER_PUMP_H_
#define CHILLER_PUMP_H_

#define CHILLER_PUMP_PORT PORTU
#define CHILLER_PUMP_PIN PCF_PIN1

void vChillerPumpInit(void);
void vChillerPumpApplet(int init);
int iChillerPumpKey(int xx, int yy);

#endif /* CHILLER_PUMP_H_ */
