/*
 * chiller_pump.h
 *
 *  Created on: Aug 5, 2013
 *      Author: brad
 */

#ifndef CHILLER_PUMP_H_
#define CHILLER_PUMP_H_

//#define CHILLER_PUMP_PORT PORTU
//#define CHILLER_PUMP_PIN PCF_PIN1

#define CHILLER_PUMP_PORT GPIOC
#define CHILLER_PUMP_PIN GPIO_Pin_2
#define PUMPING 1
#define STOPPED -1
#define STOP -1
void vChillerPumpInit(void);
void vChillerPumpApplet(int init);
int iChillerPumpKey(int xx, int yy);
void vChillerPump(uint8_t state);
unsigned char ucGetChillerPumpState();

#endif /* CHILLER_PUMP_H_ */
