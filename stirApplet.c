/*
 * stirApplet.c
 *
 *  Created on: 14/12/2014
 *      Author: brad
 */




#include <stdint.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "queue.h"
#include "stirApplet.h"
#include "stir.h"

xTaskHandle xStirAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xStirAppletRunningSemaphore;

void vStirAppletDisplay( void *pvParameters);
void vStirApplet(int init);

#define START_STIR_X1 155
#define START_STIR_Y1 30
#define START_STIR_X2 300
#define START_STIR_Y2 100
#define START_STIR_W (START_STIR_X2-START_STIR_X1)
#define START_STIR_H (START_STIR_Y2-START_STIR_Y1)

#define STOP_STIR_X1 155
#define STOP_STIR_Y1 105
#define STOP_STIR_X2 300
#define STOP_STIR_Y2 175
#define STOP_STIR_W (STOP_STIR_X2-STOP_STIR_X1)
#define STOP_STIR_H (STOP_STIR_Y2-STOP_STIR_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vStirApplet(int init){
  if (init)
        {
                lcd_DrawRect(STOP_STIR_X1, STOP_STIR_Y1, STOP_STIR_X2, STOP_STIR_Y2, Cyan);
                lcd_fill(STOP_STIR_X1+1, STOP_STIR_Y1+1, STOP_STIR_W, STOP_STIR_H, Red);
                lcd_DrawRect(START_STIR_X1, START_STIR_Y1, START_STIR_X2, START_STIR_Y2, Cyan);
                lcd_fill(START_STIR_X1+1, START_STIR_Y1+1, START_STIR_W, START_STIR_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL STIR APPLET");
                lcd_printf(22,4,13, "START STIR");
                lcd_printf(22,8,12, "STOP STIR");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vStirAppletDisplay,
                    ( signed portCHAR * ) "chiller_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xStirAppletDisplayHandle );
        }

}


void vStirAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        StirState xStirState;
        for(;;)
        {

            xSemaphoreTake(xStirAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned
            xStirState = xGetStirState();
                switch (xStirState)
                {
                case STIR_DRIVING:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                                lcd_printf(1,13,15,"STIR DRIVING");
                        }
                        else{
                                lcd_fill(1,210, 180,17, Black);
                        }
                        break;
                }
                case STIR_STOPPED:
                {
                        if(tog)
                        {
                                lcd_fill(1,210, 180,29, Black);
                                lcd_printf(1,13,11,"STIR STOPPED");
                        }
                        else
                          {
                                lcd_fill(1,210, 180,17, Black);
                          }

                        break;
                }
                default:
                {
                        break;
                }
                }

                tog = tog ^ 1;
                xSemaphoreGive(xStirAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);


        }
}


int iStirKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > STOP_STIR_X1+1 && xx < STOP_STIR_X2-1 && yy > STOP_STIR_Y1+1 && yy < STOP_STIR_Y2-1)
    {
      vStir(STIR_STOPPED);

    }
  else if (xx > START_STIR_X1+1 && xx < START_STIR_X2-1 && yy > START_STIR_Y1+1 && yy < START_STIR_Y2-1)
    {
      vStir(STIR_DRIVING);
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xStirAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xStirAppletDisplayHandle != NULL)
        {
          vTaskDelete(xStirAppletDisplayHandle);
          vTaskDelay(100);
          xStirAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xStirAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}
