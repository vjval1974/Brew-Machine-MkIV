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
#include <stdlib.h>
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
#include "timers.h"
#include "console.h"
#include "brew.h"


volatile int8_t cs = STOPPED;
volatile int8_t iCraneState = STOPPED;

static const int STEP_COMPLETE = BREW_STEP_COMPLETE;
static const int STEP_FAILED = BREW_STEP_FAILED;
static const int STEP_TIMEOUT = BREW_STEP_TIMEOUT;



//xQueueHandle xCraneQueueHandle;
xQueueHandle xCraneQueue;
xSemaphoreHandle xAppletRunningSemaphore;
xTaskHandle xCraneTaskHandle = NULL, xCraneAppletDisplayHandle = NULL;

void vTaskCrane(void * pvParameters);

struct CraneCommand
{
  int8_t cDirection; //1 = up, 2 = down , -1 = stopped
  unsigned char ucEnable; // off on
};

struct CraneCommand Stop = {STOP, 1};
struct CraneCommand Up = {UP, 1};
struct CraneCommand Down = {DN, 1};


// Helper Function to debounce the input from the dry contacts on the relays
uint8_t debounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  uint8_t read1, read2;
  read1 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
  vTaskDelay(5);
  read2 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
  if (read1 == read2)
    return read1;
  else return 0;

}

void * pvCraneTime = (void *)1;
//void vCraneTimerCallback(xTimerHandle xHandle);
uint16_t uCraneTime;
xTimerHandle xCraneTimerHandle;

void vCraneInit(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // Set up the input pin configuration for PE4
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  CRANE_UPPER_LIMIT_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  CRANE_LOWER_LIMIT_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

  vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
  vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low

  vSemaphoreCreateBinary(xAppletRunningSemaphore);
  xCraneQueue = xQueueCreate( 1, sizeof( int ) );

  xTaskCreate( vTaskCrane,
      ( signed portCHAR * ) "Crane",
      configMINIMAL_STACK_SIZE +500,
      NULL,
      tskIDLE_PRIORITY,
      &xCraneTaskHandle );

  printf("Crane Initialised\r\n");

}


void vCraneFunc(int Command)
{
  //portENTER_CRITICAL(); // so that we dont get switched out whilst accessing these pins.

  switch (Command)
  {
  case UP:
    {
      // need to turn relay 2 off which sets up for REV direction
      vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
      vPCF_SetBits(CRANE_PIN1, CRANE_PORT); //pull low
      break;
    }
  case DN:
    {
      // need to turn relay 1 off which sets up for REV direction
      vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
      vPCF_SetBits(CRANE_PIN2, CRANE_PORT); //pull low
      break;
    }

  default:
    {
      vPCF_ResetBits(CRANE_PIN1, CRANE_PORT); //pull low
      vPCF_ResetBits(CRANE_PIN2, CRANE_PORT); //pull low
      break;
    }
  }
  //portEXIT_CRITICAL();

}

void vTaskCrane(void * pvParameters)
{
  int xMessage;
  static int xLastMessage;
  uint8_t limit = 0xFF, limit1 = 0xFF; //neither on or off.

  xLastMessage = STOP;
  portBASE_TYPE xStatus;
  for (;;)
    {
      //vConsolePrint("crane task called\r\n");
      xStatus = xQueueReceive(xCraneQueue, &xMessage, 50); // Check the queue every 0.05 second for a new command

      // NEED to check that xMessage isn't returned NULL when no message in queue
      if (xStatus != pdTRUE)
        {
          xMessage = xLastMessage;
        }
      else
        {
          vConsolePrint("Crane Task has received a message\r\n");
        }


      switch(iCraneState)
      {
      case TOP:
        {
          if (xMessage == DN)
            {
              vCraneFunc(DN);
              iCraneState = DRIVING_DOWN;
            }
          else if (xMessage == DN_INC)
            {
              iCraneState = DRIVING_DOWN_INC;
            }
          break;
        }
      case BOTTOM:
        {
          if (xMessage == UP)
            {
              vCraneFunc(UP);
              iCraneState = DRIVING_UP;
            }
          break;
        }
      case DRIVING_UP:
        {
          if (xMessage == STOP)
            {
              vCraneFunc(STOP);
              vConsolePrint("STOP while DU \r\n");
              iCraneState = STOPPED;
            }
          else if (xMessage == DN)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit != 1)
                {
                  vCraneFunc(xMessage);
                  iCraneState = DRIVING_DOWN;
                }
              else
                iCraneState = BOTTOM;
            }
          else
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              if (limit == 1)
                {
                  vCraneFunc(STOP);
                  vConsolePrint("STOP limit DU \r\n");
                  iCraneState = TOP;
                }
            }
          break;
        }
      case DRIVING_DOWN:
        {
          if (xMessage == STOP)
            {
              vCraneFunc(STOP);
              vConsolePrint("STOP while DDN \r\n");
              iCraneState = STOPPED;
            }
          else if (xMessage == UP)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              if (limit == 1)
                {
                  iCraneState = TOP;
                }
              else
                {
                  vCraneFunc(xMessage);
                  iCraneState = DRIVING_UP;
                }

            }
          else// if (xMessage->cDirection == DN)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit == 1)
                {
                  vTaskDelay(200);
                  vCraneFunc(STOP);
                  vConsolePrint("STOP limit DDN \r\n");
                  iCraneState = BOTTOM;
                }
            }

          break;
        }
      case DRIVING_DOWN_INC:
        {
          limit = debounce(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
          if(limit != 1)
            {
              vCraneFunc(DN);
              vConsolePrint("Crane DN \r\n");
              vTaskDelay(200);
              vCraneFunc(STOP);
              vConsolePrint("Crane STOP \r\n");
              vTaskDelay(1000);

            }
          else
            {
              // Make sure we are down
              vCraneFunc(DN);
              vTaskDelay(100);

              vConsolePrint("STOP limit DDN \r\n");
              vCraneFunc(STOP);
              iCraneState = BOTTOM;
              xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
            }
          break;
        }
      case STOPPED:
        {
          if (xMessage == UP)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
              if (limit != 1)
                {
                  vCraneFunc(xMessage);
                  iCraneState = DRIVING_UP;
                }
              else iCraneState = TOP;
            }
          else if (xMessage == DN)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit != 1)
                {
                  vCraneFunc(xMessage);
                  iCraneState = DRIVING_DOWN;
                }
              else iCraneState = BOTTOM;
            }
          else if (xMessage == DN_INC)
            {
              limit = debounce(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
              if (limit != 1)
                {

                  iCraneState = DRIVING_DOWN_INC;
                }
              else iCraneState = BOTTOM;
            }

          break;
        }
      default:
        {
          vCraneFunc(STOP); //stop the crane on an instant.
          iCraneState = STOPPED;
          break;

        }
      }// Switch
      xLastMessage = xMessage;


    }// Task Loop
}//func



void vCraneAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (iCraneState)
      {
      case DRIVING_UP:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"DRIVING UP");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case DRIVING_DOWN:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"DRIVING DOWN");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case TOP:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"TOP");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case BOTTOM:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"BOTTOM");
            }
          else
            {
              lcd_fill(1,210, 180,17, Black);
            }

          break;
        }
      case STOPPED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"STOPPED");
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

#define BAK_X1 200
#define BAK_Y1 190
#define BAK_X2 315
#define BAK_Y2 235
#define BAK_W (BAK_X2-BAK_X1)
#define BAK_H (BAK_Y2-BAK_Y1)

void vCraneApplet(int init){
  if (init)
    {
      lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, Red);
      lcd_fill(UP_X1+1, UP_Y1+1, UP_W, UP_H, Blue);
      lcd_DrawRect(DN_X1, DN_Y1, DN_X2, DN_Y2, Red);
      lcd_fill(DN_X1+1, DN_Y1+1, DN_W, DN_H, Blue);
      lcd_DrawRect(ST_X1, ST_Y1, ST_X2, ST_Y2, Cyan);
      lcd_fill(ST_X1+1, ST_Y1+1, ST_W, ST_H, Red);
      lcd_DrawRect(BAK_X1, BAK_Y1, BAK_X2, BAK_Y2, Cyan);
      lcd_fill(BAK_X1+1, BAK_Y1+1, BAK_W, BAK_H, Magenta);
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
  int pxMessage;


  if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
    {
      pxMessage = UP;
      xQueueSendToBack( xCraneQueue, ( void * ) &pxMessage, 0 );
    }
  else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
    {
      pxMessage = DN;
      xQueueSendToBack( xCraneQueue, ( void * ) &pxMessage, 0 );

    }
  else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
    {
      pxMessage = STOP;
      xQueueSendToBack( xCraneQueue, ( void * ) &pxMessage, portMAX_DELAY );


    }
  else if (xx > BAK_X1 && yy > BAK_Y1 && xx < BAK_X2 && yy < BAK_Y2)
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


