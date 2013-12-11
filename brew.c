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
#include "chiller_pump.h"
#include "mash_pump.h"
#include "valves.h"
#include "hlt.h"
#include "Flow1.h"
#include "crane.h"
#include "stir.h"
#include "mill.h"
#include "parameters.h"
#include "message.h"
#include "io_util.h"
#include "boil_valve.h"
#include "boil.h"
#include "hop_dropper.h"

#define MAX_BREW_STEPS iMaxBrewSteps()

// Applet States
#define MAIN 0
#define GRAPH 1
#define STATS 2
#define RES  3
#define QUIT 4
#define OTHER 5

// Brew Step Codes
#define BREW_STEP_BASE 40
#define BREW_STEP_COMPLETE 40
#define BREW_STEP_FAILED 41
#define BREW_STEP_WAIT 42

// Running States
#define IDLE 0
#define RUNNING 1

struct State BrewState = {0,0,0,0,0,0,0,0,0,0,0,0,0};

const int STEP_COMPLETE = 40;
const int STEP_FAILED = 41;
const int STEP_WAIT = 42;

const char  * pcRunningState[2] = {"IDLE", "RUNNING"};
const char  * pcHLTLevels[3] = {"LOW", "MID", "HIGH"};
const char  * pcHLTStates[4] = {"IDLE", "FILL+HEAT", "DRAIN", "AT TEMP"};
const char  * pcCraneStates[6] = {"Top", "Bottom", "Driving up", "Driving Down", "Down Inc", "Stopped"};
const char  * pcTASKS[6] = {"Brew", "Boil", "HLT", "Crane", "BoilValve", "HopDropper"};
const char  * pcBrewMessages[3] = {"Step Complete", "Step Failed", "Wait for previous steps"};



#define CLEAR_APPLET_CANVAS lcd_fill(CANVAS_X1+1,CANVAS_Y1+1, CANVAS_W, CANVAS_H, Black);
#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%0d.%0d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));

#define DRAW_RESUME_BUTTONS lcd_DrawRect(BUTTON_4_X1, BUTTON_4_Y1, BUTTON_4_X2, BUTTON_4_Y2, Red); \
    lcd_fill(BUTTON_4_X1+1, BUTTON_4_Y1+1, BUTTON_4_W, BUTTON_4_H, Blue); \
    lcd_DrawRect(BUTTON_5_X1, BUTTON_5_Y1, BUTTON_5_X2, BUTTON_5_Y2, Red); \
    lcd_fill(BUTTON_5_X1+1, BUTTON_5_Y1+1, BUTTON_5_W, BUTTON_5_H, Blue); \
    lcd_DrawRect(BUTTON_6_X1, BUTTON_6_Y1, BUTTON_6_X2, BUTTON_6_Y2, Red); \
    lcd_fill(BUTTON_6_X1+1, BUTTON_6_Y1+1, BUTTON_6_W, BUTTON_6_H, Blue);

#define DRAW_RESUME_BUTTON_TEXT  lcd_printf(1,13,5, "UP"); \
    lcd_printf(9,13,5, "DOWN"); \
    lcd_printf(17,13,5, "RESUME"); //Button6

#define DRAW_MAIN_BUTTON_TEXT  lcd_printf(1,13,5, "START"); \
    lcd_printf(9,13,5, "PAUSE"); \
    lcd_printf(17,13,5, "SKIP"); //Button6




void vBrewAppletDisplay( void *pvParameters);
void vBrewGraphAppletDisplay(void * pvParameters);
void vBrewStatsAppletDisplay(void * pvParameters);
void vBrewResAppletDisplay(void * pvParameters);
void vBrewReset(void);
void vBrewApplet(int init);
void vBrewRunStep(void);
void vBrewNextStep(void);

xTaskHandle xBrewTaskHandle = NULL,
    xBrewAppletDisplayHandle = NULL,
    xBrewGraphAppletDisplayHandle = NULL,
    xBrewStatsAppletDisplayHandle = NULL,
    xBrewResAppletDisplayHandle = NULL,
    xBrewHLTTaskHandle = NULL;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xBrewAppletRunningSemaphore;
xQueueHandle xBrewTaskStateQueue = NULL, xBrewTaskReceiveQueue = NULL;
xQueueHandle xBrewAppletTextQueue = NULL;

static int iAppletState = MAIN;
static unsigned char ucResStep = 0;

// Define Structure for brew steps
static struct BrewStep
{
  const char * pcStepName;
  int (*func)(void * pvParameters);
  void (*poll)(void * pvParameters);
  int iFuncParams[5];
  uint16_t uTimeout;
  uint32_t uStartTime;
  uint16_t uElapsedTime;
  unsigned char ucComplete; // step complete flag
  unsigned char ucWait; // Wait for previous steps to be complete?
} Brew[]; //Brew is an array of brew steps


static int iBrewStepMonitor(int iWaiting)
{
  static char buf[50];
  int ii=0, jj=0, kk=0;
  int iPreviousStepsComplete;

  //vConsolePrint("STEP MONITOR\r\n");
  for (jj = 0;jj <= BrewState.ucStep;jj++)
    {
      //sprintf(buf,"checking step %d : state = %d\r\n",jj,Brew[jj].ucComplete);
      //vConsolePrint(buf);
      if (Brew[jj].ucComplete == 0)
        {

          kk++;
          Brew[jj].uElapsedTime = (BrewState.uSecondCounter - Brew[jj].uStartTime);

          if (jj != BrewState.ucStep)
            {
              sprintf(buf, "Waiting on Step %d:'%s' \r\n", jj, Brew[jj].pcStepName);
              vConsolePrint(buf);
            }

          if (Brew[jj].uElapsedTime > Brew[jj].uTimeout)
            {
              sprintf(buf, "Step %d: %s - TIMEOUT\r\n", jj, Brew[jj].pcStepName);
              vConsolePrint(buf);
            }
        }

    }
  iPreviousStepsComplete = kk;

  if (iWaiting)
    {
      //  sprintf(buf, "Returning %d\r\n=====================\r\n", iPreviousStepsComplete);
      //   vConsolePrint(buf);
      return iPreviousStepsComplete;
    }
  else
    // vConsolePrint("Returning 0\r\n");
    return 0;

}

static int iMaxBrewSteps(void)
{
  static char buf[50];
  int i = 0;
  while (Brew[i].pcStepName != NULL)
    {
      i++;
    }
  //sprintf(buf, "Sizeof Brew[] = %d\r\n", i);
  //vConsolePrint(buf);
  return i;
}



void vBrewTotalReset(void){

  int iMax = iMaxBrewSteps();
  vBrewReset();
  vTaskDelete(xBrewTaskHandle);
  vQueueDelete(xBrewTaskReceiveQueue);
  vQueueDelete(xBrewTaskStateQueue);
  xBrewTaskHandle = NULL;
  xBrewTaskReceiveQueue = NULL;
  xBrewTaskStateQueue = NULL;
  for (int i = 0; i < iMax; i++)
    {
      Brew[i].ucComplete = 0;
      Brew[i].uElapsedTime = 0;
      Brew[i].uStartTime = 0;
    }

}

//----------------------------------------------------------------------------------------------------------------------------
// Brew Task
//----------------------------------------------------------------------------------------------------------------------------
void vTaskBrew(void * pvParameters)
{
  portTickType xBrewStart;
  static uint16_t uBrewSecondsElapsed = 0, uBrewMinutesElapsed = 0, uBrewHoursElapsed=0;
  xBrewStart = xTaskGetTickCount();

  char pcBrewElapsedTime[9], pcStepElapsedTime[16];
  uint32_t uStartingStep = (uint32_t)pvParameters;
  //char buf1[40];
  BrewState.ucStep = 0;
  BrewState.uHoursElapsed = 0;
  BrewState.uMinutesElapsed = 0;
  BrewState.uSecondCounter = 0;
  BrewState.ucHLTState = HLT_STATE_IDLE;
  BrewState.ucHopAddition = 0;
  BrewState.ucRunningState = IDLE;
  BrewState.ucTarget = 0;
  BrewState.ucTargetHLTTemp = 0;
  BrewState.ucTargetHLTVolume = 0;
  BrewState.ucTargetWaterVolume = 0;
  BrewState.xLastWakeTime = xTaskGetTickCount();
  static uint8_t uMsg = IDLE;
  struct GenericMessage * xMessage;
  static int iContent = 0;
  static int iWaiting = 0, iStepsToComplete = 0;

  xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));

  static char buf[50], buf1[50];

  for(;;)
    {
      if(xQueueReceive(xBrewTaskStateQueue, &uMsg, 0) == pdPASS)
        {
          BrewState.ucRunningState = uMsg;
          if (BrewState.ucRunningState == IDLE)
            sprintf(buf, "BrewState changing to IDLE\r\n");
          else
            sprintf(buf, "BrewState changing to RUNNING\r\n");
          vConsolePrint(buf);
          // if (uMsg == RUNNING && BrewState.ucStep == 0)
          //   vBrewRunStep();
        }
      switch(BrewState.ucRunningState)
      {
      case IDLE:
        {
          //vConsolePrint("Brew Idle\r\n");

          break;
        }
      case RUNNING:
        {
          // Every thirty seconds, print the brew time and step time to the console.
          if ((BrewState.uSecondsElapsed % 15) == 0)
            {
              sprintf(pcBrewElapsedTime, "Brew: %02u:%02u:%02u\r\n", BrewState.uHoursElapsed, BrewState.uMinutesElapsed, BrewState.uSecondsElapsed);
              vConsolePrint(pcBrewElapsedTime);
              sprintf(pcStepElapsedTime, "Step:%02um:%02us\r\n", Brew[BrewState.ucStep].uElapsedTime/60, Brew[BrewState.ucStep].uElapsedTime%60);
              vConsolePrint(pcStepElapsedTime);
            }


          // receive message from queue and if there is one, act on it.. (ie if the message is STEP COMPLETED.. then change to the next step.
          if(xQueueReceive(xBrewTaskReceiveQueue, &xMessage, 0) == pdPASS)
            {

              iContent = *(int*)xMessage->pvMessageContent; //casts to int * and then dereferences.
              sprintf(buf, "Brew Task: Msg from %s : %d : %u\r\n", pcTASKS[xMessage->ucFromTask- TASK_BASE], *(int *)xMessage->pvMessageContent, xMessage->uiStepNumber);
              vConsolePrint(buf);

              sprintf(buf1, "Brew Task: iContent = %s\r\n", pcBrewMessages[iContent-BREW_STEP_BASE]);
              vConsolePrint(buf1);

              switch(iContent)
              {
              case BREW_STEP_COMPLETE:
                {
                  Brew[xMessage->uiStepNumber].ucComplete = 1;
                  sprintf(buf, "Brew Task: Setting Step %d to Complete\r\n", xMessage->uiStepNumber);
                  vConsolePrint(buf);
                  break;
                }
              case BREW_STEP_FAILED:
                {

                  break;
                }
              case BREW_STEP_WAIT:
                {
                  vConsolePrint("WAITING COMMAND RECEIVED\r\n");
                  iWaiting = 1;

                }
              }
            }

          iStepsToComplete = iBrewStepMonitor(iWaiting); // updates elapsed times and monitors timeouts of steps.

          if (iWaiting) // are we waiting on all previous steps to be completed?
            {
              if (iStepsToComplete)// are there any steps to be completed?
                {
                  iWaiting = 1;
                }
              else // if not, we move on...
                {
                  iWaiting = 0;
                  BrewState.ucStep++;
                  vBrewRunStep();
                }
            }

          // If there is a polling function, run it.
          if (Brew[BrewState.ucStep].poll)
            Brew[BrewState.ucStep].poll((void *)Brew[BrewState.ucStep].iFuncParams);

          //size_t heap;
          //heap = xPortGetFreeHeapSize();
          //vConsolePrint("HEAP REMAINING: %d\r\n", heap);
          // Update the counters
          BrewState.uSecondCounter++;
          BrewState.uSecondsElapsed++;
          if (BrewState.uSecondsElapsed == 60)
            {
              BrewState.uSecondsElapsed = 0;
              BrewState.uMinutesElapsed++;
            }

          if (BrewState.uMinutesElapsed == 60)
            {
              BrewState.uMinutesElapsed = 0;
              BrewState.uHoursElapsed++;
            }

          break;
        }
      }

      vTaskDelayUntil(&BrewState.xLastWakeTime, 1000 / portTICK_RATE_MS );

    }

}

//----------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------------
// BREW PAUSE
//----------------------------------------------------------------------------------------------------------------------------
void vBrewReset(void){
  static struct GenericMessage * xMessage = NULL;
  static int iCommand = 0;
  if (xMessage == NULL)
    xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = BOIL_TASK;
  xMessage->pvMessageContent = (void *)&iCommand;
  if (xBoilQueue != NULL)
    xQueueSendToBack(xBoilQueue, &xMessage, 0);
  vChillerPump(STOP);
  vMashPump(STOP);
  vMill(MILL_STOPPED); // need to change state name.
  vValveActuate(HLT_VALVE, CLOSE);
  vValveActuate(INLET_VALVE, CLOSE);
  vValveActuate(MASH_VALVE, CLOSE);
  vValveActuate(CHILLER_VALVE, CLOSE);
}

void vBrewNextStep(void){
  static struct GenericMessage * xMessage = NULL;
  static int iCommand;
  static char buf[50], buf1[50];
  static int iCount = 0;
  iCount++;
  if (xMessage ==  NULL)
    xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = BREW_TASK;
  xMessage->pvMessageContent = (void *)&iCommand;


  sprintf(buf, "Requesting next step: %d, %s\r\n", BrewState.ucStep+1, Brew[BrewState.ucStep+1].pcStepName);
  vConsolePrint(buf);
  if (BrewState.ucStep < MAX_BREW_STEPS && Brew[BrewState.ucStep+1].ucWait == 1)
    {
      iCommand = STEP_WAIT;
      sprintf(buf1, "NextStep:'%s' requires waiting\r\n", Brew[BrewState.ucStep].pcStepName);
      vConsolePrint(buf1);
      xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage, 0);

    }
  else if (BrewState.ucStep < MAX_BREW_STEPS-1){
      BrewState.ucStep++;
  //    sprintf(buf1, "NextStep: %d, %s\r\n", BrewState.ucStep, Brew[BrewState.ucStep].pcStepName);
  //    vConsolePrint(buf1);
      vBrewRunStep();
  }
  else
    {
      vConsolePrint("Brew Finished!\r\n");
      BrewState.ucRunningState = IDLE;
    }
  sprintf(buf1, "NextStep: Count = %d\r\n", iCount);
  vConsolePrint(buf1);


}

void vBrewRunStep(void){
  int iFuncReturnValue = 0;
  static char buf[50];

  //vConsolePrint("vBrewRunStep Called\r\n");

  sprintf(buf, "RunStep: %d -> %s\r\n", BrewState.ucStep, Brew[BrewState.ucStep].pcStepName);
  vConsolePrint(buf);

  vBrewReset();

  Brew[BrewState.ucStep].uElapsedTime = 0;
  Brew[BrewState.ucStep].uStartTime = BrewState.uSecondCounter;
  CLEAR_APPLET_CANVAS;

  if (Brew[BrewState.ucStep].func)
    Brew[BrewState.ucStep].func((void *)Brew[BrewState.ucStep].iFuncParams);

}


#define STRIKE 0
#define MASH_OUT 1
#define SPARGE 2
#define NO_MASH_OUT 3
#define SPARE 4

//===================================================================================================================================================
void vBrewHLTSetupFunction(int piParameters[5]){
  static struct HLTMsg  * Content =  NULL;
  struct GenericMessage * Message = NULL;

  if (Content == NULL)
    Content = (struct HLTMsg *)malloc(sizeof(struct HLTMsg));
  if (Message == NULL)
    Message = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));

  switch( piParameters[0] )
  {
  case HLT_STATE_FILL_HEAT:
    {
      Content->uState = HLT_STATE_FILL_HEAT;
      switch( piParameters[1] )
      {
      case STRIKE:
        {
          Content->pcMsgText = "Strike";
          Content->uData1 = BrewParameters.fStrikeTemp;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);
          break;
        }
      case MASH_OUT:
        {
          Content->pcMsgText = "Mash Out";
          Content->uData1 = BrewParameters.fMashOutTemp;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);
          break;
        }
      case SPARGE:
        {
          Content->pcMsgText = "Sparge";
          Content->uData1 = BrewParameters.fSpargeTemp;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);

          break;
        }
      case NO_MASH_OUT:
        {

          break;
        }
      case SPARE:
        {

          break;
        }

      }
      break;
    }
  case HLT_STATE_DRAIN:
    {
      Content->uState = HLT_STATE_DRAIN;
      switch( piParameters[1] )
      {
      case STRIKE:
        {
          Content->pcMsgText = "Drain For Strike";
          Content->uData1 = BrewParameters.fStrikeLitres;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);
          break;
        }
      case MASH_OUT:
        {
          Content->pcMsgText = "Drain For Mash Out";
          Content->uData1 = BrewParameters.fMashOutLitres;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);
          break;
        }
      case SPARGE:
        {
          Content->pcMsgText = "Drain For Sparge";
          Content->uData1 = BrewParameters.fSpargeLitres;
          //xQueueSendToBack(xBrewTaskHLTQueue, &Content, portMAX_DELAY);
          break;
        }
      case NO_MASH_OUT:
        {

          break;
        }
      case SPARE:
        {

          break;
        }

      }
      break;

    }
  }
  Message->ucFromTask = BREW_TASK;
  Message->ucToTask = HLT_TASK;
  Message->uiStepNumber = BrewState.ucStep;
  Message->pvMessageContent = Content;
  xQueueSendToBack(xBrewTaskHLTQueue, &Message, portMAX_DELAY);

  //xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
  vBrewNextStep();
}


void vBrewMillSetupFunction (int piParameters[5])
{

  vMill(DRIVING);
  vConsolePrint("Grain Mill Started\r\n");
  //xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
}

void vBrewValvesSetupFunction (int piParameters[5])
{

  vValveActuate(HLT_VALVE, CLOSE);
  vValveActuate(MASH_VALVE, CLOSE);

  vValveActuate(INLET_VALVE, CLOSE);
  vConsolePrint("Valves Setup Func Called\r\n");
  vTaskDelay(50);
  //xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
  Brew[BrewState.ucStep].ucComplete = 1;
  vBrewNextStep();
}

void vBrewBoilValveSetupFunction (int piParameters[5])
{
  static struct GenericMessage * xMessage = NULL;
  static int * iCommand;
  iCommand = &piParameters[0];
  if (xMessage == NULL)
    xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = BOIL_VALVE_TASK;
  xMessage->uiStepNumber = BrewState.ucStep;
  xMessage->pvMessageContent = (void *)iCommand;
  vConsolePrint("Boil Valve Setup Function called\r\n");
  xQueueSendToBack(xBoilValveQueue, &xMessage, 0);
  vBrewNextStep();
}

void vBrewCraneSetupFunction(int piParameters[5])
{
  static struct GenericMessage * xMessage = NULL;
  static int * iCommand;
  iCommand = &piParameters[0];
  if (xMessage == NULL)
    xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = CRANE_TASK;
  xMessage->uiStepNumber = BrewState.ucStep;
  xMessage->pvMessageContent = (void *)iCommand;
  // piParameters[0] sets the state to send to the crane task.
  // check to see if we are driving down.. if so , stir
  //if (piParameters[0] == DN_INC)
  //  vStir(DRIVING);

  // start the crane.
  int i = xQueueSendToBack(xCraneQueue, &xMessage, 0);

  vBrewNextStep();
}

void vBrewChillSetupFunction(int piParameters[5])
{
  static struct GenericMessage * xMessage = NULL;
  const int iCommand = OPEN;
  if (xMessage ==  NULL)
    xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = BOIL_VALVE_TASK;
  xMessage->uiStepNumber = BrewState.ucStep;
  xMessage->pvMessageContent = (void *)&iCommand;
  vConsolePrint("Boil Valve Setup Function called\r\n");
  xQueueSendToBack(xBoilValveQueue, &xMessage, 1000);
  vTaskDelay(50);
  vChillerPump(PUMPING);
  vValveActuate(CHILLER_VALVE, OPEN);

}

void vBrewChillPollFunction(int piParameters[5])
{
  if (Brew[BrewState.ucStep].uElapsedTime >= piParameters[0]*60)
    {
      vChillerPump(STOPPED);
      vValveActuate(CHILLER_VALVE, CLOSE);
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();

    }

}
//uint8_t uNextStep = MSG_STEP_COMPLETE;

void vBrewMillPollFunction (int piParameters[5])
{
  int iMillTime = piParameters[0] * 60;
  if (Brew[BrewState.ucStep].uElapsedTime >= iMillTime)
    {
      vMill(STOPPED);
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
      //xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
    }


}
void vBrewWaitingPollFunction(int piParameters[5])
{
  if (Brew[BrewState.ucStep].uElapsedTime >= piParameters[0])
    {
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
    }

  // else
  //   vConsolePrint("Waiting: Poll Function\r\n");

}
#define MASH_STAGE_1 1
#define MASH_STAGE_2 2
#define MASH_STAGE_3 3
#define MASH_STAGE_4 4

static int iStirState = MASH_STAGE_1;
static int iPumpState = MASH_STAGE_1;

  void vBrewMashSetupFunction(int piParameters[5])
  {
    iStirState = MASH_STAGE_1;
    iPumpState = MASH_STAGE_1;

    int iCycles = 2;
    int ii;
    for (ii = 0; ii < iCycles; ii++)
      {
        vValveActuate(MASH_VALVE, OPEN);
        vTaskDelay(2000);
        vValveActuate(MASH_VALVE, CLOSED);
        vTaskDelay(2000);
      }
  }

void vBrewMashPollFunction(int piParameters[5])
{
  int iMashTime = piParameters[0] * 60;
  int iStirTime1 = piParameters[1] * 60;
  int iStirTime2 = piParameters[2] * 60;
  int iPumpTime1 = piParameters[3] * 60;
  int iPumpTime2 = piParameters[4] * 60;
  int iTimeRemaining = iMashTime;

  static char buf[50];
  iTimeRemaining = iMashTime - Brew[BrewState.ucStep].uElapsedTime;

  //sprintf(buf, "Time Remaining: %d, Elapsed: %d, p1: %d, p2: %d\r\n", iTimeRemaining, Brew[BrewState.ucStep].uElapsedTime, iPumpTime1, iPumpTime2);
  //vConsolePrint(buf);

  switch (iPumpState)
  {
  case MASH_STAGE_1:
    {
      if ((Brew[BrewState.ucStep].uElapsedTime < iPumpTime1))
          {
            vMashPump(PUMPING);
            iPumpState = MASH_STAGE_2;
            vConsolePrint("MashPoll1: Pumping\r\n");
          }
      if (iPumpTime1 == 0)
          {
            vMashPump(STOPPED);
            iPumpState = MASH_STAGE_2;
            vConsolePrint("MashPoll1: Pump Time 1 = 0, not pumping\r\n");
          }
      break;
    }
  case MASH_STAGE_2:
    {
      if ((Brew[BrewState.ucStep].uElapsedTime > iPumpTime1))
        {
          vMashPump(STOPPED);
          iPumpState = MASH_STAGE_3;
          vConsolePrint("MashPoll2: Stopped\r\n");
        }
      break;
    }
  case MASH_STAGE_3:
      {
        if (iTimeRemaining < iPumpTime2)
          {
            vMashPump(PUMPING);
            iPumpState = MASH_STAGE_4;
            vConsolePrint("MashPoll3: Pumping\r\n");
          }
        if (iPumpTime2 == 0)
          {
            vMashPump(STOPPED);
            iPumpState = MASH_STAGE_4;
            vConsolePrint("MashPoll3: Pump Time 2 = 0, not pumping\r\n");
          }
        break;

      }
  case MASH_STAGE_4:
    {
      break;
    }
  }

  switch (iStirState)
  {
  case MASH_STAGE_1:
    {
      if ((Brew[BrewState.ucStep].uElapsedTime < iStirTime1))
          {
            vStir(DRIVING);
            iStirState = MASH_STAGE_2;
            vConsolePrint("MashPoll1: Stirring\r\n");
          }
      if (iStirTime1 == 0)
          {
            vStir(STOPPED);
            iStirState = MASH_STAGE_2;
            vConsolePrint("MashPoll1: Stir Time 1 = 0, not Stirring\r\n");
          }
      break;
    }
  case MASH_STAGE_2:
    {
      if ((Brew[BrewState.ucStep].uElapsedTime > iStirTime1))
        {
          vStir(STOPPED);
          iStirState = MASH_STAGE_3;
          vConsolePrint("MashPoll2: Stopped Stirring\r\n");
        }
      break;
    }
  case MASH_STAGE_3:
      {
        if (iTimeRemaining < iStirTime2)
          {
            vStir(DRIVING);
            iStirState = MASH_STAGE_4;
            vConsolePrint("MashPoll3: Stirring\r\n");
          }
        if (iStirTime2 == 0)
          {
            vStir(STOPPED);
            iStirState = MASH_STAGE_4;
            vConsolePrint("MashPoll3: Stir Time 2 = 0, not Stirring\r\n");
          }
        break;

      }
  case MASH_STAGE_4:
    {
      break;
    }
  }

  if (Brew[BrewState.ucStep].uElapsedTime >= iMashTime)
    {
      vMashPump(STOPPED);
      vStir(STOPPED);
      iStirState = MASH_STAGE_1;
      iPumpState = MASH_STAGE_1;
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
    }


}


void vBrewPumpToBoilSetupFunction(int piParameters[5])
{
  int iIterations = piParameters[0], ii;
  vValveActuate(MASH_VALVE, OPEN);
  for (ii = 0; ii < iIterations; ii++)
    // try to prime, if not already primed
    {
      vMashPump(PUMPING);
      vTaskDelay(2000);
      vMashPump(STOPPED);
      vTaskDelay(2000);
    }

}

void vBrewPumpToBoilPollFunction(int  piParameters[5])
{
  int iPumpToBoilTime = piParameters[1];
  vMashPump(PUMPING);
  vValveActuate(MASH_VALVE, OPEN);
  if (Brew[BrewState.ucStep].uElapsedTime >= iPumpToBoilTime)
    {
      Brew[BrewState.ucStep].ucComplete = 1;
      vMashPump(STOPPED);
      vValveActuate(MASH_VALVE, CLOSE);
      vBrewNextStep();
    }

}
#define BOIL_0 0
#define BOIL_1 1
#define BOIL_2 2
#define BOIL_3 3
#define BOIL_4 4
#define BOIL_5 5
#define BOIL_6 6
#define BOIL_7 7
static int iBoilState = BOIL_0;
static struct GenericMessage * xBoilMessage = NULL;
static struct TextMsg * xBoilTextMessage = NULL;
void vBrewBoilSetupFunction(int piParameters[5])
{
  const int i100 = 100;
  if (xBoilMessage == NULL)
    xBoilMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));
  if (xBoilTextMessage == NULL)
    xBoilTextMessage = (struct TextMsg *)malloc(sizeof(struct TextMsg));


  if (piParameters[2] == 1)
    {
      iBoilState = BOIL_0;
      xBoilMessage->pvMessageContent = (void *)&piParameters[1];
      xBoilTextMessage->pcMsgText = "BOIL";
      xBoilTextMessage->ucLine = 5;
      xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
    }
  else
    {
      xBoilMessage->pvMessageContent = (void *)&i100;
      xBoilTextMessage->pcMsgText = "Bring To Boil";
           xBoilTextMessage->ucLine = 5;
           xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
      iBoilState = BOIL_7;
    }


  xBoilMessage->ucFromTask = BREW_TASK;
  xBoilMessage->ucToTask = BOIL_TASK;
  xBoilMessage->uiStepNumber = BrewState.ucStep;
  vConsolePrint("Boil Setup Function called\r\n");
  xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);

}

void vBrewBoilPollFunction(int piParameters[5])
{

  int iBoilTime = BrewParameters.uiBoilTime*60;
  int iBringToBoilTime = BrewParameters.uiBringToBoilTime*60;
  int iBoilDuty = piParameters[1];
  int iTimeRemaining = ((iBoilTime - Brew[BrewState.ucStep].uElapsedTime)/60);
  int iBringTimeRemaining= ((iBringToBoilTime - Brew[BrewState.ucStep].uElapsedTime)/60);
  static int iLastTime;
  //  xBoilMessage->pvMessageContent = (void *)&iBoilDuty;
  static char buf[50];
  if (iTimeRemaining != iLastTime)
    {
      iLastTime = iTimeRemaining;
    }
  switch (iBoilState)
  {
  case BOIL_0:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[0])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 0\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 0 DELIVERED";
          xBoilTextMessage->ucLine = 6;
          xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
          iBoilState = BOIL_1;
        }
      break;
    }
  case BOIL_1:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[1])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 1\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 1 DELIVERED";
          xBoilTextMessage->ucLine = 6;
          xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
          iBoilState = BOIL_2;
        }
      break;
    }
  case BOIL_2:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[2])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 2\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 2 DELIVERED";
                   xBoilTextMessage->ucLine = 6;
                   xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);

          iBoilState = BOIL_3;
        }
      break;
    }
  case BOIL_3:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[3])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 3\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 3 DELIVERED";
                   xBoilTextMessage->ucLine = 6;
                   xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);

          iBoilState = BOIL_4;
        }
      break;
    }
  case BOIL_4:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[4])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 4\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 4 DELIVERED";
                   xBoilTextMessage->ucLine = 6;
                   xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);

          iBoilState = BOIL_5;
        }
      break;
    }
  case BOIL_5:
    {
      if (iTimeRemaining <   BrewParameters.uiHopTimes[5])
        {
          xQueueSendToBack(xHopsQueue, (void *)1,0);
          vConsolePrint("Hops Delivered 5\r\n");
          xBoilTextMessage->pcMsgText = "HOPS 5 DELIVERED";
                   xBoilTextMessage->ucLine = 6;
                   xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);

          iBoilState = BOIL_6;
        }
      break;
    }
  case BOIL_6:
    {
      if (Brew[BrewState.ucStep].uElapsedTime >= iBoilTime)
        {
          iBoilDuty = 0;
          Brew[BrewState.ucStep].ucComplete = 1;
          xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);
          vBrewNextStep();
        }
      break;
    }
  case BOIL_7:
    {
      if (Brew[BrewState.ucStep].uElapsedTime >= iBringToBoilTime)
        {
          iBoilDuty = 0;
          Brew[BrewState.ucStep].ucComplete = 1;
          xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);
          vBrewNextStep();
        }
      break;
    }

  default:
    {
      iBoilState = 100;
      break;
      // hang here;
    }
  }// switch

}

//----------------------------------------------------------------------------------------------------------------------------
// BREW APPLET - Called from menu
//----------------------------------------------------------------------------------------------------------------------------
void vBrewApplet(int init){
  size_t heap;
  if (init)
    {
      heap = xPortGetFreeHeapSize();
      vConsolePrint("HEAP REMAINING: %d\r\n", heap);
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
      lcd_printf(17,13,5, "SKIP"); //Button6


      vConsolePrint("Brew Applet Opened\r\n");
      vSemaphoreCreateBinary(xBrewAppletRunningSemaphore);
      if (xBrewTaskStateQueue == NULL && xBrewTaskReceiveQueue == NULL )
        {
          xBrewTaskStateQueue = xQueueCreate(1, sizeof(uint8_t));
          xBrewTaskReceiveQueue = xQueueCreate(5, sizeof(struct GenericMessage *));
          if (xBrewTaskStateQueue == NULL || xBrewTaskReceiveQueue == NULL )
            vConsolePrint("FATAL Error creating Brew Task Queues\r\n");
          else vConsolePrint("Created Brew Task Queues\r\n");
        }
      //      if (xBrewTaskHLTQueue == NULL)
      //            {
      //              xBrewTaskHLTQueue = xQueueCreate(5,sizeof(struct GenericMessage *));
      //              if (xBrewTaskHLTQueue == NULL)
      //                vConsolePrint("FATAL Error creating  Brew HLT Task Queue\r\n");
      //              else vConsolePrint("Created Brew HLT Task Queues\r\n");
      //            }

      if (xBrewAppletTextQueue == NULL)
        {
          xBrewAppletTextQueue = xQueueCreate(5,sizeof(struct TextMsg *));
          if (xBrewAppletTextQueue == NULL)
            vConsolePrint("FATAL Error creating Applet Text Queue\r\n");
          else vConsolePrint("Created Applet Text Queue\r\n");
        }

      if (xBrewTaskHandle == NULL)
        xTaskCreate( vTaskBrew,
            ( signed portCHAR * ) "BrewRun",
            configMINIMAL_STACK_SIZE + 800,
            NULL,
            tskIDLE_PRIORITY+2,
            &xBrewTaskHandle );
      vConsolePrint("Brew Task Created\r\n");

      if (xBrewHLTTaskHandle == NULL)
        xTaskCreate( vTaskBrewHLT,
            ( signed portCHAR * ) "BrewHLT",
            configMINIMAL_STACK_SIZE + 200,
            NULL,
            tskIDLE_PRIORITY,
            &xBrewHLTTaskHandle );
      vConsolePrint("Brew HLT Task Created\r\n");

      xTaskCreate( vBrewAppletDisplay,
          ( signed portCHAR * ) "BrewDisp",
          configMINIMAL_STACK_SIZE + 200,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewAppletDisplayHandle );

      if (xBrewGraphAppletDisplayHandle == NULL)
        {
          xTaskCreate( vBrewGraphAppletDisplay,
              ( signed portCHAR * ) "BrewGraph",
              configMINIMAL_STACK_SIZE,
              NULL,
              tskIDLE_PRIORITY,
              &xBrewGraphAppletDisplayHandle );
          vConsolePrint("Graph Applet Task Created\r\n");
        }
      vTaskSuspend(xBrewGraphAppletDisplayHandle);

      if (xBrewStatsAppletDisplayHandle == NULL)
        {
          xTaskCreate( vBrewStatsAppletDisplay,
              ( signed portCHAR * ) "BrewStats",
              configMINIMAL_STACK_SIZE,
              NULL,
              tskIDLE_PRIORITY,
              &xBrewStatsAppletDisplayHandle );
          vConsolePrint("Stats Applet Task Created\r\n");
        }

      vTaskSuspend(xBrewStatsAppletDisplayHandle);
      if (xBrewResAppletDisplayHandle == NULL)
        {
          xTaskCreate( vBrewResAppletDisplay,
              ( signed portCHAR * ) "BrewRes",
              configMINIMAL_STACK_SIZE,
              NULL,
              tskIDLE_PRIORITY,
              &xBrewResAppletDisplayHandle );
          vConsolePrint("Resume Applet Task Created\r\n");
        }

      vTaskSuspend(xBrewResAppletDisplayHandle);

      vTaskDelay(200);
    }
  else vConsolePrint("Leaving Brew Applet\r\n");

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
      if (ucLastStep != ucResStep)
        {
          CLEAR_APPLET_CANVAS;
          if (ucResStep > 0)
            lcd_printf(1,4,10, "  STEP %d: %s", ucResStep-1, Brew[ucResStep-1].pcStepName);
          lcd_printf(1,5,10, "->STEP %d: %s", ucResStep, Brew[ucResStep].pcStepName);
          if (ucResStep < MAX_BREW_STEPS)
            lcd_printf(1,6,10, "  STEP %d: %s", ucResStep+1, Brew[ucResStep+1].pcStepName);
          vConsolePrint("Resume Display Applet Running\r\n");

          ucLastStep = ucResStep;
        }
      vTaskDelay(200);
    }


}

//{
//      if (ucLastStep != BrewState.ucStep)
//        {
//          CLEAR_APPLET_CANVAS;
//          if (BrewState.ucStep > 0)
//            lcd_printf(1,4,10, "  STEP %d: %s", BrewState.ucStep-1, Brew[BrewState.ucStep-1].pcStepName);
//          lcd_printf(1,5,10, "->STEP %d: %s", BrewState.ucStep, Brew[BrewState.ucStep].pcStepName);
//          if (BrewState.ucStep < MAX_BREW_STEPS)
//            lcd_printf(1,6,10, "  STEP %d: %s", BrewState.ucStep+1, Brew[BrewState.ucStep+1].pcStepName);
//          vConsolePrint("Resume Display Applet Running\r\n");
//
//          ucLastStep = BrewState.ucStep;
//        }
//      vTaskDelay(200);
//    }



void vBrewAppletDisplay( void *pvParameters){

  static char tog = 0; //toggles each loop
  //unsigned int uiDecimalPlaces = 3;
  float fHLTTemp = 0, fMashTemp = 0, fAmbientTemp = 0, fCabinetTemp;
  float fHLTTempLast = 1, fMashTempLast = 1, fAmbientTempLast = 1, fCabinetTempLast;
  static char buf[8][40];
  static char b[40], a[40];
  struct TextMsg * RcvdTextMsg;
  RcvdTextMsg = (struct TextMsg *)malloc(sizeof(struct TextMsg));
  unsigned char ucHLTLevel;
  static unsigned char ucLastStep = 255;
  static unsigned char ucLastState = 255;
  static unsigned char ucLastHLTLevel = 255;
  lcd_printf(0,10,40, "|HLT   |MASH  |CAB   |AMB   |");
  lcd_printf(0,11,40, "|      |      |      |      |");
  ucLastStep = BrewState.ucStep+1;
  ucLastState = BrewState.ucRunningState-1;

  for(;;)
    {

      xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned
      if (xQueueReceive(xBrewAppletTextQueue, &RcvdTextMsg, 0) == pdPASS)
        {
          sprintf(buf[RcvdTextMsg->ucLine], RcvdTextMsg->pcMsgText);
        }
      fHLTTemp = ds1820_get_temp(HLT);
      fMashTemp = ds1820_get_temp(MASH);
      fAmbientTemp = ds1820_get_temp(AMBIENT);
      fCabinetTemp = ds1820_get_temp(CABINET);

      if (!GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN))
        {
          ucHLTLevel = HLT_LEVEL_MID;
          if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN))
            {
              ucHLTLevel = HLT_LEVEL_HIGH;
            }

        }
      else {
          ucHLTLevel = HLT_LEVEL_LOW;
      }



      if (ucLastStep != BrewState.ucStep && Brew[BrewState.ucStep].pcStepName != NULL)
        {
          sprintf(buf[0], "STEP %d: %s", BrewState.ucStep, Brew[BrewState.ucStep].pcStepName);
          ucLastStep = BrewState.ucStep;
        }
      if (ucLastState != BrewState.ucRunningState)
        {
          sprintf(buf[1], "STATE = %s", pcRunningState[BrewState.ucRunningState]);
          ucLastState = BrewState.ucRunningState;
        }
      if (ucLastHLTLevel != ucHLTLevel)
        {
          sprintf(buf[3], "HLT Level = %s", pcHLTLevels[ucHLTLevel]);
          ucLastHLTLevel = ucHLTLevel;
        }
      sprintf(buf[2], "Elapsed Time: %02d:%02d", Brew[BrewState.ucStep].uElapsedTime/60, Brew[BrewState.ucStep].uElapsedTime%60);


      //fHLTTemp = fHLTTemp + 0.1;


      if(tog)
        {
          CLEAR_APPLET_CANVAS;

          //lcd_printf(1, 8, 25, "HLT = %d.%ddegC", (unsigned int)floor(fHLTTemp), (unsigned int)((fHLTTemp-floor(fHLTTemp))*pow(10, 1)));
          //lcd_printf(1, 9, 25, "MASH = %d.%ddegC", (unsigned int)floor(fMashTemp), (unsigned int)((fMashTemp-floor(fMashTemp))*pow(10, 1)));
          //lcd_printf(1, 10, 25, "Cabinet = %d.%ddegC", (unsigned int)floor(fCabinetTemp), (unsigned int)((fCabinetTemp-floor(fCabinetTemp))*pow(10, 1)));
          //lcd_printf(1, 11, 25, "Ambient = %d.%ddegC", (unsigned int)floor(fAmbientTemp), (unsigned int)((fAmbientTemp-floor(fAmbientTemp))*pow(10, 1)));
          //lcd_printf(1,10,40, "|HLT   |MASH  |CAB   |AMB   |");
          //lcd_printf(1,11,40, "|      |      |      |      |");
          if((fHLTTempLast != fHLTTemp) ||(fMashTempLast != fMashTemp) || (fCabinetTempLast != fCabinetTemp) || (fAmbientTempLast != fAmbientTemp) )
            {
              lcd_fill(0,159, 254, 32, Black);
              lcd_printf(0,10,40, "|HLT   |MASH  |CAB   |AMB   |");
              lcd_printf(0,11,40, "|      |      |      |      |");
              LCD_FLOAT(1,11, 2,fHLTTemp);
              LCD_FLOAT(8,11, 2,fMashTemp);
              LCD_FLOAT(15,11, 2,fCabinetTemp);
              LCD_FLOAT(22,11, 2,fAmbientTemp);
              fHLTTempLast = fHLTTemp;
              fMashTempLast = fMashTemp;
              fAmbientTempLast = fAmbientTemp;
              fCabinetTempLast = fCabinetTemp;
            }
          lcd_text_xy(1 * 8, 2 * 16, buf[0], Yellow, Blue2);
          lcd_text_xy(1 * 8, 3 * 16, buf[1], Green, Blue2);
          lcd_text_xy(1 * 8, 4 * 16, buf[2], Blue2, Blue2);
          lcd_text_xy(1 * 8, 5 * 16, buf[3], Blue, Blue2);
          if (buf[4] != NULL)
            lcd_text_xy(1 * 8, 6 * 16, buf[4], Grey, Blue2);
          if (buf[5] != NULL)
            lcd_text_xy(1 * 8, 7 * 16, buf[5], Grey, Blue2);
          if (buf[6] != NULL)
            lcd_text_xy(1 * 8, 8 * 16, buf[6], Grey, Blue2);
          if (buf[7] != NULL)
            lcd_text_xy(1 * 8, 9 * 16, buf[7], Grey, Blue2);

          xSemaphoreGive(xBrewAppletRunningSemaphore); //give back the semaphore as its safe to return now.
          vTaskDelay(750);

        }
      else
        {
          CLEAR_APPLET_CANVAS;
          xSemaphoreGive(xBrewAppletRunningSemaphore); //give back the semaphore as its safe to return now.
          vTaskDelay(250);

        }
      static int j = 0;
      int i = uxTaskGetStackHighWaterMark(xBrewTaskHandle);
      sprintf(a, "BrewTask HWM: %d\r\n", i);
      if (j != i){
          vConsolePrint(a);
          j = i;
      }


      tog = tog ^ 1;


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
    return QUIT_BUTTON;

  return NO_BUTTON;

}

int iBrewKey(int xx, int yy)
{
  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  int iKeyFromApplet = 0;
  uint8_t uButton;
  const uint8_t uRun = RUNNING, uIdle = IDLE, uStepComplete = STEP_COMPLETE;
  uButton = uEvaluateTouch(xx,yy);
  static uint8_t iOneShot = 0;
  static unsigned char ucPause = 0;
  static struct GenericMessage * xMessage;
  xMessage = (struct GenericMessage *)malloc(sizeof(struct GenericMessage));



  if (xx == -1 || yy == -1)
    return 0;

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
          ucResStep = BrewState.ucStep;
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

      case QUIT_BUTTON:
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
         ucResStep = BrewState.ucStep;
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

      case QUIT_BUTTON:
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
          ucResStep = BrewState.ucStep;
          break;
        }
      case BUTTON_4:
        {
          if (ucResStep < MAX_BREW_STEPS)
            {
              //BrewState.ucStep++;
              ucResStep++;
              vConsolePrint("Increasing Brew Step\r\n");
            }
          break;
        }
      case BUTTON_5:
        {
          if (ucResStep > 0)
            {
              //BrewState.ucStep--;
              ucResStep--;
              vConsolePrint("Decreasing Brew Step\r\n");
            }
          break;
        }
      case BUTTON_6: //RESUME Button at bottom
        {
          //for (int i = 0; i < BrewState.ucStep; i++)
            for (int i = 0; i < ucResStep; i++)
            {
              Brew[i].ucComplete = 1;
            }
            BrewState.ucStep = ucResStep;
          vBrewRunStep();
          xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
          break;
        }

      case QUIT_BUTTON:
        {
          vConsolePrint("Leaving Resume Applet\r\n");
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
          vTaskSuspend(xBrewResAppletDisplayHandle);
          CLEAR_APPLET_CANVAS;
          DRAW_RESUME_BUTTONS;
          DRAW_MAIN_BUTTON_TEXT;

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
          ucResStep = BrewState.ucStep;
          vConsolePrint("Button3 Pressed\r\n");

          break;
        }
      case BUTTON_4:
        {
          vBrewRunStep();
          xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
          //xQueueSendToBack(xBrewTaskReceiveQueue, &uStepComplete, 0);
          break;
        }
      case BUTTON_5:
        {
          if (ucPause == 0)
            {
              xQueueSendToBack(xBrewTaskStateQueue, &uIdle, 0);
              ucPause = 1;
            }
          else if (ucPause == 1)
            {
              xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
              ucPause = 0;
            }

          break;
        }
      case BUTTON_6:
        {
          Brew[BrewState.ucStep].ucComplete = 1;
          if (BrewState.ucRunningState == RUNNING)
            {
              vBrewNextStep();
            }
          else
            BrewState.ucStep++;
          //xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
          break;
        }

      case QUIT_BUTTON:
        {
          iAppletState = QUIT;
          vConsolePrint("QUIT Pressed\r\n");
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY); //given back in QUIT state
          vTaskSuspend(xBrewAppletDisplayHandle);
          CLEAR_APPLET_CANVAS;
          //if (iOneShot == 0)
          {
            lcd_DrawRect(Q_X1, Q_Y1, Q_X2, Q_Y2, White);
            lcd_fill(Q_X1+1, Q_Y1+1, Q_W, Q_H, Blue);
            lcd_printf(13,4,13,"QUIT???");
            lcd_printf(13,5,13,"IF YES PRESS");
            lcd_printf(13,6,13,"QUIT AGAIN");
            lcd_printf(13,7,13,"IF NO, PRESS");
            lcd_printf(13,8,13,"HERE");
            vConsolePrint("Press back again to exit BREW\r\n");
          }
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

      if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
        {
          vConsolePrint("Selected to leave BREW Applet\r\n");

          vTaskResume(xBrewAppletDisplayHandle);
          if (xBrewAppletDisplayHandle != NULL)
            {
              vTaskDelete(xBrewAppletDisplayHandle);
              vTaskDelay(100);
              xBrewAppletDisplayHandle = NULL;
            }
          vBrewTotalReset();
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



static struct BrewStep Brew_REAL[] = {
    // TEXT                       SETUP                           POLL                                   PARMS          TIMEOUT   START    ELAPSED COMPLETE WAIT
    {"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 0},
   // {"Open BoilValve",       (void *)vBrewBoilValveSetupFunction, NULL,                              {OPEN,0,0,0,0},                       5,      0,      0, 0, 1},
    {"Close D-Valves",       (void *)vBrewValvesSetupFunction,    NULL,                              {CLOSE,0,0,0,0},                      1,      0,      0, 0, 0},
    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {CLOSE,0,0,0,0},                      5,      0,      0, 0, 1},
    {"Fill+Heat:Strike",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_FILL_HEAT,STRIKE,0,0,0},   40*60,  0,      0, 0, 0},
   // {"Grind Grains",         (void *)vBrewMillSetupFunction,      (void *)vBrewMillPollFunction,     {6,0,0,0,0},                          7*60,   0,      0, 0, 0},
    {"Drain HLT for Mash",   (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_DRAIN,STRIKE,0,0,0},       5*60,   0,      0, 0, 1},
  //{"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
  //{"Lower Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {DN_INC,0,0,0,0},                     60,     0,      0, 0, 0},
    {"Fill+Heat:Mash Out",   (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_FILL_HEAT, MASH_OUT,0,0,0},40*60,  0,      0, 0, 1},
    {"Mash",                 (void *)vBrewMashSetupFunction,      (void *)vBrewMashPollFunction,     {60,1,0,5,5},                          60*60,  0,      0, 0, 0},
    {"Drain for Mash Out",   (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_DRAIN,MASH_OUT,0,0,0},     5*60,   0,      0, 0, 1},
  //{"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
    {"Fill+Heat:Sparge",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_FILL_HEAT,SPARGE,0,0,0},   40*60,  0,      0, 0, 1},
  //{"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {60,0,0,0,0},                         2*60,   0,      0, 0, 0},
    {"Mash out",             NULL,                                (void *)vBrewMashPollFunction,     {10,5,0,3,3},                         10*60,  0,      0, 0, 0},
    {"MO pump to boil",      (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{3,5*60,0,0,0},                      11*60,  0,      0, 0, 0},
    {"Drain for Sparge",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_DRAIN,SPARGE,0,0,0},       10*60,  0,      0, 0, 1},
  //{"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          2*60,   0,      0, 0, 0},
    {"Sparge",               NULL,                                (void *)vBrewMashPollFunction,     {5,1,0,1,3},                          5*60,   0,      0, 0, 1},
    {"Pump to boil",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{3,5*60,0,0,0},                      11*60,  0,      0, 0, 1},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         0,      0,      0, 0, 0},
    {"Fill+Heat:Clean ",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_STATE_FILL_HEAT, MASH_OUT,0,0,0},40*60,  0,      0, 0, 0},
    {"BringToBoil",          (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {90,100,0,0,0},                        90*60,  0,      0, 0, 0},
    //{"Waiting",            NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0},
    {"Boil",                 (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {91,55,1,0,0},                        90*60,  0,      0, 0, 1},
    {"SettlingBeforeChill",  NULL,                                NULL,                                {120,0,0,0,0},                          0,      0,      0, 0, 0},
    {"Chill",                (void *)vBrewChillSetupFunction,     (void *)vBrewChillPollFunction ,   {5,0,0,0,0},                          10*60,  0,      0, 0, 1},
    {"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction,  {1,0,0,0,0},                          0,      0,      0, 0, 0},
    {NULL,                   NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0}
};

//static struct BrewStep Brew[] = {
//    {"Waiting",              NULL,                              (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                         20,     0,      0, 0, 0},
//    {"Close D-Valves",       (void *)vBrewValvesSetupFunction,  NULL,                              {CLOSE,0,0,0,0},                      1,      0,      0, 0, 0},
//    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction,NULL,                             {CLOSE,0,0,0,0},                      5,     0,      0, 0, 0},
//    {"Fill+Heat:HOT",     (void *)vBrewHLTSetupFunction,     NULL,                              {HLT_STATE_FILL_HEAT,MASH_OUT,0,0,0},   40*60,  0,      0, 0, 0},
//    {"Drain HLT to Mash",   (void *)vBrewHLTSetupFunction,     NULL,                              {HLT_STATE_DRAIN,STRIKE,0,0,0},       5*60,   0,      0, 0, 1},
//    {"Waiting",              NULL,                              (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                         20,     0,      0, 0, 0},
//    {"Fill+Heat:HOT",     (void *)vBrewHLTSetupFunction,     NULL,                              {HLT_STATE_FILL_HEAT,MASH_OUT,0,0,0},   40*60,  0,      0, 0, 1},
//    {"Mash out",             NULL,                              (void *)vBrewMashPollFunction,     {5,5,5,5,5},                         0,      0,      0, 0, 0}, // includes stirring etc (call the same func as mash with diff parms
//    {"MO pump to boil",      (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{3,5*60,0,0,0},                 11*60,   0,      0, 0, 0}, // have to do a couple of stop/starts for pump etc
//    {"Drain for Mash Out",   (void *)vBrewHLTSetupFunction,     NULL,                              {HLT_STATE_DRAIN,MASH_OUT,0,0,0},     5*60,   0,      0, 0, 1},
//    {"Mash out",             NULL,                              (void *)vBrewMashPollFunction,     {5,5,5,5,5},                         0,      0,      0, 0, 0}, // includes stirring etc (call the same func as mash with diff parms
//    {"MO pump to boil",      (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{3,5*60,0,0,0},                 11*60,   0,      0, 0, 0}, // have to do a couple of stop/starts for pump etc
//    {"BringToBoil",          (void *)vBrewBoilSetupFunction,     (void *)vBrewBoilPollFunction ,    {91,55,0,0,0},                       90*60,      0,      0, 0, 1},
//    {"Waiting",              NULL,                              (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                         20,     0,      0, 0, 0},
//    {"Chill",                 (void *)vBrewChillSetupFunction,    (void *)vBrewChillPollFunction , {5,0,0,0,0},                         0,      0,      0, 0, 1},
//    {NULL,                    NULL,                              NULL,                              {0,0,0,0,0},                          0,     0,      0, 0, 0}
//        // {"Grind Grains",         (void *)vBrewMillSetupFunction,    (void *)vBrewMillPollFunction,     {6,0,0,0,0},                          7*60,     0,      0, 0,
//};


static struct BrewStep Brew[] = {
    //TEXT                       SETUP                                POLL                                   PARMS            TIMEOUT   START    ELAPSED COMPLETE WAIT
    {"Waiting1",               NULL,                               (void *)vBrewWaitingPollFunction ,  {2,0,0,0,0},            20,     0,      0, 0, 0},
//    {"Close D-Valves",        (void *)vBrewValvesSetupFunction,    NULL,                               {CLOSE,0,0,0,0},        1,      0,      0, 0, 0},
    {"Open BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                               {OPEN,0,0,0,0},                      5,      0,      0, 0, 0},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 0},
    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {CLOSE,0,0,0,0},                      5,      0,      0, 0, 1},
    {"Lower Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {DN_INC,0,0,0,0},                         25,     0,      0, 0, 1},
    {"Waiting1",               NULL,                               (void *)vBrewWaitingPollFunction ,  {5,0,0,0,0},            20,     0,      0, 0, 0},
    //{"Mash",                  (void *)vBrewMashSetupFunction,      (void *)vBrewMashPollFunction,      {5,0,0,1,2},            6*60,  0,      0, 0, 0},
    {"Open BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                               {OPEN,0,0,0,0},                      5,      0,      0, 0, 1},
    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {CLOSE,0,0,0,0},                      5,      0,      0, 0, 1},

   // {"Mash",                  (void *)vBrewMashSetupFunction,      (void *)vBrewMashPollFunction,      {5,0,0,1,2},            6*60,  0,      0, 0, 0},
    {"Boil",                  (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,     {91,55,1,0,0},          90*60,  0,      0, 0, 1},
//    {"MO pump to boil",       (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{3,1*60,0,0,0},         11*60,  0,      0, 0, 0}, // have to do a couple of stop/starts for pump etc
//    {"BringToBoil",           (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,     {91,55,0,0,0},          90*60,  0,      0, 0, 0},
//    {"Boil",                  (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,     {91,55,1,0,0},          90*60,  0,      0, 0, 1},
//    {"Chill",                 (void *)vBrewChillSetupFunction,     (void *)vBrewChillPollFunction ,    {5,0,0,0,0},            0,      0,      0, 0, 1},
//    {"Raise Crane",           (void *)vBrewCraneSetupFunction,     NULL,                               {UP,0,0,0,0},           25,     0,      0, 0, 0},
//    {"Waiting2",              NULL,                                (void *)vBrewWaitingPollFunction ,  {2,0,0,0,0},            20,     0,      0, 0, 1},
//    {"Waiting3",              NULL,                                (void *)vBrewWaitingPollFunction ,  {3,0,0,0,0},            20,     0,      0, 0, 1},
    {"Waiting4",              NULL,                                (void *)vBrewWaitingPollFunction ,  {2,0,0,0,0},            20,     0,      0, 0, 1},
    {NULL,                    NULL,                                NULL,                               {0,0,0,0,0},            0,      0,      0, 0, 0}
};





// Creates a function that takes a single element array of BrewStep Pointers(?)
// note how calling each element (although above 0) shows each of their names.

//void iTest1(struct BrewStep (*ThisBrew)[1])
//{
//  static char buf[50];
//  sprintf(buf, "calling setup function of %s, %s, %s\r\n", ThisBrew[0]->pcStepName, ThisBrew[1]->pcStepName, ThisBrew[2]->pcStepName);
//  vConsolePrint(buf);
//ThisBrew[2]->func((void *)ThisBrew[2]->iFuncParams);
//
//}

// creates a pointer to a single elemented BrewStep Array.. points it to the
// statically allocated Brew array above, then calls uTest1 with the pointer as
// the argument.

//int iTest(int i){
//  struct BrewStep (*MyBrew)[1];
//  vConsolePrint("iTest\r\n");
//
//  MyBrew = &Brew;
//  iTest1(MyBrew);
//  MyBrew[2]->uTimeout = 1;
//  return 1;
//
//}


