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

#define CRANE_ENABLE_PIN GPIO_Pin_9
#define CRANE_STEP_PIN GPIO_Pin_8
#define CRANE_DIR_PIN GPIO_Pin_7
#define CRANE_PORT GPIOC

void vCraneInit(void);
void vCraneRun(uint16_t speed);
void manual_crane_applet(int init);
int manual_crane_key(int x, int y);

extern xTaskHandle xCraneTaskHandle, xCraneUpToLimitTaskHandle,	xCraneDnToLimitTaskHandle, xCraneAppletDisplayHandle;

#endif

