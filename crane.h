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
#define CRANE_ENABLE_PIN GPIO_Pin_12
#define CRANE_DIR_PIN GPIO_Pin_14

//---CRANE STEPPER DRIVE PORT/PIN
#define CRANE_DRIVE_PORT GPIOC
#define CRANE_STEP_PIN GPIO_Pin_8

//---STIR CONTROL PORTS/PINS
#define CRANE_STIR_CONTROL_PORT GPIOA
#define CRANE_STIR_ENABLE_PIN GPIO_Pin_0
#define CRANE_STIR_DIR_PIN GPIO_Pin_1

//---STIR STEPPER DRIVE PORT/PIN
#define CRANE_STIR_DRIVE_PORT GPIOC
#define CRANE_STIR_STEP_PIN GPIO_Pin_9

//---CRANE LIMITS PORT/PINS
#define CRANE_UPPER_LIMIT_PIN GPIO_Pin_4
#define CRANE_LOWER_LIMIT_PIN GPIO_Pin_0
#define CRANE_LIMIT_PORT GPIOA




void vCraneInit(void);
void vCraneRun(uint16_t speed);
void manual_crane_applet(int init);
int manual_crane_key(int x, int y);

extern xTaskHandle xCraneTaskHandle, xCraneUpToLimitTaskHandle,	xCraneDnToLimitTaskHandle, xCraneAppletDisplayHandle;

#endif

