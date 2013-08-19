//-------------------------------------------------------------------------
// Author: Brad Goold
// Date: 18 Feb 2012
// Email Address: W0085400@umail.usq.edu.au
// 
// Purpose:
// Pre:
// Post:
// RCS $Date$
// RCS $Revision$
// RCS $Source$
// RCS $Log$
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "crane.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "I2C-IO.h"


#define UP 1
#define DN 255
#define DRIVING 1
#define STOPPED -1


#define TIMER_USED TIM3



xQueueHandle xCraneQueueHandle;

xSemaphoreHandle xAppletRunningSemaphore;

xTaskHandle xCraneTaskHandle = NULL, xCraneAppletDisplayHandle = NULL;
void vTaskCrane(void * pvParameters);

struct Step
{
  unsigned char ucDirection; //1 = up, -1 = down
  unsigned char ucEnable;
  //int uSpeed; //final speed in percent
  //int uTime; //acceleration delay (controls rate of acceleration)

  //        char ucMessageID;
  //        char ucData[ 20 ];
};

struct Step Stop = {UP, STOPPED};

struct Step Up = {UP, DRIVING};
struct Step Down = {DN, DRIVING};



xQueueHandle xCraneQueue1, xCraneQueue2;
#if 1
void vCraneInit(void)
{


  vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
  vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
    printf("Crane Initialised\r\n");


  vSemaphoreCreateBinary(xAppletRunningSemaphore);
  xCraneQueue2 = xQueueCreate( 1, sizeof( struct Step * ) );

  xTaskCreate( vTaskCrane,
      ( signed portCHAR * ) "Crane",
      configMINIMAL_STACK_SIZE +500,
      NULL,
      tskIDLE_PRIORITY,
      &xCraneTaskHandle );

}
#endif



volatile int8_t crane_state = STOPPED;

void vCraneFunc(struct Step * step)
{
  portENTER_CRITICAL(); // so that we dont get switched out whilst accessing these pins.
  if (step->ucEnable == DRIVING)
    {
      if (step->ucDirection == UP)
        {
          // need to turn relay 2 off which sets up for REV direction
          vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
          vPCF_SetBits(CRANE_PIN1, CRANE_PORT); //pull low
        }
      else
        {
          // need to turn relay 1 off which sets up for REV direction
          vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
          vPCF_SetBits(CRANE_PIN2, CRANE_PORT); //pull low
        }
    }
  else
    {
      vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
      vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
      crane_state = STOPPED;
    }
  portEXIT_CRITICAL();
}


void vTaskCrane(void * pvParameters)
{
  struct Step * xMessage;
  uint8_t limit = 0xFF; //neither on or off.
  portBASE_TYPE xStatus;
  for (;;)
    {


      xStatus = xQueueReceive(xCraneQueue2, &(xMessage), 500); // Check the queue every 0.5 second for a new command
      if (xStatus == pdTRUE)
        {
          // AT THE MOMENT, THE BUTTONS DONT STOP THE CRANE WHEN DRIVING... need to fix
          vCraneFunc(xMessage);
          //need to check for the limit at the top/bottom
          //... but also need to code some logic in case the limit is reached whilst accelerating etc.
          ////

          while (crane_state == DRIVING)
            {
              //printf("%d, ", xMessage->ucDirection);

              if (xMessage->ucDirection == UP)
                {
                  limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);

                  if (limit == 0)
                    {
                      //                printf("Stopping on limit\r\n");
                      vCraneFunc(&Stop); //stop the crane on an instant.

                      break;

                    }
                }
              else if (xMessage->ucDirection == DN)
                {
                  limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

                  if (limit == 0)
                    {
                      //              printf("Stopping on limit\r\n");
                      vCraneFunc(&Stop); //stop the crane on an instant.

                      break;
                    }
                }
              taskYIELD();
            }
        }
      else
        {
          // printf("Waiting!\r\n");
          //queue empty
        }


    }
}



void vCraneAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (crane_state)
      {
      case DRIVING:
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
      case STOPPED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"NOT DRIVING");
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

#define UP_X1 0
#define UP_Y1 30
#define UP_X2 150
#define UP_Y2 100
#define UP_W (UP_X2-UP_X1)
#define UP_H (UP_Y2-UP_Y1)

#define DN_X1 0
#define DN_Y1 105
#define DN_X2 150
#define DN_Y2 175
#define DN_W (DN_X2-DN_X1)
#define DN_H (DN_Y2-DN_Y1)



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

void vCraneApplet(int init){
  if (init)
    {
      lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, Red);
      lcd_fill(UP_X1+1, UP_Y1+1, UP_W, UP_H, Blue);
      lcd_DrawRect(DN_X1, DN_Y1, DN_X2, DN_Y2, Red);
      lcd_fill(DN_X1+1, DN_Y1+1, DN_W, DN_H, Blue);
      lcd_DrawRect(ST_X1, ST_Y1, ST_X2, ST_Y2, Cyan);
      lcd_fill(ST_X1+1, ST_Y1+1, ST_W, ST_H, Red);
      lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
      lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
      lcd_printf(10,1,12,  "MANUAL CRANE");
      lcd_printf(8,4,2, "UP");
      lcd_printf(8,8,2, "DN");
      lcd_printf(26,6,4, "STOP");
      lcd_printf(30, 13, 4, "Back");

      xTaskCreate( vCraneAppletDisplay,
          ( signed portCHAR * ) "Crane_display",
          configMINIMAL_STACK_SIZE +500,
          NULL,
          tskIDLE_PRIORITY + 1,
          &xCraneAppletDisplayHandle );
    }

}



int iCraneKey(int xx, int yy){

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  struct Step *pxMessage;


  if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
    {
      //printf("up pressed\r\n");
      //pxMessage = &xSteps[0];
      //pxMessage = &MashStirSteps[0];
      pxMessage = &Up;
      //while (pxMessage->ucDirection != 0)
      // {
      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, 0 );
      //pxMessage++;
      // }

    }
  else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
    {
      //printf("down pressed\r\n");
      pxMessage = &Down;
      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, 0 );

    }
  else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
    {
      //printf("stop pressed\r\n");
      pxMessage = &Stop;

     // scrappy... needs state to be stopped to break into while loop in vTaskCrane
      crane_state = STOPPED;

      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, portMAX_DELAY );
    }

  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {

      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xCraneAppletDisplayHandle != NULL)
        {
          vTaskDelete(xCraneAppletDisplayHandle);
          vTaskDelay(100);
          xCraneAppletDisplayHandle = NULL;
        }


      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}


