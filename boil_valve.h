/*
 * boil_valve.h
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */

#ifndef BOIL_VALVE_H_
#define BOIL_VALVE_H_


// Boil Valve outputs
#define BOIL_VALVE_PORT PORTU
#define BOIL_VALVE_PIN1 PCF_PIN1
#define BOIL_VALVE_PIN2 PCF_PIN2


// Boil Valve inputs
#define BOIL_VALVE_CLOSED_PORT GPIOE
#define BOIL_VALVE_OPENED_PORT GPIOE
#define BOIL_VALVE_CLOSED_PIN GPIO_Pin_2
#define BOIL_VALVE_OPENED_PIN GPIO_Pin_3


void vBoilValveInit();
void vTaskBoilValve(void * pvParameters);
int iBoilValveKey(int xx, int yy);
void vBoilValveApplet(int init);

extern xQueueHandle xBoilValveQueue;




#endif /* BOIL_VALVE_H_ */
