/*
 * hlt_pump.h
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

#ifndef HLT_PUMP_H_
#define HLT_PUMP_H_



#define HLT_PUMP_PORT GPIOC
#define HLT_PUMP_PIN GPIO_Pin_2

void vHLTPumpInit(void);
void vHLTPumpApplet(int init);
int iHLTPumpKey(int xx, int yy);
#endif /* HLT_PUMP_H_ */
