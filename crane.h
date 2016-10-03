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
#include "queue.h"

//now on I2C
#define CRANE_UPPER_LIMIT_PIN 1
#define CRANE_UPPER_LIMIT_PORT PORTV

#define CRANE_LOWER_LIMIT_PIN 2
#define CRANE_LOWER_LIMIT_PORT PORTV

// Crane outputs
#define CRANE_PORT PORTU
#define CRANE_PIN1 PCF_PIN6
#define CRANE_PIN2 PCF_PIN7


void vCraneInit(void);
void vCraneRun(uint16_t speed);
void vTaskCrane(void * pvParameters);
void vCraneApplet(int init);
int iCraneKey(int x, int y);
int8_t vGetCraneState(void);
unsigned char ucGetCranePosition();



typedef enum
{
	CRANE_UP,
	CRANE_DOWN,
	CRANE_DOWN_INCREMENTAL,
	CRANE_STOP
} CraneCommand;


typedef enum
{
	CRANE_AT_TOP,
	CRANE_AT_BOTTOM,
	CRANE_DRIVING_UP,
	CRANE_DRIVING_DOWN,
	CRANE_DRIVING_DOWN_IN_INCREMENTS,
	CRANE_STOPPED
} CraneState;


typedef struct
{
		CraneCommand xCommand;
		int iBrewStep;
		unsigned char ucFromTask;
} CraneMessage;

extern xTaskHandle xCraneTaskHandle, xCraneAppletDisplayHandle;
extern xQueueHandle xCraneQueue;

#endif

