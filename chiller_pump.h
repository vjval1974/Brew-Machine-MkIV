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

typedef enum
{
    CHILLER_PUMP_UNDEFINED,
    CHILLER_PUMP_PUMPING,
    CHILLER_PUMP_STOPPED
} ChillerPumpState_t;

typedef enum
{
  START_CHILLER_PUMP,
  STOP_CHILLER_PUMP
} ChillerPumpCommand;

void vChillerPumpInit(void);
void vChillerPumpApplet(int init);
int iChillerPumpKey(int xx, int yy);
void vChillerPump(ChillerPumpCommand command);
ChillerPumpState_t GetChillerPumpState();
void vToggleChillerPump();

#endif /* CHILLER_PUMP_H_ */
