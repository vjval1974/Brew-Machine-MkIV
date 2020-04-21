/*
 * button.h
 *
 *  Created on: Sep 9, 2018
 *      Author: ubuntu
 */

#ifndef BUTTON_H_
#define BUTTON_H_
#include <stdlib.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "queue.h"

typedef struct
{
	uint16_t x1;
	uint16_t y1;
	uint16_t x2;
	uint16_t y2;
	const char * text;
	uint16_t outlineColor;
	uint16_t fillColor;
	int (* action)();
	const char * stateText;
} Button;

void vDrawButton(Button button);
void vDrawButtons(Button buttons[], int count);
//static int ButtonHasBeenPressed(Button button, int xx, int yy);
int ActionKeyPress(Button buttons[], int count, int xx, int yy);
int BackFromApplet(xSemaphoreHandle sem, xTaskHandle displayTaskToDelete );

#endif /* BUTTON_H_ */
