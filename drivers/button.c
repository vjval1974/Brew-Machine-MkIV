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



void vDrawButton(Button button)
{
	const double xPosTextQuotient = 7.5, yPosTextQuotient = 32.5;
	lcd_DrawRect(button.x1, button.y1, button.x2, button.y2, button.outlineColor);
	lcd_fill(button.x1 + 1, button.y1 + 1, button.x2-button.x1, button.y2-button.y1, button.fillColor);
	uint16_t lenText = strlen(button.text);
	uint16_t lenStateText = strlen(button.stateText);
	lcd_printf((int)floor(button.x1/xPosTextQuotient), (int)floor((button.y2 + button.y1)/yPosTextQuotient), lenText, button.text);
	lcd_printf((int)floor(button.x1/xPosTextQuotient), (int)floor((button.y2 + button.y1)/yPosTextQuotient)+1, lenStateText, button.stateText);
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

