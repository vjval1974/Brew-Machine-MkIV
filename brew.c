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
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "console.h"

// Applet States
#define MAIN 0
#define GRAPH 1
#define STATS 2
#define QUIT 3
#define OTHER 4


void vBrewAppletDisplay( void *pvParameters);

void vBrewApplet(int init);
xTaskHandle xBrewTaskHandle = NULL, xBrewAppletDisplayHandle = NULL;
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

//===================================================================================================================================================
#define TOP_BANNER_X1 0
#define TOP_BANNER_X2 254
#define TOP_BANNER_Y1 0
#define TOP_BANNER_Y2 29
#define TOP_BANNER_W (TOP_BANNER_X2-TOP_BANNER_X1)
#define TOP_BANNER_H (TOP_BANNER_Y2-TOP_BANNER_Y1)

#define BUTTON_1_X1 259
#define BUTTON_1_X2 318
#define BUTTON_1_Y1 0
#define BUTTON_1_Y2 49
#define BUTTON_1_W (BUTTON_1_X2-BUTTON_1_X1)
#define BUTTON_1_H (BUTTON_1_Y2-BUTTON_1_Y1)

#define BUTTON_2_X1 259
#define BUTTON_2_X2 318
#define BUTTON_2_Y1 54
#define BUTTON_2_Y2 104
#define BUTTON_2_W (BUTTON_2_X2-BUTTON_2_X1)
#define BUTTON_2_H (BUTTON_2_Y2-BUTTON_2_Y1)

#define BUTTON_3_X1 259
#define BUTTON_3_X2 318
#define BUTTON_3_Y1 109
#define BUTTON_3_Y2 159
#define BUTTON_3_W (BUTTON_3_X2-BUTTON_3_X1)
#define BUTTON_3_H (BUTTON_3_Y2-BUTTON_3_Y1)

#define BUTTON_4_X1 0
#define BUTTON_4_X2 60
#define BUTTON_4_Y1 190
#define BUTTON_4_Y2 235
#define BUTTON_4_W (BUTTON_4_X2-BUTTON_4_X1)
#define BUTTON_4_H (BUTTON_4_Y2-BUTTON_4_Y1)

#define BUTTON_5_X1 65
#define BUTTON_5_X2 125
#define BUTTON_5_Y1 190
#define BUTTON_5_Y2 235
#define BUTTON_5_W (BUTTON_5_X2-BUTTON_5_X1)
#define BUTTON_5_H (BUTTON_5_Y2-BUTTON_5_Y1)

#define BUTTON_6_X1 130
#define BUTTON_6_X2 190
#define BUTTON_6_Y1 190
#define BUTTON_6_Y2 235
#define BUTTON_6_W (BUTTON_6_X2-BUTTON_6_X1)
#define BUTTON_6_H (BUTTON_6_Y2-BUTTON_6_Y1)



#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

#define Q_X1 100
#define Q_Y1 50
#define Q_X2 200
#define Q_Y2 150
#define Q_W (Q_X2-Q_X1)
#define Q_H (Q_Y2-Q_Y1)



void vBrewApplet(int init){

  if (init)
    {

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

        lcd_printf(33,1,13,"GRAPH");
        lcd_printf(33,5,13,"STATS");
        lcd_printf(33,8,13,"OTHER");
        lcd_printf(30,12,10, "QUIT");
        }
/*
      if (uMashValveState)
        {
          lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Red);
          lcd_printf(12,4,13, "MASH->BOIL");
        }

      else
        {
          lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
          lcd_printf(12,4,13, "MASH->MASH");
        }


      if (uBoilValveState)
        {
          lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Red);
          lcd_printf(24,4,13, "BOIL OPENED");
        }
      else
        {
          lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Green);
          lcd_printf(24,4,13, "BOIL CLOSED");
        }

      if (uInletValveState)
        {
          lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Blue);
          lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Red);
          lcd_printf(0,8,13, "INLET OPENED");
        }

      else
        {
          lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
          lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
          lcd_printf(0,8,13, "INLET CLOSED");
        }

      if (uChillerValveState)
             {
               lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Blue);
               lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Red);
               lcd_printf(12,8,13, "CHILLER OPENED");
             }

           else
             {
               lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Cyan);
               lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Green);
               lcd_printf(12,8,13, "CHILLER CLOSED");
             }


      //                lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Cyan);
          //                lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Green);
          //                lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
          //                lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
          //                lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Cyan);
          //                lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Green);
          //                lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
          //                lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
          //                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
          //                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
//          lcd_printf(10,1,18,  "MANUAL VALVE APPLET");
//          lcd_printf(0,4,13, "HLT_VALVE");
//          lcd_printf(12,4,13, "MASH VALVE");
//          lcd_printf(24,4,13, "BOIL VALVE");
//          lcd_printf(0,8,13, "INLET VALVE");
//          lcd_printf(30, 13, 4, "Back");
//          //vTaskDelay(2000);
          //adc_init();
          //adc_init();
          //create a dynamic display task
           */
       vSemaphoreCreateBinary(xBrewAppletRunningSemaphore);

      xTaskCreate( vBrewAppletDisplay,
          ( signed portCHAR * ) "BrewDisp",
          configMINIMAL_STACK_SIZE + 200,
          NULL,
          tskIDLE_PRIORITY,
         &xBrewAppletDisplayHandle );

    }


}


void vBrewAppletDisplay( void *pvParameters){

  for(;;)
    vTaskDelay(500);
  /*    static char tog = 0; //toggles each loop

        uint8_t uHLTValveState = CLOSED;
        uint8_t uMashValveState = CLOSED;
        uint8_t uBoilValveState = CLOSED;
        uint8_t uInletValveState = CLOSED;
        uint8_t uChillerValveState = CLOSED;
        unsigned int uiDecimalPlaces = 3;
        float fHLTTemp = 0, fMashTemp = 0;
        float fNumber = 54.3211;
        //TEMPLATE
//        printf("***********whole = %d.%d \n\r", (unsigned int)floor(fNumber), (unsigned int)((fNumber-floor(fNumber))*pow(10, uiDecimalPlaces)));

        static uint8_t hlt_last = 0, mash_last = 0, boil_last = 0, inlet_last = 0, chiller_last = 0;
        for(;;)
          {

            xSemaphoreTake(xValvesAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
            //return to the menu system until its returned

            fHLTTemp = ds1820_get_temp(HLT);
            fMashTemp = ds1820_get_temp(MASH);

            uHLTValveState = GPIO_ReadInputDataBit(HLT_VALVE_PORT, HLT_VALVE_PIN);
            uMashValveState = GPIO_ReadInputDataBit(MASH_VALVE_PORT, MASH_VALVE_PIN);
            uBoilValveState = GPIO_ReadInputDataBit(BOIL_VALVE_PORT, BOIL_VALVE_PIN);
            uInletValveState = GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN);
            uChillerValveState = GPIO_ReadInputDataBit(CHILLER_VALVE_PORT, CHILLER_VALVE_PIN);
            if (uHLTValveState != hlt_last ||
                uBoilValveState != boil_last ||
                uMashValveState != mash_last ||
                uInletValveState != inlet_last ||
                uChillerValveState != chiller_last
            ){
                if (uHLTValveState)
                  {
                    lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Blue);
                    lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Red);
                    lcd_printf(0,4,13,"HLT->MASH");
                  }

                else
                  {
                    lcd_DrawRect(TOGGLE_HLT_VALVE_X1, TOGGLE_HLT_VALVE_Y1, TOGGLE_HLT_VALVE_X2, TOGGLE_HLT_VALVE_Y2, Cyan);
                    lcd_fill(TOGGLE_HLT_VALVE_X1+1, TOGGLE_HLT_VALVE_Y1+1, TOGGLE_HLT_VALVE_W, TOGGLE_HLT_VALVE_H, Green);
                    lcd_printf(0,4,13,"HLT->HLT");
                  }

                if (uMashValveState)
                  {
                    lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Blue);
                    lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Red);
                    lcd_printf(12,4,13, "MASH->BOIL");
                  }

                else
                  {
                    lcd_DrawRect(TOGGLE_MASH_VALVE_X1, TOGGLE_MASH_VALVE_Y1, TOGGLE_MASH_VALVE_X2, TOGGLE_MASH_VALVE_Y2, Cyan);
                    lcd_fill(TOGGLE_MASH_VALVE_X1+1, TOGGLE_MASH_VALVE_Y1+1, TOGGLE_MASH_VALVE_W, TOGGLE_MASH_VALVE_H, Green);
                    lcd_printf(12,4,13, "MASH->MASH");
                  }


                if (uBoilValveState)
                  {
                    lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Blue);
                    lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Red);
                    lcd_printf(24,4,13, "BOIL OPENED");
                  }
                else
                  {
                    lcd_DrawRect(TOGGLE_BOIL_VALVE_X1, TOGGLE_BOIL_VALVE_Y1, TOGGLE_BOIL_VALVE_X2, TOGGLE_BOIL_VALVE_Y2, Cyan);
                    lcd_fill(TOGGLE_BOIL_VALVE_X1+1, TOGGLE_BOIL_VALVE_Y1+1, TOGGLE_BOIL_VALVE_W, TOGGLE_BOIL_VALVE_H, Green);
                    lcd_printf(24,4,13, "BOIL CLOSED");
                  }

                if (uInletValveState)
                  {
                    lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Blue);
                    lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Red);
                    lcd_printf(0,8,13, "INLET OPENED");
                  }

                else
                  {
                    lcd_DrawRect(TOGGLE_INLET_VALVE_X1, TOGGLE_INLET_VALVE_Y1, TOGGLE_INLET_VALVE_X2, TOGGLE_INLET_VALVE_Y2, Cyan);
                    lcd_fill(TOGGLE_INLET_VALVE_X1+1, TOGGLE_INLET_VALVE_Y1+1, TOGGLE_INLET_VALVE_W, TOGGLE_INLET_VALVE_H, Green);
                    lcd_printf(0,8,13, "INLET CLOSED");
                  }

                if (uChillerValveState)
                  {
                    lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Blue);
                    lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Red);
                    lcd_printf(0,8,13, "CHILLER OPENED");
                  }

                else
                  {
                    lcd_DrawRect(TOGGLE_CHILLER_VALVE_X1, TOGGLE_CHILLER_VALVE_Y1, TOGGLE_CHILLER_VALVE_X2, TOGGLE_CHILLER_VALVE_Y2, Cyan);
                    lcd_fill(TOGGLE_CHILLER_VALVE_X1+1, TOGGLE_CHILLER_VALVE_Y1+1, TOGGLE_CHILLER_VALVE_W, TOGGLE_CHILLER_VALVE_H, Green);
                    lcd_printf(0,8,13, "CHILLER CLOSED");
                  }

                hlt_last = uHLTValveState;
                boil_last = uBoilValveState;
                mash_last = uMashValveState;
                inlet_last = uInletValveState;
                chiller_last = uChillerValveState;
            }

            if(tog)
              {
                lcd_fill(1,190, 180,39, Black);

                lcd_printf(1, 12, 25, "HLT = %d.%ddegC", (unsigned int)floor(fHLTTemp), (unsigned int)((fHLTTemp-floor(fHLTTemp))*pow(10, 3)));
                lcd_printf(1, 13, 25, "MASH = %d.%ddegC", (unsigned int)floor(fMashTemp), (unsigned int)((fMashTemp-floor(fMashTemp))*pow(10, 3)));
                lcd_printf(1, 14, 25, "Currently @ %d.%d ml", (unsigned int)floor(fGetBoilFlowLitres()), (unsigned int)((fGetBoilFlowLitres()-floor(fGetBoilFlowLitres()))*pow(10, 3)));

              }
            else{
                lcd_fill(1,210, 180,17, Black);
            }

            tog = tog ^ 1;

            xSemaphoreGive(xValvesAppletRunningSemaphore); //give back the semaphore as its safe to return now.
            vTaskDelay(500);

          }
*/
}
#define MAIN 0
#define GRAPH 1
int iGraphKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;

  if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
     {
      vConsolePrint("Leaving Graph Applet\r\n");
      return 1;
     }
  return 0;
}

int iQuitKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  lcd_DrawRect(Q_X1, Q_Y1, Q_X2, Q_Y2, Yellow);
  lcd_fill(Q_X1+1, Q_Y1+1, Q_W, Q_H, Red);
  vConsolePrint("Press back again to exit BREW\r\n");

  if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
     {
      vConsolePrint("Leaving BREW Applet\r\n");
      return 2;
     }
  else if (xx > Q_X1 && yy > Q_Y1 && xx < Q_X2 && yy < Q_Y2)
       {
        vConsolePrint("Staying BREW Applet\r\n");
        return 1;
       }
      return 0;
}


int iBrewKey(int xx, int yy)
{
  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  int iKeyFromApplet = 0;

  switch (iAppletState)
  {
  case GRAPH:
    {
      iKeyFromApplet = iGraphKey(xx,yy);
      if (iKeyFromApplet == 1)
        iAppletState = MAIN;

      break;

    }
  case QUIT:
    {
      iKeyFromApplet = iQuitKey(xx,yy);
            if (iKeyFromApplet == 2)
              {
                //try to take the semaphore from the display applet. wait here if we cant take it.
                          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
                          //delete the display applet task if its been created.
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
            else if (iKeyFromApplet == 1)
              iAppletState = MAIN;

      break;

    }
  case OTHER:
    {
      break;

    }
  case MAIN:
    {

      if (xx > BUTTON_1_X1+5 && xx < BUTTON_1_X2-5 && yy > BUTTON_1_Y1+5 && yy < BUTTON_1_Y2-5)
        {
          vConsolePrint("Button1 Pressed\r\n");
          iAppletState = GRAPH;
        }
      else if (xx > BUTTON_2_X1+5 && xx < BUTTON_2_X2-5 && yy > BUTTON_2_Y1+5 && yy < BUTTON_2_Y2-5)
        {
          vConsolePrint("Button2 Pressed\r\n");
        }
      if (xx > BUTTON_3_X1+5 && xx < BUTTON_3_X2-5 && yy > BUTTON_3_Y1+5 && yy < BUTTON_3_Y2-5)
        {
          vConsolePrint("Button3 Pressed\r\n");
        }
      else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
        {
          iAppletState = QUIT;
          vConsolePrint("QUIT Pressed\r\n");
        }
      break;
    }
  }


  vTaskDelay(10);
  return 0;

}

//===================================================================================================================================================






static struct BrewStep Brew[] = {
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0},
    {"Waiting",          NULL,                   NULL,                 0}
};

