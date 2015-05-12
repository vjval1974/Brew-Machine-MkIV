/*
 * mill.c
 *
 *  Created on: Dec 13, 2012
 *      Author: brad
 */


//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "mill.h"
#include "main.h"

void vMillAppletDisplay( void *pvParameters);
void vMillApplet(int init);
xTaskHandle xMillTaskHandle = NULL, xMillAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;



volatile MillState xMillState = MILL_STOPPED;

void vMillInit(void){

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  MILL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( MILL_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(MILL_PORT, MILL_PIN); //pull low
  xMillState = MILL_STOPPED;
  vSemaphoreCreateBinary(xAppletRunningSemaphore);


}

void vMill( MillState state )
{
  if (state == MILL_DRIVING)
    GPIO_WriteBit(MILL_PORT, MILL_PIN, 1);
  else
    GPIO_WriteBit(MILL_PORT, MILL_PIN, 0);
  xMillState = state;

}



#define START_MILL_X1 155
#define START_MILL_Y1 30
#define START_MILL_X2 300
#define START_MILL_Y2 100
#define START_MILL_W (START_MILL_X2-START_MILL_X1)
#define START_MILL_H (START_MILL_Y2-START_MILL_Y1)

#define STOP_MILL_X1 155
#define STOP_MILL_Y1 105
#define STOP_MILL_X2 300
#define STOP_MILL_Y2 175
#define STOP_MILL_W (STOP_MILL_X2-STOP_MILL_X1)
#define STOP_MILL_H (STOP_MILL_Y2-STOP_MILL_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vMillApplet(int init){
  if (init)
        {
                lcd_DrawRect(STOP_MILL_X1, STOP_MILL_Y1, STOP_MILL_X2, STOP_MILL_Y2, Cyan);
                lcd_fill(STOP_MILL_X1+1, STOP_MILL_Y1+1, STOP_MILL_W, STOP_MILL_H, Red);
                lcd_DrawRect(START_MILL_X1, START_MILL_Y1, START_MILL_X2, START_MILL_Y2, Cyan);
                lcd_fill(START_MILL_X1+1, START_MILL_Y1+1, START_MILL_W, START_MILL_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL MILL APPLET");
                lcd_printf(22,4,13, "START MILL");
                lcd_printf(22,8,12, "STOP MILL");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vMillAppletDisplay,
                    ( signed portCHAR * ) "Mill_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xMillAppletDisplayHandle );
        }

}


void vMillAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        for(;;)
        {

            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned
                switch (xMillState)
                {
                case MILL_DRIVING:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                                lcd_printf(1,13,15,"MILL DRIVING");
                        }
                        else{
                                lcd_fill(1,210, 180,17, Black);
                        }
                        break;
                }
                case MILL_STOPPED:
                {
                        if(tog)
                        {
                                lcd_fill(1,210, 180,29, Black);
                                lcd_printf(1,13,11,"MILL STOPPED");
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
                xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);


        }
}

int iMillKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > STOP_MILL_X1+1 && xx < STOP_MILL_X2-1 && yy > STOP_MILL_Y1+1 && yy < STOP_MILL_Y2-1)
    {
      vMill(MILL_STOPPED);


    }
  else if (xx > START_MILL_X1+1 && xx < START_MILL_X2-1 && yy > START_MILL_Y1+1 && yy < START_MILL_Y2-1)
    {
      vMill(MILL_DRIVING);

    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xMillAppletDisplayHandle != NULL)
        {
          vTaskDelete(xMillAppletDisplayHandle);
          vTaskDelay(100);
          xMillAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

