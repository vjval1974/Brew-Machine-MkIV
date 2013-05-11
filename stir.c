/*
 * stir.c
 *
 *  Created on: Dec 15, 2012
 *      Author: brad
 */
//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "stir.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"


#define CW 1
#define CCW -1
// was going to pass in a struct Step *step on the queue..
// when the queue receives the pointer, we can then maybe get step[0..n]
// if step[n].ucDirection = 0, and step[n].uSpeed = 0 and step[n].uTime = 0, then
// all steps are over and we maybe can send a message back saying we are finished processing
// that sequence
xQueueHandle xStirQueueHandle;

xSemaphoreHandle xAppletRunningSemaphore;

xTaskHandle xStirTaskHandle = NULL, xStirAppletDisplayHandle = NULL;
void vTaskStir(void * pvParameters);


struct Step
{
  unsigned char ucDirection;
  int uSpeed;
  int  uTime;
  //        char ucMessageID;
  //        char ucData[ 20 ];
};


struct Step xSteps[] = {
      {CW,100,1},
      {CCW,50,1}
};

struct Step MashStirSteps[] = {
      {CW,100,10},
      {CW,0,10},
      {CCW,50,5},
      {CCW,75,5},
      {CCW,100,5},
      {CCW,0,10},
      {CW,100,1},
      {CW,100,1},
      {CW,100,1},
      {CCW,50,1},
      {0, 0, 0}
};

xQueueHandle xStirQueue1, xStirQueue2;

void vStirInit(void)
{
  unsigned long ulFrequency;
  GPIO_InitTypeDef GPIO_InitStructure;


  //CONTROL PINS
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_InitStructure.GPIO_Pin =  STIR_STEP_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //Alt Function
  GPIO_Init( STIR_DRIVE_PORT, &GPIO_InitStructure );
  //GPIO_SetBits(STIR_DRIVE_PORT, STIR_STEP_PIN);

  GPIO_InitStructure.GPIO_Pin =  STIR_ENABLE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //Alt Function
  GPIO_Init( STIR_CONTROL_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(STIR_CONTROL_PORT, STIR_ENABLE_PIN);

  GPIO_InitStructure.GPIO_Pin =  STIR_DIR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //Alt Function
  GPIO_Init( STIR_CONTROL_PORT, &GPIO_InitStructure );
  GPIO_SetBits(STIR_CONTROL_PORT, STIR_DIR_PIN);

  //TIM3->ARR = 12000;

  vSemaphoreCreateBinary(xAppletRunningSemaphore);

  xStirQueueHandle = xQueueCreate(10, sizeof(struct Step *));

  // Create a queue capable of containing 10 unsigned long values.
  xStirQueue1 = xQueueCreate( 10, sizeof( unsigned long ) );

  // Create a queue capable of containing 10 pointers to AMessage structures.
  // These should be passed by pointer as they contain a lot of data.
  xStirQueue2 = xQueueCreate( 50, sizeof( struct Step * ) );

  xTaskCreate( vTaskStir,
      ( signed portCHAR * ) "Stir",
      configMINIMAL_STACK_SIZE +500,
      NULL,
      tskIDLE_PRIORITY,
      &xStirTaskHandle );

}

volatile uint8_t stir_state = OFF;

#define MAX_SPEED_ARR 6800 //The maximum speed. (the lower, the faster)
#define MIN_SPEED_ARR 40000 //Motor stopped ARR value
#define SPEED_RANGE (MIN_SPEED_ARR - MAX_SPEED_ARR)

void vStirFunc(struct Step * step)
{

  TIM_Cmd( TIM3, ENABLE );
      uint16_t uCurrentSpeed,  uInitialSpeed = TIM3->ARR, uSetSpeed;
      uint8_t flag = 0;
      if (step->ucDirection == CW)
        GPIO_WriteBit(STIR_CONTROL_PORT, STIR_DIR_PIN, 1);
      else
        GPIO_WriteBit(STIR_CONTROL_PORT, STIR_DIR_PIN, 0);

      GPIO_WriteBit(STIR_CONTROL_PORT, STIR_ENABLE_PIN, ON);

      uSetSpeed = (MIN_SPEED_ARR - (step->uSpeed * (SPEED_RANGE/100))); //convert speed % to ARR value
      if (step->uSpeed == 0)
        flag = 1;
      if (uSetSpeed > uInitialSpeed) //are we increasing from here or decreasing?
        {
          printf("Decreasing\r\n");
          //Decrease Speed
          //up counting decreases speed... then stop when steps get too large
          for (uCurrentSpeed = uInitialSpeed; uCurrentSpeed < uSetSpeed ; uCurrentSpeed+=100)
            {

                        //if (uCurrentSpeed >= MIN_SPEED_ARR || step->uTime == 0)
                         if(flag)
                          {
                            //TIM_Cmd(TIM3, DISABLE);
                            TIM_SetAutoreload(TIM3, MIN_SPEED_ARR);
                            //crane_state = STOPPED;
                            printf("timer3 disabled\r\n");
                            GPIO_WriteBit(STIR_CONTROL_PORT, STIR_ENABLE_PIN, OFF);
                            break;
                          }
              TIM_SetAutoreload(TIM3, uCurrentSpeed);
              vTaskDelay(10);
            }
        }
      else
        {
          printf("Increasing\r\n");
          //increase speed
          for (uCurrentSpeed = uInitialSpeed; uCurrentSpeed > uSetSpeed; uCurrentSpeed-=100)
            {
              vTaskDelay(10);
              TIM_SetAutoreload(TIM3, uCurrentSpeed);
            }
        }
      printf("initial speed was = %u\r\n", uInitialSpeed);
      printf("current speed is = %u\r\n", uCurrentSpeed);
      printf("set speed is  = %u\r\n", uSetSpeed);

      if (uCurrentSpeed < MIN_SPEED_ARR)
         stir_state = ON;
      else stir_state = OFF;

}



void vTaskStir(void * pvParameters)
{
  struct Step * xMessage;
  portBASE_TYPE xStatus;
  for (;;)
    {
      xStatus = xQueueReceive(xStirQueue2, &(xMessage), portMAX_DELAY);
      if (xStatus == pdTRUE)
        {
          printf("received step: %d\r\n", xMessage->uTime);
          vStirFunc(xMessage);
          vTaskDelay(xMessage->uTime * 1000); //delay for the time required (* 1000 gives seconds)
          //something in queue
        }
      else
        {
          //  printf("Waiting!\r\n");
          //queue empty
        }


    }
}



void vStirAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (stir_state)
      {
      case ON:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"STIRRING");
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
              lcd_printf(1,13,11,"NOT STIRRING");
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

#define CW_X1 0
#define CW_Y1 30
#define CW_X2 150
#define CW_Y2 100
#define CW_W (CW_X2-CW_X1)
#define CW_H (CW_Y2-CW_Y1)

#define CCW_X1 0
#define CCW_Y1 105
#define CCW_X2 150
#define CCW_Y2 175
#define CCW_W (CCW_X2-CCW_X1)
#define CCW_H (CCW_Y2-CCW_Y1)



#define ST_X1 155
#define ST_Y1 30
#define ST_X2 300
#define ST_Y2 175
#define ST_W (ST_X2-ST_X1)
#define ST_H (ST_Y2-ST_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vStirApplet(int init){
  if (init)
    {
      lcd_DrawRect(CW_X1, CW_Y1, CW_X2, CW_Y2, Red);
      lcd_fill(CW_X1+1, CW_Y1+1, CW_W, CW_H, Blue);
      lcd_DrawRect(CCW_X1, CCW_Y1, CCW_X2, CCW_Y2, Red);
      lcd_fill(CCW_X1+1, CCW_Y1+1, CCW_W, CCW_H, Blue);
      lcd_DrawRect(ST_X1, ST_Y1, ST_X2, ST_Y2, Cyan);
      lcd_fill(ST_X1+1, ST_Y1+1, ST_W, ST_H, Red);
      lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
      lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
      lcd_printf(10,1,12,  "MANUAL STIR");
      lcd_printf(8,4,2, "CW");
      lcd_printf(8,8,2, "CCW");
      lcd_printf(26,6,4, "STOP");
      lcd_printf(30, 13, 4, "Back");

      xTaskCreate( vStirAppletDisplay,
          ( signed portCHAR * ) "Stir_display",
          configMINIMAL_STACK_SIZE +500,
          NULL,
          tskIDLE_PRIORITY + 1,
          &xStirAppletDisplayHandle );
    }

}



int iStirKey(int xx, int yy){

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  static struct Step CW_Step = {CW, 100, 1};
  static struct Step CCW_Step = {CCW, 100, 1};
  static struct Step STOP_Step = {CW, 0, 1};
  struct Step *pxMessage;


  if (xx > CW_X1+1 && xx < CW_X2-1 && yy > CW_Y1+1 && yy < CW_Y2-1)
    {

      //pxMessage = &xSteps[0];
      //pxMessage = &MashStirSteps[0];
      pxMessage = &CW_Step;
      //while (pxMessage->ucDirection != 0)
      // {
      xQueueSendToBack( xStirQueue2, ( void * ) &pxMessage, portMAX_DELAY );
      //pxMessage++;
      // }

    }
  else if (xx > CCW_X1+1 && xx < CCW_X2-1 && yy > CCW_Y1+1 && yy < CCW_Y2-1)
    {

      pxMessage = &CCW_Step;
      xQueueSendToBack( xStirQueue2, ( void * ) &pxMessage, portMAX_DELAY );

    }
  else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
    {
      pxMessage = &STOP_Step;
      xQueueSendToBack( xStirQueue2, ( void * ) &pxMessage, portMAX_DELAY );
    }

  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {

      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xStirAppletDisplayHandle != NULL)
        {
          vTaskDelete(xStirAppletDisplayHandle);
          vTaskDelay(100);
          xStirAppletDisplayHandle = NULL;
        }


      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

