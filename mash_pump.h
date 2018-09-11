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

typedef enum
{
  MASH_PUMP_UNDEFINED,
  MASH_PUMP_PUMPING,
  MASH_PUMP_STOPPED
} MashPumpState_t;

typedef enum
{
  START_MASH_PUMP,
  STOP_MASH_PUMP
} MashPumpCommand;

#define PUMPING 1
#define STOPPED -1
#define STOP -1

void vMashPumpInit(void);
void vMashPumpApplet(int init);
int iMashPumpKey(int xx, int yy);
void vMashPump(MashPumpCommand command);
MashPumpState_t GetMashPumpState();
void vToggleMashPump();

#endif /* MASH_PUMP_H_ */
