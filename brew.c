//-------------------------------------------------------------------------
// Author: Brad Goold
// Date: 14 Nov 2013
// Email Address: W0085400@umail.usq.edu.au
//
// Purpose:
// Pre:
// Post:
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "console.h"
#include "ds1820.h"
#include "brew.h"

// Applet States
#define MAIN 0
#define GRAPH 1
#define STATS 2
#define RES  3
#define QUIT 4
#define OTHER 5

#define MAX_BREW_STEPS 5

#define CLEAR_APPLET_CANVAS lcd_fill(CANVAS_X1+1,CANVAS_Y1+1, CANVAS_W, CANVAS_H, Black);


#define DRAW_RESUME_BUTTONS lcd_DrawRect(BUTTON_4_X1, BUTTON_4_Y1, BUTTON_4_X2, BUTTON_4_Y2, Red); \
    lcd_fill(BUTTON_4_X1+1, BUTTON_4_Y1+1, BUTTON_4_W, BUTTON_4_H, Blue); \
    lcd_DrawRect(BUTTON_5_X1, BUTTON_5_Y1, BUTTON_5_X2, BUTTON_5_Y2, Red); \
    lcd_fill(BUTTON_5_X1+1, BUTTON_5_Y1+1, BUTTON_5_W, BUTTON_5_H, Blue); \
    lcd_DrawRect(BUTTON_6_X1, BUTTON_6_Y1, BUTTON_6_X2, BUTTON_6_Y2, Red); \
    lcd_fill(BUTTON_6_X1+1, BUTTON_6_Y1+1, BUTTON_6_W, BUTTON_6_H, Blue);

#define DRAW_RESUME_BUTTON_TEXT  lcd_printf(1,13,5, "UP"); \
    lcd_printf(9,13,5, "DOWN"); \
    lcd_printf(17,13,5, "RESUME"); //Button6



void vBrewAppletDisplay( void *pvParameters);
void vBrewGraphAppletDisplay(void * pvParameters);
void vBrewStatsAppletDisplay(void * pvParameters);
void vBrewResAppletDisplay(void * pvParameters);
void vBrewPause(void);
void vBrewApplet(int init);


xTaskHandle xBrewTaskHandle = NULL,
            xBrewAppletDisplayHandle = NULL,
            xBrewGraphAppletDisplayHandle = NULL,
            xBrewStatsAppletDisplayHandle = NULL,
            xBrewResAppletDisplayHandle = NULL;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xBrewAppletRunningSemaphore;

static int iAppletState = MAIN;

// Define Structure for brew steps
static struct BrewStep
    {
        const char * pcStepName;
        int (*func)(void *);
        void (*task)(void *);
        uint16_t uTimeout;
        uint32_t uStartTime;
        uint16_t uElapsedTime;
    } Brew[]; //Brew is an array of brew steps.

struct State
{
      unsigned char ucStep;
      portTickType xLastWakeTime;
      unsigned char ucTarget;
      unsigned char ucHopAddition;
      unsigned char ucTargetHLTVolume;
      unsigned char ucTargetHLTTemp;
      unsigned char ucTargetWaterVolume;
      unsigned char ucHLTState;
} BrewState = {0,0,0,0,0,0,0,0};



//----------------------------------------------------------------------------------------------------------------------------
// Brew Task
//----------------------------------------------------------------------------------------------------------------------------
void vTaskBrew(void * pvParameters)
{
  portTickType xBrewStart, xBrewLastWakeTime;
  static uint16_t uBrewSecondsElapsed = 0, uBrewMinutesElapsed = 0, uBrewHoursElapsed=0;
  xBrewStart = xTaskGetTickCount();
  xBrewLastWakeTime = xTaskGetTickCount();
  char pcBrewElapsedTime[9];
  uint32_t uStartingStep = (uint32_t)pvParameters;
  BrewState.ucStep = uStartingStep;

  for(;;)
    {
      sprintf(pcBrewElapsedTime, "%02u:%02u:%02u\r\n", uBrewHoursElapsed, uBrewMinutesElapsed, uBrewSecondsElapsed);
      vConsolePrint(pcBrewElapsedTime);

      // code here

      uBrewSecondsElapsed++;
      if (uBrewSecondsElapsed == 60)
        {
          uBrewSecondsElapsed = 0;
          uBrewMinutesElapsed++;
        }

      if (uBrewMinutesElapsed == 60)
        {
          uBrewMinutesElapsed = 0;
          uBrewHoursElapsed++;
        }
      vTaskDelayUntil(&xBrewLastWakeTime, 1000 / portTICK_RATE_MS );

    }

}

//----------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------
// BREW PAUSE
//----------------------------------------------------------------------------------------------------------------------------
void vBrewPause(void){

}


//----------------------------------------------------------------------------------------------------------------------------
// BREW APPLET - Called from menu
//----------------------------------------------------------------------------------------------------------------------------
void vBrewApplet(int init){

  if (init)
    {
      lcd_DrawRect(TOP_BANNER_X1, TOP_BANNER_Y1, TOP_BANNER_X2, TOP_BANNER_Y2, Cyan);
      lcd_fill(TOP_BANNER_X1+1, TOP_BANNER_Y1+1, TOP_BANNER_W, TOP_BANNER_H, Blue);
      //lcd_printf(12,0,13,"BREW");
      lcd_printf(15,1,13,"BREW");

      lcd_DrawRect(BUTTON_1_X1, BUTTON_1_Y1, BUTTON_1_X2, BUTTON_1_Y2, Red);
      lcd_fill(BUTTON_1_X1+1, BUTTON_1_Y1+1, BUTTON_1_W, BUTTON_1_H, Green);
      lcd_DrawRect(BUTTON_2_X1, BUTTON_2_Y1, BUTTON_2_X2, BUTTON_2_Y2, Red);
      lcd_fill(BUTTON_2_X1+1, BUTTON_2_Y1+1, BUTTON_2_W, BUTTON_2_H, Green);
      lcd_DrawRect(BUTTON_3_X1, BUTTON_3_Y1, BUTTON_3_X2, BUTTON_3_Y2, Red);
      lcd_fill(BUTTON_3_X1+1, BUTTON_3_Y1+1, BUTTON_3_W, BUTTON_3_H, Green);

      lcd_DrawRect(BUTTON_4_X1, BUTTON_4_Y1, BUTTON_4_X2, BUTTON_4_Y2, Red);
      lcd_fill(BUTTON_4_X1+1, BUTTON_4_Y1+1, BUTTON_4_W, BUTTON_4_H, Blue);
      lcd_DrawRect(BUTTON_5_X1, BUTTON_5_Y1, BUTTON_5_X2, BUTTON_5_Y2, Red);
      lcd_fill(BUTTON_5_X1+1, BUTTON_5_Y1+1, BUTTON_5_W, BUTTON_5_H, Blue);
      lcd_DrawRect(BUTTON_6_X1, BUTTON_6_Y1, BUTTON_6_X2, BUTTON_6_Y2, Red);
      lcd_fill(BUTTON_6_X1+1, BUTTON_6_Y1+1, BUTTON_6_W, BUTTON_6_H, Blue);

      lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, White);
      lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Red);

      lcd_printf(33,1,13,"GRAPH"); //Button1
      lcd_printf(33,5,13,"STATS"); //Button2
      lcd_printf(33,8,13,"RESUME"); //Button3
      lcd_printf(30,13,10, "QUIT");
      lcd_printf(1,13,5, "START"); //Button4
      lcd_printf(9,13,5, "PAUSE"); //Button5
      lcd_printf(17,13,5, "SPARE"); //Button6


      //create a dynamic display task
      vConsolePrint("Brew Applet Opened\r\n");
      vSemaphoreCreateBinary(xBrewAppletRunningSemaphore);

      xTaskCreate( vBrewAppletDisplay,
          ( signed portCHAR * ) "BrewDisp",
          configMINIMAL_STACK_SIZE + 200,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewAppletDisplayHandle );

      xTaskCreate( vBrewGraphAppletDisplay,
          ( signed portCHAR * ) "BrewGraph",
          configMINIMAL_STACK_SIZE,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewGraphAppletDisplayHandle );

      vTaskSuspend(xBrewGraphAppletDisplayHandle);

      xTaskCreate( vBrewStatsAppletDisplay,
          ( signed portCHAR * ) "BrewStats",
          configMINIMAL_STACK_SIZE,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewStatsAppletDisplayHandle );

      vTaskSuspend(xBrewStatsAppletDisplayHandle);

      xTaskCreate( vBrewResAppletDisplay,
          ( signed portCHAR * ) "BrewRes",
          configMINIMAL_STACK_SIZE,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewResAppletDisplayHandle );

      vTaskSuspend(xBrewResAppletDisplayHandle);
      vConsolePrint("Brew Applet Tasks Created\r\n");
      vTaskDelay(200);
    }
  else vConsolePrint("!init\r\n");

}

//----------------------------------------------------------------------------------------------------------------------------
// BREW GRAPH SUB-APPLET
//----------------------------------------------------------------------------------------------------------------------------
void vBrewGraphAppletDisplay(void * pvParameters){
int i = 0;
  CLEAR_APPLET_CANVAS;
for (;;)
  {
    vConsolePrint("Graph Display Applet Running\r\n");

    vTaskDelay(500);
  }

}


//----------------------------------------------------------------------------------------------------------------------------
// BREW STATISTICS SUB-APPLET
//----------------------------------------------------------------------------------------------------------------------------
void vBrewStatsAppletDisplay(void * pvParameters){

   CLEAR_APPLET_CANVAS;
   for (;;)
     {
       vConsolePrint("Stats Display Applet Running\r\n");
       vTaskDelay(500);
     }

}


void vBrewResAppletDisplay(void * pvParameters){
  static unsigned char ucLastStep;
  ucLastStep = 255;
  CLEAR_APPLET_CANVAS;
  DRAW_RESUME_BUTTONS;
  DRAW_RESUME_BUTTON_TEXT;

   for (;;)
     {
       if (ucLastStep != BrewState.ucStep)
         {
           CLEAR_APPLET_CANVAS;
           if (BrewState.ucStep > 0)
             lcd_printf(1,4,10, "  STEP %d: %s", BrewState.ucStep-1, Brew[BrewState.ucStep-1].pcStepName);
           lcd_printf(1,5,10, "->STEP %d: %s", BrewState.ucStep, Brew[BrewState.ucStep].pcStepName);
           if (BrewState.ucStep < MAX_BREW_STEPS)
             lcd_printf(1,6,10, "  STEP %d: %s", BrewState.ucStep+1, Brew[BrewState.ucStep+1].pcStepName);
           vConsolePrint("Resume Display Applet Running\r\n");

           ucLastStep = BrewState.ucStep;
         }
       vTaskDelay(200);
     }


}

#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%d.%d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));

void vBrewAppletDisplay( void *pvParameters){

     static char tog = 0; //toggles each loop
        unsigned int uiDecimalPlaces = 3;
        float fHLTTemp = 0, fMashTemp = 0, fAmbientTemp = 0, fCabinetTemp;
        float fNumber = 54.3211;
        //TEMPLATE
//        printf("***********whole = %d.%d \n\r", (unsigned int)floor(fNumber), (unsigned int)((fNumber-floor(fNumber))*pow(10, uiDecimalPlaces)));

        static uint8_t hlt_last = 0, mash_last = 0, boil_last = 0, inlet_last = 0, chiller_last = 0;


        for(;;)
          {

            xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
            //return to the menu system until its returned

            fHLTTemp = ds1820_get_temp(HLT);
            fMashTemp = ds1820_get_temp(MASH);
            fAmbientTemp = ds1820_get_temp(AMBIENT);
            fCabinetTemp = ds1820_get_temp(CABINET);

            if(tog)
              {
                CLEAR_APPLET_CANVAS;
                LCD_FLOAT(1,7,2,27.451);
                lcd_printf(1, 8, 25, "HLT = %d.%ddegC", (unsigned int)floor(fHLTTemp), (unsigned int)((fHLTTemp-floor(fHLTTemp))*pow(10, 1)));
                lcd_printf(1, 9, 25, "MASH = %d.%ddegC", (unsigned int)floor(fMashTemp), (unsigned int)((fMashTemp-floor(fMashTemp))*pow(10, 1)));
                lcd_printf(1, 10, 25, "Cabinet = %d.%ddegC", (unsigned int)floor(fCabinetTemp), (unsigned int)((fCabinetTemp-floor(fCabinetTemp))*pow(10, 1)));
                lcd_printf(1, 11, 25, "Ambient = %d.%ddegC", (unsigned int)floor(fAmbientTemp), (unsigned int)((fAmbientTemp-floor(fAmbientTemp))*pow(10, 1)));

              }
            else{
               CLEAR_APPLET_CANVAS;
            }

            tog = tog ^ 1;

            xSemaphoreGive(xBrewAppletRunningSemaphore); //give back the semaphore as its safe to return now.
            vTaskDelay(200);

          }

}


static uint8_t uEvaluateTouch(int xx, int yy)
{

  if (xx > BUTTON_1_X1+5 && xx < BUTTON_1_X2-5 && yy > BUTTON_1_Y1+5 && yy < BUTTON_1_Y2-5)
      return BUTTON_1;
  if (xx > BUTTON_2_X1+5 && xx < BUTTON_2_X2-5 && yy > BUTTON_2_Y1+5 && yy < BUTTON_2_Y2-5)
      return BUTTON_2;
  if (xx > BUTTON_3_X1+5 && xx < BUTTON_3_X2-5 && yy > BUTTON_3_Y1+5 && yy < BUTTON_3_Y2-5)
      return BUTTON_3;
  if (xx > BUTTON_4_X1+5 && xx < BUTTON_4_X2-5 && yy > BUTTON_4_Y1+5 && yy < BUTTON_4_Y2-5)
      return BUTTON_4;
  if (xx > BUTTON_5_X1+5 && xx < BUTTON_5_X2-5 && yy > BUTTON_5_Y1+5 && yy < BUTTON_5_Y2-5)
      return BUTTON_5;
  if (xx > BUTTON_6_X1+5 && xx < BUTTON_6_X2-5 && yy > BUTTON_6_Y1+5 && yy < BUTTON_6_Y2-5)
      return BUTTON_6;
  if (xx > BK_X1+5 && xx < BK_X2-5 && yy > BK_Y1+5 && yy < BK_Y2-5)
      return QUIT_SUB;

  return NO_BUTTON;

}

int iBrewKey(int xx, int yy)
{
  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  int iKeyFromApplet = 0;
  uint8_t uButton;
  uButton = uEvaluateTouch(xx,yy);
  switch (iAppletState)
  {
  case GRAPH:
    {
      vConsolePrint("AppletState = GRAPH\r\n");
      switch(uButton)
      {
      case BUTTON_1:
        {
          iAppletState = GRAPH;
          break;
        }
      case BUTTON_2:
        {
          iAppletState = STATS;
          break;
        }
      case BUTTON_3:
        {
          iAppletState = RES;
          break;
        }
      case BUTTON_4:
        {
          break;
        }
      case BUTTON_5:
        {
          break;
        }
      case BUTTON_6:
        {
          break;
        }

      case QUIT_SUB:
        {
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewGraphAppletDisplayHandle);
          vTaskResume(xBrewAppletDisplayHandle);
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          iAppletState = MAIN;

          break;
        }
      case NO_BUTTON:
        {
          break;
        }
      }// switch uButton
      break;
    }// case GRAPH
  case STATS:
    {
      vConsolePrint("AppletState = STATS\r\n");
      switch(uButton)
      {
      case BUTTON_1:
        {
          iAppletState = GRAPH;
          break;
        }
      case BUTTON_2:
        {
          iAppletState = STATS;
          break;
        }
      case BUTTON_3:
        {
          iAppletState = RES;
          break;
        }
      case BUTTON_4:
        {
          break;
        }
      case BUTTON_5:
        {
          break;
        }
      case BUTTON_6:
        {
          break;
        }

      case QUIT_SUB:
        {
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewStatsAppletDisplayHandle);
          vTaskResume(xBrewAppletDisplayHandle);
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          iAppletState = MAIN;
          break;
        }
      case NO_BUTTON:
        {
          break;
        }
      }// switch uButton
      break;
    }// case STATS

  case RES:
    {
      vConsolePrint("AppletState = RES\r\n");
      switch(uButton)
      {
      case BUTTON_1:
        {
          iAppletState = GRAPH;
          break;
        }
      case BUTTON_2:
        {
          iAppletState = STATS;
          break;
        }
      case BUTTON_3:
        {
          iAppletState = RES;
          break;
        }
      case BUTTON_4:
        {
          if (BrewState.ucStep < MAX_BREW_STEPS)
            {
              BrewState.ucStep++;
              vConsolePrint("Increasing Brew Step\r\n");
            }
          break;
        }
      case BUTTON_5:
        {
          if (BrewState.ucStep > 0)
            {
              BrewState.ucStep--;
              vConsolePrint("Decreasing Brew Step\r\n");
            }
          break;
        }
      case BUTTON_6:
        {
          break;
        }

      case QUIT_SUB:
        {
          vConsolePrint("Leaving Stats Applet\r\n");
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewResAppletDisplayHandle);
          vTaskResume(xBrewAppletDisplayHandle);
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          iAppletState = MAIN;
          break;
        }
      case NO_BUTTON:
        {
          break;
        }
      }// switch uButton
      break;
    }// case RES
  case MAIN:
    {
      switch(uButton)
      {
      case BUTTON_1:
        {
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewAppletDisplayHandle);
          vTaskSuspend(xBrewStatsAppletDisplayHandle);
          vTaskSuspend(xBrewResAppletDisplayHandle);
          CLEAR_APPLET_CANVAS;
          vTaskResume(xBrewGraphAppletDisplayHandle);
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          vConsolePrint("Button1 Pressed\r\n");
          iAppletState = GRAPH;

          break;
        }
      case BUTTON_2:
        {
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewAppletDisplayHandle);
          vTaskSuspend(xBrewGraphAppletDisplayHandle);
          vTaskSuspend(xBrewResAppletDisplayHandle);
          CLEAR_APPLET_CANVAS;
          vTaskResume(xBrewStatsAppletDisplayHandle);
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          vConsolePrint("Button2 Pressed\r\n");
          iAppletState = STATS;

          break;
        }
      case BUTTON_3:
        {
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewAppletDisplayHandle);
          vTaskSuspend(xBrewGraphAppletDisplayHandle);
          vTaskSuspend(xBrewStatsAppletDisplayHandle);

          CLEAR_APPLET_CANVAS;
          DRAW_RESUME_BUTTONS;
          DRAW_RESUME_BUTTON_TEXT;

          vTaskResume(xBrewResAppletDisplayHandle);

          xSemaphoreGive(xBrewAppletRunningSemaphore);


          iAppletState = RES;
          vConsolePrint("Button3 Pressed\r\n");

          break;
        }
      case BUTTON_4:
        {
          break;
        }
      case BUTTON_5:
        {
          break;
        }
      case BUTTON_6:
        {
          break;
        }

      case QUIT_SUB:
        {
          iAppletState = QUIT;
          vConsolePrint("QUIT Pressed\r\n");
          break;
        }
      case NO_BUTTON:
        {
          break;
        }

      }// switch uButton
      break;
    }// case STATS
  case QUIT:
    {
      xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
      vTaskSuspend(xBrewAppletDisplayHandle);
      lcd_DrawRect(Q_X1, Q_Y1, Q_X2, Q_Y2, White);
      lcd_fill(Q_X1+1, Q_Y1+1, Q_W, Q_H, Blue);
      lcd_printf(13,4,13,"QUIT???");
      lcd_printf(13,5,13,"IF YES PRESS");
      lcd_printf(13,6,13,"QUIT AGAIN");
      lcd_printf(13,7,13,"IF NO, PRESS");
      lcd_printf(13,8,13,"HERE");
      vConsolePrint("Press back again to exit BREW\r\n");

      if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
        {
          vConsolePrint("Leaving BREW Applet\r\n");

          vTaskResume(xBrewAppletDisplayHandle);
          if (xBrewAppletDisplayHandle != NULL)
            {
              vTaskDelete(xBrewAppletDisplayHandle);
              vTaskDelay(100);
              xBrewAppletDisplayHandle = NULL;
            }

          //return the semaphore for taking by another task.
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          iAppletState = MAIN;
          return 1;
        }
      else if (xx > Q_X1 && yy > Q_Y1 && xx < Q_X2 && yy < Q_Y2)
        {
          vTaskResume(xBrewAppletDisplayHandle);
          iAppletState = MAIN;
          vConsolePrint("Staying BREW Applet\r\n");
        }
      xSemaphoreGive(xBrewAppletRunningSemaphore);
      break;
    }// case QUIT
  }//Switch iAppletState

    vTaskDelay(10);
    return 0;
}



//===================================================================================================================================================






static struct BrewStep Brew[] = {
    {"Waiting",          NULL,                   NULL,                 0},
    {"Close Valves",          NULL,                   NULL,                 0},
    {"Fill HLT Full",          NULL,                   NULL,                 0},
    {"Heat HLT",          NULL,                   NULL,                 0},
    {"Grind Grains",          NULL,                   NULL,                 0},
    {"Drain HLT",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0}
};

