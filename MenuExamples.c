/*
 * MenuExamples.c
 *
 *  Created on: Oct 4, 2016
 *      Author: Brad Goold
 *
 */

#include "leds.h"
#include <stdio.h>
#include "console.h"
#include "brew.h"
#include "lcd.h"
#include "I2C-IO.h"

void item_1_callback(unsigned char button_down)
{
	if (button_down)
		vLEDToggle(D4_PORT, D4_PIN);
}

void item_2_callback(unsigned char button_down)
{
	vI2C_RecyclePower();

}

void example_applet(int init)
{
	if (init)
	{
		lcd_printf(20, 1, 20, "Example Applet");
		lcd_printf(34, 13, 4, "Back");

	}
}

int example_applet_touch_handler(int xx, int yy)
{
	lcd_fill(0, 0, 150, 40, 0xFF);
	lcd_printf(1,1, 20, "Touch %d,%d", xx, yy);

	// return non-zero to exit applet
	return xx > 200 && yy > 200;
}
