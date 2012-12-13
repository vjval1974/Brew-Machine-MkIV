/*
 * valves.c
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */


#include "valves.h"
#include <stdint.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"

void vValvesAppletDisplay( void *pvParameters);
void vValvesApplet(int init);
xTaskHandle xValvesTaskHandle = NULL, xValvesAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;



void vValvesInit(void){


  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_VALVE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( HLT_VALVE_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(HLT_VALVE_PORT, HLT_VALVE_PIN); //pull low

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  MASH_VALVE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( MASH_VALVE_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(MASH_VALVE_PORT, MASH_VALVE_PIN); //pull low

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  INLET_VALVE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( INLET_VALVE_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(INLET_VALVE_PORT, INLET_VALVE_PIN); //pull low

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  BOIL_VALVE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( BOIL_VALVE_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(BOIL_VALVE_PORT, BOIL_VALVE_PIN); //pull low

  vSemaphoreCreateBinary(xAppletRunningSemaphore);


}

void vValveActuate( unsigned char valve, unsigned char state )
{
  uint8_t current = 0;

  switch (valve)
  {
  case HLT_VALVE:
    {
      if (state == TOGGLE)
         {
          current = GPIO_ReadInputDataBit(HLT_VALVE_PORT, HLT_VALVE_PIN);
               GPIO_WriteBit(HLT_VALVE_PORT, HLT_VALVE_PIN, current ^ 1);

         }
      else GPIO_WriteBit(HLT_VALVE_PORT, HLT_VALVE_PIN, state);
      break;
    }
  case MASH_VALVE:
    {
      if (state == TOGGLE)
         {
          current = GPIO_ReadInputDataBit(MASH_VALVE_PORT, MASH_VALVE_PIN);
               GPIO_WriteBit(MASH_VALVE_PORT, MASH_VALVE_PIN, current ^ 1);

         }
      else GPIO_WriteBit(MASH_VALVE_PORT, MASH_VALVE_PIN, state);
      break;
    }
  case BOIL_VALVE:
    {
      if (state == TOGGLE)
         {
          current = GPIO_ReadInputDataBit(BOIL_VALVE_PORT, BOIL_VALVE_PIN);
               GPIO_WriteBit(BOIL_VALVE_PORT, BOIL_VALVE_PIN, current ^ 1);
         }
      else GPIO_WriteBit(BOIL_VALVE_PORT, BOIL_VALVE_PIN, state);

      break;
    }
  case INLET_VALVE:
    {
      if (state == TOGGLE)
         {
          current = GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN);
               GPIO_WriteBit(INLET_VALVE_PORT, INLET_VALVE_PIN, current ^ 1);
         }
      else GPIO_WriteBit(INLET_VALVE_PORT, INLET_VALVE_PIN, state);
      break;
    }
  default:
    {
      break;
    }
  }

}



#define TOGGLE_HLT_VALVE_X1 0
#define TOGGLE_HLT_VALVE_Y1 30
#define TOGGLE_HLT_VALVE_X2 90
#define TOGGLE_HLT_VALVE_Y2 100
#define TOGGLE_HLT_VALVE_W (TOGGLE_HLT_VALVE_X2-TOGGLE_HLT_VALVE_X1)
#define TOGGLE_HLT_VALVE_H (TOGGLE_HLT_VALVE_Y2-TOGGLE_HLT_VALVE_Y1)

#define TOGGLE_MASH_VALVE_X1 95
#define TOGGLE_MASH_VALVE_Y1 30
#define TOGGLE_MASH_VALVE_X2 180
#define TOGGLE_MASH_VALVE_Y2 100
#define TOGGLE_MASH_VALVE_W (TOGGLE_MASH_VALVE_X2-TOGGLE_MASH_VALVE_X1)
#define TOGGLE_MASH_VALVE_H (TOGGLE_MASH_VALVE_Y2-TOGGLE_MASH_VALVE_Y1)

#define TOGGLE_BOIL_VALVE_X1 185
#define TOGGLE_BOIL_VALVE_Y1 30
#define TOGGLE_BOIL_VALVE_X2 275
#define TOGGLE_BOIL_VALVE_Y2 100
#define TOGGLE_BOIL_VALVE_W (TOGGLE_BOIL_VALVE_X2-TOGGLE_BOIL_VALVE_X1)
#define TOGGLE_BOIL_VALVE_H (TOGGLE_BOIL_VALVE_Y2-TOGGLE_BOIL_VALVE_Y1)

#define TOGGLE_INLET_VALVE_X1 0
#define TOGGLE_INLET_VALVE_Y1 105
#define TOGGLE_INLET_VALVE_X2 90
#define TOGGLE_INLET_VALVE_Y2 175
#define TOGGLE_INLET_VALVE_W (TOGGLE_INLET_VALVE_X2-TOGGLE_INLET_VALVE_X1)
#define TOGGLE_INLET_VALVE_H (TOGGLE_INLET_VALVE_Y2-TOGGLE_INLET_VALVE_Y1)


#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vValvesApplet(int init){
  if (init)
        {
                lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Cyan);
                lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Green);
                lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
                lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
                lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Cyan);
                lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Green);
                lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
                lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL VALVE APPLET");
                lcd_printf(0,4,13, "HLT_VALVE");
                lcd_printf(12,4,13, "MASH VALVE");
                lcd_printf(24,4,13, "BOIL VALVE");
                lcd_printf(0,8,13, "INLET VALVE");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vValvesAppletDisplay,
                    ( signed portCHAR * ) "Mill_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xValvesAppletDisplayHandle );
        }

}


void vValvesAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        for(;;)
        {

            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned


            tog = tog ^ 1;
            xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
            vTaskDelay(500);


        }
}

int iValvesKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > TOGGLE_HLT_VALVE_X1+1 && xx < TOGGLE_HLT_VALVE_X2-1 && yy > TOGGLE_HLT_VALVE_Y1+1 && yy < TOGGLE_HLT_VALVE_Y2-1)
    {
      vValveActuate(HLT_VALVE, TOGGLE);

    }
  else if (xx > TOGGLE_MASH_VALVE_X1+1 && xx < TOGGLE_MASH_VALVE_X2-1 && yy > TOGGLE_MASH_VALVE_Y1+1 && yy < TOGGLE_MASH_VALVE_Y2-1)
    {
      vValveActuate(MASH_VALVE, TOGGLE);

    }
  else if (xx > TOGGLE_BOIL_VALVE_X1+1 && xx < TOGGLE_BOIL_VALVE_X2-1 && yy > TOGGLE_BOIL_VALVE_Y1+1 && yy < TOGGLE_BOIL_VALVE_Y2-1)
     {
      vValveActuate(BOIL_VALVE, TOGGLE);
     }
  else if (xx > TOGGLE_INLET_VALVE_X1+1 && xx < TOGGLE_INLET_VALVE_X2-1 && yy > TOGGLE_INLET_VALVE_Y1+1 && yy < TOGGLE_INLET_VALVE_Y2-1)
     {
      vValveActuate(INLET_VALVE, TOGGLE);
     }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xValvesAppletDisplayHandle != NULL)
        {
          vTaskDelete(xValvesAppletDisplayHandle);
          vTaskDelay(100);
          xValvesAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

