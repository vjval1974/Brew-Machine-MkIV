///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012, Brad Goold, All Rights Reserved.
//
// Authors: Brad Goold
//
// Date: 18 Feb 2012
//
// RCS $Date$
// RCS $Revision$
// RCS $Source$
// RCS $Log$///////////////////////////////////////////////////////////////////////////////

#ifndef CRANE_H
#define CRANE_H

#include "FreeRTOS.h"
#include "task.h"

//---CRANE CONTROL PORTS/PINS
#define CRANE_CONTROL_PORT GPIOB
#define CRANE_ENABLE_PIN GPIO_Pin_14
#define CRANE_DIR_PIN GPIO_Pin_12

//---CRANE STEPPER DRIVE PORT/PIN
#define CRANE_DRIVE_PORT GPIOC
#define CRANE_STEP_PIN GPIO_Pin_8


//---CRANE LIMITS PORT/PINS
#define CRANE_UPPER_LIMIT_PIN GPIO_Pin_4
#define CRANE_LOWER_LIMIT_PIN GPIO_Pin_0
#define CRANE_LIMIT_PORT GPIOA


#define CRANE_PORT PORTU
#define CRANE_PIN1 PCF_PIN6
#define CRANE_PIN2 PCF_PIN7


void vCraneInit(void);
void vCraneRun(uint16_t speed);
void vCraneApplet(int init);
int iCraneKey(int x, int y);

extern xTaskHandle xCraneTaskHandle, xCraneUpToLimitTaskHandle,	xCraneDnToLimitTaskHandle, xCraneAppletDisplayHandle;

#endif

