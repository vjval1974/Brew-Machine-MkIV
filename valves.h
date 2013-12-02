/*
 * valves.h
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */

#ifndef VALVES_H_
#define VALVES_H_

#define HLT_VALVE 0
#define MASH_VALVE 1
#define BOIL_VALVE 2
#define INLET_VALVE 3
#define CHILLER_VALVE 4

#define CLOSED 11
#define OPENED 10
#define OPEN 0
#define CLOSE 1


#define HLT_VALVE_PORT GPIOB
#define MASH_VALVE_PORT GPIOB
#define INLET_VALVE_PORT GPIOB
#define CHILLER_VALVE_PORT GPIOC

#define HLT_VALVE_PIN GPIO_Pin_0
#define MASH_VALVE_PIN GPIO_Pin_1
#define INLET_VALVE_PIN GPIO_Pin_5
#define CHILLER_VALVE_PIN GPIO_Pin_9


void vValvesInit(void);
void vValvesApplet(int init);
int iValvesKey(int xx, int yy);
void vValveActuate(unsigned char valve, unsigned char state);



#endif /* VALVES_H_ */
