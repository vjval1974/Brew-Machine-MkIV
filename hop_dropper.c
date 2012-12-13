/*
 * hop_dropper.c
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */
#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "adc.h"
#include "ds1820.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include "task.h"
#include "semphr.h"
#include "hop_dropper.h"

#define ON 1
#define OFF 0

volatile uint8_t hop_dropper_state = OFF;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;

xTaskHandle xHopsNextTaskHandle = NULL, xHopDropperAppletDisplayHandle = NULL;


void vHopsInit(void)
{

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HOP_DROPPER_DRIVE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( HOP_DROPPER_DRIVE_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(HOP_DROPPER_DRIVE_PORT, HOP_DROPPER_DRIVE_PIN);

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HOP_DROPPER_LIMIT_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;// Output - Push Pull
  GPIO_Init( HOP_DROPPER_LIMIT_PORT, &GPIO_InitStructure );


}

void vHopsDrive(unsigned char state){

      GPIO_WriteBit(HOP_DROPPER_DRIVE_PORT, HOP_DROPPER_DRIVE_PIN, state);

}




void vTaskHopsNext(  void * pvParameters)
{
  char gap = 0, in = 0;
  vHopsDrive(ON);
  vTaskDelay(5); // wait for motor to start driving
  printf("looking for gap\r\n");
  while (gap == 0)
    {
      in = GPIO_ReadInputDataBit(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN); //sit in loop waiting for the gap
      //debounce
      if (in)
        vTaskDelay(5);
      // check again
      in = GPIO_ReadInputDataBit(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN); //sit in loop waiting for the gap
      if (in)
        gap = 1;
      else gap = 0;
    }
 printf("found gap\r\n");
  // we get here if we have a gap.
 printf("looking for next limit\r\n");
  for(;;)
    {
      //wait until we get another limit

      in = GPIO_ReadInputDataBit(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN); //sit in loop waiting for the gap
      //debounce
      if (!in)
        vTaskDelay(5);
      // check again
      in = GPIO_ReadInputDataBit(HOP_DROPPER_LIMIT_PORT, HOP_DROPPER_LIMIT_PIN); //sit in loop waiting for the gap
      if (!in) //we have hit the next stop
        {
          printf("found next limit!\r\n");
          vHopsDrive(OFF);
          xHopsNextTaskHandle = NULL;
          vTaskDelete(NULL);
        }
vTaskDelay(10);
    }


}





void vHopDropperAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop

        for(;;)
        {
            hop_dropper_state = GPIO_ReadInputDataBit(HOP_DROPPER_DRIVE_PORT, HOP_DROPPER_DRIVE_PIN);
            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                    //return to the menu system until its returned
                switch (hop_dropper_state)
                {
                case ON:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                                lcd_printf(1,13,15,"DRIVING");
                        }
                        else{
                                lcd_fill(1,210, 180,17, Black);
                        }
                        break;
                }
                case OFF:
                {
                        if(tog)
                        {
                                lcd_fill(1,210, 180,29, Black);
                                lcd_printf(1,13,11,"OFF");
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


#define HOPS_NEXT_X1 0
#define HOPS_NEXT_Y1 30
#define HOPS_NEXT_X2 100
#define HOPS_NEXT_Y2 100
#define HOPS_NEXT_W (HOPS_NEXT_X2-HOPS_NEXT_X1)
#define HOPS_NEXT_H (HOPS_NEXT_Y2-HOPS_NEXT_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)
void vHopDropperApplet(int init){
  if (init)
        {
                lcd_DrawRect(HOPS_NEXT_X1, HOPS_NEXT_Y1, HOPS_NEXT_X2, HOPS_NEXT_Y2, Red);
                lcd_fill(HOPS_NEXT_X1+1, HOPS_NEXT_Y1+1, HOPS_NEXT_W, HOPS_NEXT_H, Blue);

                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL Hops APPLET");
                lcd_printf(3,4,11,  "Hops next");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vHopDropperAppletDisplay,
                    ( signed portCHAR * ) "hlt_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY+1 ,
                    &xHopDropperAppletDisplayHandle );
        }

}


int iHopDropperKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  if (xx > HOPS_NEXT_X1+1 && xx < HOPS_NEXT_X2-1 && yy > HOPS_NEXT_Y1+1 && yy < HOPS_NEXT_Y2-1)
    {
      printf("Checking if task already running...\r\n");
      if (xHopsNextTaskHandle == NULL)
        {
          printf("No previous HOPS task\r\n");
          printf("Creating Task\r\n");
          xTaskCreate( vTaskHopsNext,
              ( signed portCHAR * ) "Hops_next",
              configMINIMAL_STACK_SIZE +800,
              NULL,
              tskIDLE_PRIORITY+1,
              &xHopsNextTaskHandle );
        }
      else
        {
          printf("Hops task already running\r\n");
        }
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xHopDropperAppletDisplayHandle != NULL)
        {
          vTaskDelete(xHopDropperAppletDisplayHandle);
          vTaskDelay(100);
          xHopDropperAppletDisplayHandle = NULL;
        }
      //delete the hops task
      if (xHopsNextTaskHandle != NULL)
        {
          vHopsDrive(OFF);
          vTaskDelete(xHopsNextTaskHandle);
          vTaskDelay(100);
          xHopsNextTaskHandle = NULL;
        }
      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}
