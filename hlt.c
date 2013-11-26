/*
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_it.h"

#include "adc.h"
#include "ds1820.h"
#include "hlt.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "task.h"
#include "semphr.h"

volatile char hlt_state = OFF;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xHLTAppletRunningSemaphore;

xTaskHandle xHeatHLTTaskHandle = NULL, xHLTAppletDisplayHandle = NULL;

volatile float diag_setpoint = 74.5; // when calling the heat_hlt task, we use this value instead of the passed parameter.

void
vHLTAppletDisplay(void *pvParameters);

void
hlt_init()
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_SSR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
  GPIO_Init(HLT_SSR_PORT, &GPIO_InitStructure);
  GPIO_ResetBits(HLT_SSR_PORT, HLT_SSR_PIN );

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_LEVEL_CHECK_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Input Pulled-up
  GPIO_Init(HLT_LEVEL_CHECK_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_HIGH_LEVEL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Input Pulled-up
  GPIO_Init(HLT_HIGH_LEVEL_PORT, &GPIO_InitStructure);
  adc_init(); //initialise the ADC1 so we can use a channel from it for the hlt level

  vSemaphoreCreateBinary(xHLTAppletRunningSemaphore);

}

float
fGetHLTLevel(void)
{
  short raw_adc_value;

  float litres = 0;
  float gradient = (HLT_ANALOGUE_MAX - HLT_ANALOGUE_MIN)
      / (HLT_MAX_LITRES - HLT_MIN_LITRES);

  raw_adc_value = read_adc(HLT_LEVEL_ADC_CHAN);

  litres = ((float) raw_adc_value - (float) HLT_ANALOGUE_MIN
      + ((float) (HLT_MIN_LITRES) * gradient)) / gradient;

  //litres = (float)((float)raw_adc_value/(float)HLT_ANALOGUE_MAX) * (float)HLT_MAX_LITRES;
  litres = 16;

  if ((GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN ) ^ 1)
      || (litres < 4.0))
    {
      return litres;
    }
  return 16;
//////////////////////////////OVERRIDDEN/////////////////////////////////
}

// Problem with HLT task.. if setpoint is changed during heating, its not recognised.. I think

void
vTaskHeatHLT(void * pvParameters)
{
  float * setpoint = (float *) pvParameters;
  float actual = 0.0;
  float hlt_level = 0.0;
  char hlt_ok = 0;
  //choose which value we use for the setpoint.
  if (setpoint == NULL )
    setpoint = (float *) &diag_setpoint;

  for (;;)
    {
      hlt_level = fGetHLTLevel();
      // ensure we have water above the elements

      if (hlt_level > 4.0)
        {
          //check the temperature
          actual = ds1820_get_temp(HLT);

          // output depending on temp
          if (actual < *setpoint)
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 1);
              hlt_state = HEATING;
            }
          else
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              hlt_state = OFF;
            }
        }
      else //DONT HEAT... WE WILL BURN OUT THE ELEMENT
        {
          GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
          hlt_state = OFF;
        }
      vTaskDelay(100); // 100us delay is fine for heating
//      taskYIELD();
    }

}

#define SETPOINT_UP_X1 0
#define SETPOINT_UP_Y1 30
#define SETPOINT_UP_X2 100
#define SETPOINT_UP_Y2 100
#define SETPOINT_UP_W (SETPOINT_UP_X2-SETPOINT_UP_X1)
#define SETPOINT_UP_H (SETPOINT_UP_Y2-SETPOINT_UP_Y1)

#define SETPOINT_DN_X1 0
#define SETPOINT_DN_Y1 105
#define SETPOINT_DN_X2 100
#define SETPOINT_DN_Y2 175
#define SETPOINT_DN_W (SETPOINT_DN_X2-SETPOINT_DN_X1)
#define SETPOINT_DN_H (SETPOINT_DN_Y2-SETPOINT_DN_Y1)

#define START_HEATING_X1 155
#define START_HEATING_Y1 30
#define START_HEATING_X2 300
#define START_HEATING_Y2 100
#define START_HEATING_W (START_HEATING_X2-START_HEATING_X1)
#define START_HEATING_H (START_HEATING_Y2-START_HEATING_Y1)

#define STOP_HEATING_X1 155
#define STOP_HEATING_Y1 105
#define STOP_HEATING_X2 300
#define STOP_HEATING_Y2 175
#define STOP_HEATING_W (STOP_HEATING_X2-STOP_HEATING_X1)
#define STOP_HEATING_H (STOP_HEATING_Y2-STOP_HEATING_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)
void
vHLTApplet(int init)
{
  if (init)
    {
      lcd_DrawRect(SETPOINT_UP_X1, SETPOINT_UP_Y1, SETPOINT_UP_X2,
          SETPOINT_UP_Y2, Red);
      lcd_fill(SETPOINT_UP_X1 + 1, SETPOINT_UP_Y1 + 1, SETPOINT_UP_W,
          SETPOINT_UP_H, Blue);
      lcd_DrawRect(SETPOINT_DN_X1, SETPOINT_DN_Y1, SETPOINT_DN_X2,
          SETPOINT_DN_Y2, Red);
      lcd_fill(SETPOINT_DN_X1 + 1, SETPOINT_DN_Y1 + 1, SETPOINT_DN_W,
          SETPOINT_DN_H, Blue);
      lcd_DrawRect(STOP_HEATING_X1, STOP_HEATING_Y1, STOP_HEATING_X2,
          STOP_HEATING_Y2, Cyan);
      lcd_fill(STOP_HEATING_X1 + 1, STOP_HEATING_Y1 + 1, STOP_HEATING_W,
          STOP_HEATING_H, Red);
      lcd_DrawRect(START_HEATING_X1, START_HEATING_Y1, START_HEATING_X2,
          START_HEATING_Y2, Cyan);
      lcd_fill(START_HEATING_X1 + 1, START_HEATING_Y1 + 1, START_HEATING_W,
          START_HEATING_H, Green);
      lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
      lcd_fill(BK_X1 + 1, BK_Y1 + 1, BK_W, BK_H, Magenta);
      lcd_printf(10, 1, 18, "MANUAL HLT APPLET");
      lcd_printf(3, 4, 11, "SP UP");
      lcd_printf(1, 8, 13, "SP DOWN");
      lcd_printf(22, 4, 13, "START HEATING");
      lcd_printf(22, 8, 12, "STOP HEATING");
      lcd_printf(30, 13, 4, "Back");
      //vTaskDelay(2000);
      //adc_init();
      //adc_init();
      //create a dynamic display task
      xTaskCreate( vHLTAppletDisplay, ( signed portCHAR * ) "hlt_disp",
          configMINIMAL_STACK_SIZE + 200, NULL, tskIDLE_PRIORITY,
          &xHLTAppletDisplayHandle);

    }

}

void
vHLTAppletDisplay(void *pvParameters)
{
  static char tog = 0; //toggles each loop
  float hlt_level;
  float diag_setpoint1; // = diag_setpoint;
  float current_temp;
  uint8_t ds10;
  char hlt_ok = 0;
  for (;;)
    {

      xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);
      //take the semaphore so that the key handler wont
      //return to the menu system until its returned
      current_temp = ds1820_get_temp(HLT);
      hlt_level = fGetHLTLevel();
      diag_setpoint1 = diag_setpoint;
      lcd_fill(1, 178, 170, 40, Black);

      //Tell user whether there is enough water to heat
      if (hlt_level > 4.0)
        lcd_printf(1, 11, 20, "level OK");
      else
        lcd_printf(1, 11, 20, "level LOW");

      //display the level if its over 4 litres
      lcd_printf(1, 12, 30, "level =  %d.%dl", (unsigned int) floor(hlt_level),
          (unsigned int) (hlt_level - floor(hlt_level) * pow(10, 3)));
      //lcd_printf(1,12,30,"level = %d litres", (int)hlt_level);
      //display the state and user info (the state will flash on the screen)
      switch (hlt_state)
        {
      case HEATING:
        {
          if (tog)
            {
              lcd_fill(1, 220, 180, 29, Black);
              lcd_printf(1, 13, 15, "HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(current_temp),
                  (unsigned int) ((current_temp - floor(current_temp))
                      * pow(10, 3)));
              //lcd_printf(1,14,15,"Currently = %2.1fdegC", current_temp);
            }
          else
            {
              lcd_fill(1, 210, 180, 17, Black);
            }
          break;
        }
      case OFF:
        {
          if (tog)
            {
              lcd_fill(1, 210, 180, 29, Black);
              lcd_printf(1, 13, 11, "NOT HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(current_temp),
                  (unsigned int) ((current_temp - floor(current_temp))
                      * pow(10, 3)));
              //lcd_printf(1,14,15,"Currently = %2.1fdegC", current_temp);
            }
          else
            {
              lcd_fill(1, 210, 180, 17, Black);
            }

          break;
        }
      default:
        {
          break;
        }
        }

      tog = tog ^ 1;
      lcd_fill(102, 99, 35, 10, Black);
      //printf("%d, %d, %d\r\n", (uint8_t)diag_setpoint, (diag_setpoint), ((uint8_t)diag_setpoint*10)%5);
      lcd_printf(13, 6, 25, "%d.%d", (unsigned int) floor(diag_setpoint),
          (unsigned int) ((diag_setpoint - floor(diag_setpoint)) * pow(10, 3)));
      //lcd_printf(13,6,15,"%d", (int)diag_setpoint);

      xSemaphoreGive(xHLTAppletRunningSemaphore);
      //give back the semaphore as its safe to return now.
      vTaskDelay(500);

    }
}

void
vHLTAppletCallback(int in_out)
{
//printf(" Called in_out = %d\r\n", in_out);

}
int
HLTKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5, h = 5;
  static uint16_t last_window = 0;
  if (xx > SETPOINT_UP_X1 + 1 && xx < SETPOINT_UP_X2 - 1
      && yy > SETPOINT_UP_Y1 + 1 && yy < SETPOINT_UP_Y2 - 1)
    {
      diag_setpoint += 0.5;
      //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
    }
  else if (xx > SETPOINT_DN_X1 + 1 && xx < SETPOINT_DN_X2 - 1
      && yy > SETPOINT_DN_Y1 + 1 && yy < SETPOINT_DN_Y2 - 1)

    {
      diag_setpoint -= 0.5;
      //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
    }
  else if (xx > STOP_HEATING_X1 + 1 && xx < STOP_HEATING_X2 - 1
      && yy > STOP_HEATING_Y1 + 1 && yy < STOP_HEATING_Y2 - 1)
    {
      //printf("Deleting Heating Task\r\n");
      if (xHeatHLTTaskHandle != NULL )
        {
          vTaskDelete(xHeatHLTTaskHandle);
          xHeatHLTTaskHandle = NULL;

        }
      GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
      hlt_state = OFF;
    }
  else if (xx > START_HEATING_X1 + 1 && xx < START_HEATING_X2 - 1
      && yy > START_HEATING_Y1 + 1 && yy < START_HEATING_Y2 - 1)
    {
      //printf("Checking if task already running...\r\n");
      if (xHeatHLTTaskHandle == NULL )
        {
          // printf("No previous HLT Heating task\r\n");
          // printf("Creating Task\r\n");
          xTaskCreate( vTaskHeatHLT, ( signed portCHAR * ) "HLT_HEAT",
              configMINIMAL_STACK_SIZE +800, NULL, tskIDLE_PRIORITY+2,
              &xHeatHLTTaskHandle);
        }
      else
        {
          //printf("Heating task already running\r\n");
        }
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xHLTAppletDisplayHandle != NULL )
        {
          vTaskDelete(xHLTAppletDisplayHandle);
          vTaskDelay(100);
          xHLTAppletDisplayHandle = NULL;
        }
      //delete the heating task
      if (xHeatHLTTaskHandle != NULL )
        {
          // commented out for test so can make beer!
          //vTaskDelete(xHeatHLTTaskHandle);
          //vTaskDelay(100);
          //xHeatHLTTaskHandle = NULL;
        }
      //return the semaphore for taking by another task.
      xSemaphoreGive(xHLTAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

