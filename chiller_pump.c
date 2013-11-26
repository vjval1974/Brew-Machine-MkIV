/*
 * chiller_pump.c
 *
 *  Created on: Aug 5, 2013
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
#include "I2C-IO.h"
#include "chiller_pump.h"
#include "console.h"

void vChillerPumpAppletDisplay( void *pvParameters);
void vChillerPumpApplet(int init);
xTaskHandle xCHILLERPumpTaskHandle = NULL, xCHILLERPumpAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xChillerAppletRunningSemaphore;




volatile int uChillerPumpState = STOPPED;

void vChillerPumpInit(void){

  GPIO_InitTypeDef GPIO_InitStructure;
   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
   GPIO_InitStructure.GPIO_Pin =  CHILLER_PUMP_PIN;
   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
   GPIO_Init( CHILLER_PUMP_PORT, &GPIO_InitStructure );
   GPIO_ResetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN); //pull low
   vSemaphoreCreateBinary(xChillerAppletRunningSemaphore);
//  vPCF_ResetBits(CHILLER_PUMP_PIN, CHILLER_PUMP_PORT); //pull low
  vSemaphoreCreateBinary(xChillerAppletRunningSemaphore);


}

void vChillerPump( uint8_t state )
{
  if (state == PUMPING)
    GPIO_SetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN);
  else
    GPIO_ResetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN); //pull low

   uChillerPumpState = state;

//  if (state)
//    vPCF_SetBits(CHILLER_PUMP_PIN, CHILLER_PUMP_PORT);
//  else
//    vPCF_ResetBits(CHILLER_PUMP_PIN, CHILLER_PUMP_PORT);
//  uChillerPumpState = state;

}



#define START_CHILLER_PUMP_X1 155
#define START_CHILLER_PUMP_Y1 30
#define START_CHILLER_PUMP_X2 300
#define START_CHILLER_PUMP_Y2 100
#define START_CHILLER_PUMP_W (START_CHILLER_PUMP_X2-START_CHILLER_PUMP_X1)
#define START_CHILLER_PUMP_H (START_CHILLER_PUMP_Y2-START_CHILLER_PUMP_Y1)

#define STOP_CHILLER_PUMP_X1 155
#define STOP_CHILLER_PUMP_Y1 105
#define STOP_CHILLER_PUMP_X2 300
#define STOP_CHILLER_PUMP_Y2 175
#define STOP_CHILLER_PUMP_W (STOP_CHILLER_PUMP_X2-STOP_CHILLER_PUMP_X1)
#define STOP_CHILLER_PUMP_H (STOP_CHILLER_PUMP_Y2-STOP_CHILLER_PUMP_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vChillerPumpApplet(int init){
  if (init)
        {
                lcd_DrawRect(STOP_CHILLER_PUMP_X1, STOP_CHILLER_PUMP_Y1, STOP_CHILLER_PUMP_X2, STOP_CHILLER_PUMP_Y2, Cyan);
                lcd_fill(STOP_CHILLER_PUMP_X1+1, STOP_CHILLER_PUMP_Y1+1, STOP_CHILLER_PUMP_W, STOP_CHILLER_PUMP_H, Red);
                lcd_DrawRect(START_CHILLER_PUMP_X1, START_CHILLER_PUMP_Y1, START_CHILLER_PUMP_X2, START_CHILLER_PUMP_Y2, Cyan);
                lcd_fill(START_CHILLER_PUMP_X1+1, START_CHILLER_PUMP_Y1+1, START_CHILLER_PUMP_W, START_CHILLER_PUMP_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL CHILLER_PUMP APPLET");
                lcd_printf(22,4,13, "START CHILLER_PUMP");
                lcd_printf(22,8,12, "STOP CHILLER_PUMP");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vChillerPumpAppletDisplay,
                    ( signed portCHAR * ) "chiller_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xCHILLERPumpAppletDisplayHandle );
        }
  else
    vConsolePrint("Leaving Chiller Pump Applet\r\n");

}


void vChillerPumpAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        for(;;)
        {

            xSemaphoreTake(xChillerAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned
                switch (uChillerPumpState)
                {
                case PUMPING:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                                lcd_printf(1,13,15,"CHILLER_PUMP DRIVING");
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
                                lcd_printf(1,13,11,"CHILLER_PUMP STOPPED");
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
                xSemaphoreGive(xChillerAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);


        }
}

int iChillerPumpKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > STOP_CHILLER_PUMP_X1+1 && xx < STOP_CHILLER_PUMP_X2-1 && yy > STOP_CHILLER_PUMP_Y1+1 && yy < STOP_CHILLER_PUMP_Y2-1)
    {
      vChillerPump(STOPPED);
      uChillerPumpState = STOPPED;

    }
  else if (xx > START_CHILLER_PUMP_X1+1 && xx < START_CHILLER_PUMP_X2-1 && yy > START_CHILLER_PUMP_Y1+1 && yy < START_CHILLER_PUMP_Y2-1)
    {
      vChillerPump(PUMPING);
      uChillerPumpState = PUMPING;
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xChillerAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xCHILLERPumpAppletDisplayHandle != NULL)
        {
          vTaskDelete(xCHILLERPumpAppletDisplayHandle);
          vTaskDelay(100);
          xCHILLERPumpAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xChillerAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}



