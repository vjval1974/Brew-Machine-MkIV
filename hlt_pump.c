/*
 * hlt_pump.c
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
#include "hlt_pump.h"

void vHLTPumpAppletDisplay( void *pvParameters);
void vHLTPumpApplet(int init);
xTaskHandle xHLTPumpTaskHandle = NULL, xHLTPumpAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;


#define PUMPING 1
#define STOPPED 0

volatile uint8_t uHLTPumpState = STOPPED;

void vHLTPumpInit(void){

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_PUMP_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( HLT_PUMP_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(HLT_PUMP_PORT, HLT_PUMP_PIN); //pull low
  vSemaphoreCreateBinary(xAppletRunningSemaphore);


}

static void vHLTPump( uint8_t state )
{
  GPIO_WriteBit(HLT_PUMP_PORT, HLT_PUMP_PIN, state);
  uHLTPumpState = state;

}



#define START_HLT_PUMP_X1 155
#define START_HLT_PUMP_Y1 30
#define START_HLT_PUMP_X2 300
#define START_HLT_PUMP_Y2 100
#define START_HLT_PUMP_W (START_HLT_PUMP_X2-START_HLT_PUMP_X1)
#define START_HLT_PUMP_H (START_HLT_PUMP_Y2-START_HLT_PUMP_Y1)

#define STOP_HLT_PUMP_X1 155
#define STOP_HLT_PUMP_Y1 105
#define STOP_HLT_PUMP_X2 300
#define STOP_HLT_PUMP_Y2 175
#define STOP_HLT_PUMP_W (STOP_HLT_PUMP_X2-STOP_HLT_PUMP_X1)
#define STOP_HLT_PUMP_H (STOP_HLT_PUMP_Y2-STOP_HLT_PUMP_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vHLTPumpApplet(int init){
  if (init)
        {
                lcd_DrawRect(STOP_HLT_PUMP_X1, STOP_HLT_PUMP_Y1, STOP_HLT_PUMP_X2, STOP_HLT_PUMP_Y2, Cyan);
                lcd_fill(STOP_HLT_PUMP_X1+1, STOP_HLT_PUMP_Y1+1, STOP_HLT_PUMP_W, STOP_HLT_PUMP_H, Red);
                lcd_DrawRect(START_HLT_PUMP_X1, START_HLT_PUMP_Y1, START_HLT_PUMP_X2, START_HLT_PUMP_Y2, Cyan);
                lcd_fill(START_HLT_PUMP_X1+1, START_HLT_PUMP_Y1+1, START_HLT_PUMP_W, START_HLT_PUMP_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL HLT_PUMP APPLET");
                lcd_printf(22,4,13, "START HLT_PUMP");
                lcd_printf(22,8,12, "STOP HLT_PUMP");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vHLTPumpAppletDisplay,
                    ( signed portCHAR * ) "Mill_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xHLTPumpAppletDisplayHandle );
        }

}


void vHLTPumpAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        for(;;)
        {

            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned
                switch (uHLTPumpState)
                {
                case PUMPING:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                                lcd_printf(1,13,15,"HLT_PUMP DRIVING");
                        }
                        else{
                                lcd_fill(1,210, 180,17, Black);
                        }
                        break;
                }
                case STOPPED:
                {
                        if(tog)
                        {
                                lcd_fill(1,210, 180,29, Black);
                                lcd_printf(1,13,11,"HLT_PUMP STOPPED");
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

int iHLTPumpKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > STOP_HLT_PUMP_X1+1 && xx < STOP_HLT_PUMP_X2-1 && yy > STOP_HLT_PUMP_Y1+1 && yy < STOP_HLT_PUMP_Y2-1)
    {
      vHLTPump(STOPPED);
      uHLTPumpState = STOPPED;

    }
  else if (xx > START_HLT_PUMP_X1+1 && xx < START_HLT_PUMP_X2-1 && yy > START_HLT_PUMP_Y1+1 && yy < START_HLT_PUMP_Y2-1)
    {
      vHLTPump(PUMPING);
      uHLTPumpState = PUMPING;
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xHLTPumpAppletDisplayHandle != NULL)
        {
          vTaskDelete(xHLTPumpAppletDisplayHandle);
          vTaskDelay(100);
          xHLTPumpAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

