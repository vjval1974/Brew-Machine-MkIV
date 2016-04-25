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
unsigned char ucGetBoilState();
unsigned int uiGetBoilDuty();
uint8_t uGetBoilLevel(void);
extern xQueueHandle xBoilQueue;

typedef enum BoilLevel
{
  HIGH,
  LOW
} BoilLevel;



typedef struct BoilerState
{
  BoilLevel level;
  char levelStr[25];
  unsigned int duty;
  unsigned char dutyStr[6];
  unsigned char boil_state;
  char boilStateStr[25];
} BoilerState;

BoilerState GetBoilerState();


#define BOILING 1
#define OFF 0

#endif /* BOIL_H_ */
