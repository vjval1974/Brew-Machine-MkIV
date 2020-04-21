/*
 * button.c
 *
 *  Created on: Sep 9, 2018
 *      Author: ubuntu
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "button.h"
#include "lcd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

uint16_t uiGetXPosForButtonText(uint16_t x1, uint16_t x2, const char * text)
{
	// 1. Get Centre
	uint16_t centre = (uint16_t)( (x2 + x1) / 2 );

	// 2. Get string length * char width (8) and halve it;
	uint16_t halfStringLength = strlen(text) * 8 / 2;

	// 3.
	return centre - halfStringLength;
}

uint16_t uiGetYPosForButtonText(uint16_t y1, uint16_t y2)
{
	return (uint16_t)( (y2 + y1) / 2 ) - 8; // halfway between the y's and take half of the char height off
}



void vDrawButton(Button button)
{
	lcd_DrawRect(button.x1, button.y1, button.x2, button.y2, button.outlineColor);
	lcd_fill(button.x1 + 1, button.y1 + 1, button.x2-button.x1, button.y2-button.y1, button.fillColor);
	uint16_t lenText = strlen(button.text);
	uint16_t lenStateText = strlen(button.stateText);
	uint16_t textXpos = uiGetXPosForButtonText(button.x1, button.x2, button.text);
	uint16_t stateTextXpos = uiGetXPosForButtonText(button.x1, button.x2, button.stateText);
	uint16_t textYpos = uiGetYPosForButtonText(button.y1, button.y2);

	//lcd_text_xy(button.x1 + 2, button.y1,    button.text,      button.outlineColor, button.fillColor);
	lcd_text_xy(textXpos, textYpos-8,    button.text,      button.outlineColor, button.fillColor);

	//lcd_text_xy(button.x1 + 2, button.y1+32, button.stateText, Yellow,              button.fillColor);
	lcd_text_xy(stateTextXpos, textYpos+8,    button.stateText,    Yellow, 			button.fillColor);
	//lcd_text_xy(button.x1 + 2, button.y1+32, button.stateText, Yellow,              button.fillColor);

}



void vDrawButtons(Button buttons[], int count)
{
	int i = 0;
	for(i = 0; i < count; i++)
	{
		vDrawButton(buttons[i]);
	}
}

void vDrawButtonExp(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const char * text, uint16_t outlineColor, uint16_t fillColor )
{
	const double xPosTextQuotient = 7, yPosTextQuotient = 32.5;
	lcd_DrawRect(x1, y1, x2, y2, outlineColor);
	lcd_fill(x1 + 1, y1 + 1, x2-x1, y2-y1, fillColor);
	lcd_printf((int)floor(x1/xPosTextQuotient), (int)floor((y2 + y1)/yPosTextQuotient), 18, text);
}

Button createButton(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const char * text, uint16_t outlineColor, uint16_t fillColor, int (* action)() )
{
	Button button;
	button.x1 = x1;
	button.y1 = y1;
	button.x2 = x2;
	button.y2 = y2;
	button.text = text;
	button.outlineColor = outlineColor;
	button.fillColor = fillColor;
	button.action = action;
	//Buttons[buttonCounter] = button;
	//buttonCounter++;
	return button;
}


int ButtonHasBeenPressed(Button button, int xx, int yy)
{

	if (xx > button.x1 + 1 && xx < button.x2 - 1  && yy > button.y1 + 1 && yy < button.y2 - 1)
		return 1;
	else return -1;
}

int ActionKeyPress(Button buttons[], int count, int xx, int yy)
{
	int i = 0;

	for(i = 0; i < count; i++)
	{
		if (ButtonHasBeenPressed(buttons[i], xx, yy) == 1)
		{
			return buttons[i].action();
		}

	}
	return 0;
}


int BackFromApplet(xSemaphoreHandle sem, xTaskHandle displayTaskToDelete )
{
	//try to take the semaphore from the display applet. wait here if we cant take it.
	xSemaphoreTake(sem, portMAX_DELAY);
	//delete the display applet task if its been created.
	if (displayTaskToDelete != NULL )
	{
		vTaskDelete(displayTaskToDelete);
		vTaskDelay(100);
		displayTaskToDelete = NULL;
	}

	//return the semaphore for taking by another task.
	xSemaphoreGive(sem);
	return 1;

}


