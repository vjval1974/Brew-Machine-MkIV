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
#include <string.h>
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
#include "main.h"
#include "boil.h"

#define ARRAY_LENGTH( x ) ( sizeof(x)/sizeof(x[0]) ) // not as yet implemented

#define MAX_BREW_STEPS iMaxBrewSteps()
static float fMashTemp = 0.0;
static float fSpargeTemp = 0.0;
static float fMashOutTemp = 0.0;


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
double dValidateDrainLitres(double parameter);
double dGetSpargeSetpoint(int currentSpargeNumber);

float fGetNominalMashTemp()
{
  return fMashTemp;
}
float fGetNominalSpargeTemp()
{
  return fSpargeTemp;
}
float fGetNominalMashOutTemp()
{
  return fMashOutTemp;
}
xTaskHandle xBrewTaskHandle = NULL,
    xBrewAppletDisplayHandle = NULL,
    xBrewGraphAppletDisplayHandle = NULL,
    xBrewStatsAppletDisplayHandle = NULL,
    xBrewResAppletDisplayHandle = NULL;


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

/*
 * Step monitor loops through all steps up until the current step and
 * checks to see if there are any that are incomplete. Prints what step
 * we are waiting on and if it has timed out.
 * Parameter: iWaiting - if this parameter is set, we return the number of
 * incomplete steps. If not, we return 0.
 *
 */
static int iBrewStepMonitor(int iWaiting)
{
  static char buf[70];
  int iStep=0, iIncompleteSteps=0;
  int iNumberOfStepsToComplete;
  static int iPrintToConsoleCtr = 0;

  for (iStep = 0;iStep <= BrewState.ucStep;iStep++)
    {
      if (Brew[iStep].ucComplete == 0)
        {

          iIncompleteSteps++;
          Brew[iStep].uElapsedTime = (BrewState.uSecondCounter - Brew[iStep].uStartTime);

          if (iStep != BrewState.ucStep)
            {
              if (iPrintToConsoleCtr++ == 0) //print once every 10 calls
                {
                  sprintf(buf, "Waiting on Step %d:'%s' \r\n", iStep, Brew[iStep].pcStepName);
                  vConsolePrint(buf);
                }
              else if (iPrintToConsoleCtr >= 10)
                {
                  iPrintToConsoleCtr = 0;
                }
            }

          if (Brew[iStep].uElapsedTime > Brew[iStep].uTimeout)
            {
              sprintf(buf, "Step %d: %s - TIMEOUT\r\n", iStep, Brew[iStep].pcStepName);
              vConsolePrint(buf);
            }
        }

    }
  iNumberOfStepsToComplete = iIncompleteSteps;

  if (iWaiting)
    {
      return iNumberOfStepsToComplete;
    }
  else
    return 0;
}

// Initial runtime version of getting the number of brew steps so the macro doesnt need updating
// Should check the ARRAY_LENGTH(x) macro as it could be a better example.

static int iMaxBrewSteps(void)
{

  int i = 0;
  while (Brew[i].pcStepName != NULL)
    {
      i++;
    }
  return i;
}


static struct HLTMsg  Content7;
struct GenericMessage Message7;

void vBrewSetHLTStateIdle()
{
  static struct HLTMsg  * Content =  &Content7;
//  struct GenericMessage * Message = &Message7;
//  Content->pcMsgText = "HLT Idle";
//  Content->uData1 = 5.0;
//  Content->uState = HLT_STATE_IDLE;
//  Message->ucFromTask = BREW_TASK;
//  Message->ucToTask = HLT_TASK;
//  Message->uiStepNumber = BrewState.ucStep;
//  Message->pvMessageContent = Content;
//  xQueueSendToBack(xBrewTaskHLTQueue, &Message, portMAX_DELAY);
//
  static HltMessage h;
  h.command = HLT_CMD_IDLE;
  h.pcTxt = "SetIdle";
  h.iData1 = 0;
  h.iData2 = 0;
  h.fData3 = 0.0;
  h.fData4 = 0.0;
  xQueueSendToBack(xHltTaskQueue, &h, portMAX_DELAY);

}

// run this when pressing the quit button
static void vBrewQuitReset(void)
{
  int iMax = iMaxBrewSteps();
  vBrewReset();
  vBrewSetHLTStateIdle();
  for (int i = 0; i < iMax; i++)
    {
      Brew[i].ucComplete = 0;
      Brew[i].uElapsedTime = 0;
      Brew[i].uStartTime = 0;
    }

  if (xBrewTaskHandle != NULL)
    vTaskDelete(xBrewTaskHandle);
  if (xBrewTaskReceiveQueue)
    vQueueDelete(xBrewTaskReceiveQueue);
  if (xBrewTaskStateQueue)
    vQueueDelete(xBrewTaskStateQueue);

  xBrewTaskHandle = NULL;
  xBrewTaskReceiveQueue = NULL;
  xBrewTaskStateQueue = NULL;
}


//run this at the "brew finished step.
void vBrewTotalReset(void){

  int iMax = iMaxBrewSteps();
  vBrewReset();

  for (int i = 0; i < iMax; i++)
    {
      Brew[i].ucComplete = 0;
      Brew[i].uElapsedTime = 0;
      Brew[i].uStartTime = 0;
    }

}





unsigned char ucGetBrewHoursElapsed()
{
  return BrewState.uHoursElapsed;
}

unsigned char ucGetBrewMinutesElapsed()
{
  return BrewState.uMinutesElapsed;
}

unsigned char ucGetBrewSecondsElapsed()
{
  return BrewState.uSecondsElapsed;
}

unsigned char ucGetBrewStepMinutesElapsed()
{
  return Brew[BrewState.ucStep].uElapsedTime/60;
}

unsigned char ucGetBrewStepSecondsElapsed()
{
  return Brew[BrewState.ucStep].uElapsedTime%60;
}

unsigned char ucGetBrewStep()
{
  return BrewState.ucStep;

}

unsigned char ucGetBrewState()
{
  return BrewState.ucRunningState;
}


//----------------------------------------------------------------------------------------------------------------------------
// Brew Task
//----------------------------------------------------------------------------------------------------------------------------
const char * btx = "Brew Task xXX";
void vTaskBrew(void * pvParameters)
{


  portTickType xBrewStart;
  static uint16_t uBrewSecondsElapsed = 0, uBrewMinutesElapsed = 0, uBrewHoursElapsed=0;
  xBrewStart = xTaskGetTickCount();

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

  xMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));

  static char buf[50], buf1[50];

  for(;;)
    {
      if (xMessage == NULL)
        {
          vConsolePrint("BREW:xMessage = NULL\r\n");
          while(1);
        }

      if(xQueueReceive(xBrewTaskStateQueue, &uMsg, 0) == pdPASS)
        {
          BrewState.ucRunningState = uMsg;
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
              // this is where the print statements were for the console
            }
          if ((BrewState.uSecondsElapsed % 16) == 0)
            {

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
          if (!iWaiting)
            {
              if (Brew[BrewState.ucStep].poll)
                Brew[BrewState.ucStep].poll((void *)Brew[BrewState.ucStep].iFuncParams);
            }
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
static struct GenericMessage xMessage20;
const int iCommand1 = 0; // changed to see if its more stable

void vBrewReset(void){
  static struct GenericMessage * xMessage;
  xMessage = &xMessage20;

  if (xMessage == NULL)
    {
      vConsolePrint("BREW_RESET: xMessage = NULL!\r\n");
      while(1);
    }
  xMessage->ucFromTask = BREW_TASK_RESET;
  xMessage->ucToTask = BOIL_TASK;
  xMessage->pvMessageContent = (void *)&iCommand1;
  if (xBoilQueue != NULL)
    xQueueSendToBack(xBoilQueue, &xMessage, 0);
  vChillerPump(STOP_CHILLER_PUMP);
  vMashPump(STOP_MASH_PUMP);
  vMill(MILL_STOPPED); // need to change state name.
  vValveActuate(HLT_VALVE, CLOSE_VALVE);
  vValveActuate(INLET_VALVE, CLOSE_VALVE);
  vValveActuate(MASH_VALVE, CLOSE_VALVE);
  vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
}

static struct GenericMessage xMessage10;
static struct GenericMessage * xMessage11;

void vBrewNextStep(void){
  xMessage11 =  & xMessage10;
  static int iCommand;
  static char buf[50], buf1[50];
  static int iCount = 0;
  iCount++;
  if (xMessage11 == NULL)
    {
      vConsolePrint("BrewNextStep: xMessage = null\r\n");
      while(1);
    }

 // xMessage11 = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  xMessage11->ucFromTask = BREW_TASK;
  xMessage11->ucToTask = BREW_TASK;
  xMessage11->pvMessageContent = (void *)&iCommand;
  xMessage11->uiStepNumber = BrewState.ucStep;

  if (Brew[BrewState.ucStep+1].pcStepName != NULL){
    sprintf(buf, "Requesting next step: %d, %s\r\n", BrewState.ucStep+1, Brew[BrewState.ucStep+1].pcStepName);
    vConsolePrint(buf);
  }
  if (BrewState.ucStep < MAX_BREW_STEPS && Brew[BrewState.ucStep+1].ucWait == 1)
    {
      iCommand = STEP_WAIT;
      sprintf(buf1, "NextStep:'%s' requires waiting\r\n", Brew[BrewState.ucStep+1].pcStepName);
      vConsolePrint(buf1);
      vTaskDelay(100);
      xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage11, 0);

    }
  else if (BrewState.ucStep < MAX_BREW_STEPS-1){
      BrewState.ucStep++;
      vBrewRunStep();
  }
  else
    {
      vConsolePrint("Brew Finished!\r\n");
      BrewState.ucRunningState = IDLE;
      vBrewTotalReset();
    }
}

void vBrewRunStep(void){
  int iFuncReturnValue = 0;
  static char buf[50];

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
#define CLEAN 4

//static struct HLTMsg  Content1;
struct GenericMessage Message1;
//===================================================================================================================================================
void vBrewHLTSetupFunction(int piParameters[5]){
static HltMessage hltMessage;
hltMessage.iData1 = 0;
hltMessage.iData2 = 0;
hltMessage.fData3 = 0.0;
hltMessage.fData4 = 0.0;
hltMessage.command = HLT_CMD_IDLE;
hltMessage.pcTxt = "Blah";

  static int iSpargeNumber = 0;
  static double dSpargeSetpoint;


  switch( (HltCommand)piParameters[0] )
  {
  case HLT_CMD_HEAT_AND_FILL:
    {
      hltMessage.command = HLT_CMD_HEAT_AND_FILL;

      switch( piParameters[1] )
      {
      case STRIKE:
        {
        	hltMessage.pcTxt = "HLTSetup-Fill-Strike";
        	hltMessage.fData3 = BrewParameters.fStrikeTemp;
        	break;
        }
      case MASH_OUT:
        {
        	hltMessage.pcTxt = "HLTSetup-MashOut";
        	hltMessage.fData3 = BrewParameters.fMashOutTemp;
        	break;
        }
      case SPARGE:
        {
          dSpargeSetpoint = dGetSpargeSetpoint(iSpargeNumber);

          hltMessage.pcTxt = "HLTSetup-Sparge";
          hltMessage.fData3 = dSpargeSetpoint;
          iSpargeNumber++;
          break;
        }
      case NO_MASH_OUT:
        {
          break;
        }
      case CLEAN:
        {
           hltMessage.pcTxt = "HLTSetup-Clean";
           hltMessage.fData3 = BrewParameters.fCleanTemp;
          break;
        }

      }
      break;
    }
  case HLT_CMD_DRAIN:
    {
    	hltMessage.command = HLT_CMD_DRAIN;
      vResetFlow1();
      switch( piParameters[1] )
      {
      case STRIKE:
        {
        	if (BrewParameters.fStrikeLitres > BrewParameters.fHLTMaxLitres)
        		BrewParameters.fStrikeLitres = BrewParameters.fHLTMaxLitres;

        	hltMessage.pcTxt = "HltSetup-DrainStrike";
        	hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fStrikeLitres);
          break;
        }
      case MASH_OUT:
        {
        	hltMessage.pcTxt = "HltSetup-DrainMashOut";
        	hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fMashOutLitres);
          break;
        }
      case SPARGE:
        {
        	hltMessage.pcTxt = "HltSetup-DrainSparge";
        	hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fSpargeLitres);
          break;
        }
      case NO_MASH_OUT:
        {

          break;
        }
      case CLEAN:
        {

          break;
        }

      }
      break;

    }
  }
  hltMessage.ucStepNumber = BrewState.ucStep;
  xQueueSendToBack(xHltTaskQueue, &hltMessage, portMAX_DELAY);
  vTaskDelay(500);
  vBrewNextStep();
}

double dGetSpargeSetpoint(int currentSpargeNumber)
{

  double retval;
  switch (currentSpargeNumber)
      {
      case 0:
        {
          retval = BrewParameters.fSpargeTemp;
          break;
        }
      case 1:
        {
          retval = BrewParameters.fSpargeTemp2;
          break;
        }
      case 2:
        {
          retval = BrewParameters.fSpargeTemp3;
          break;
        }
      default:
        {
          retval = BrewParameters.fSpargeTemp;
          break;
        }
      }
  return retval;

}

double dValidateDrainLitres(double parameter)
{
  if (parameter > BrewParameters.fHLTMaxLitres)
     parameter = BrewParameters.fHLTMaxLitres;
  return parameter;
}

void vBrewMillSetupFunction (int piParameters[5])
{
  vMill(MILL_DRIVING);
}

void vBrewValvesSetupFunction (int piParameters[5])
{
  vValveActuate(HLT_VALVE, CLOSE_VALVE);
  vValveActuate(MASH_VALVE, CLOSE_VALVE);
  vValveActuate(INLET_VALVE, CLOSE_VALVE);
  vConsolePrint("Valves Setup Func Called\r\n");
  vTaskDelay(50);
  //xQueueSendToBack(xBrewTaskReceiveQueue, &STEP_COMPLETE, 0);
  Brew[BrewState.ucStep].ucComplete = 1;
  vBrewNextStep();
}

static struct GenericMessage xMessage30;
void vBrewBoilValveSetupFunction (int piParameters[5])
{
  static struct GenericMessage * xMessage = &xMessage30;
  static int * iCommand;
  iCommand = &piParameters[0];
  if (xMessage == NULL)
    {
      vConsolePrint("BoilValve setup: xMessage = null\r\n");
      while(1);
    }
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = BOIL_VALVE_TASK;
  xMessage->uiStepNumber = BrewState.ucStep;
  xMessage->pvMessageContent = (void *)iCommand;
  vConsolePrint("Boil Valve Setup Function called\r\n");
  xQueueSendToBack(xBoilValveQueue, &xMessage, 0);
  vBrewNextStep();
}

static struct GenericMessage xMessage31;
void vBrewCraneSetupFunction(int piParameters[5])
{
  static struct GenericMessage * xMessage = &xMessage31;
  static int * iCommand;
  iCommand = &piParameters[0];

  if (xMessage == NULL)
    {
      vConsolePrint("Cranesetup: xMessage = null\r\n");
      while(1);
    }
  xMessage->ucFromTask = BREW_TASK;
  xMessage->ucToTask = CRANE_TASK;
  xMessage->uiStepNumber = BrewState.ucStep;
  xMessage->pvMessageContent = (void *)iCommand;

  int i = xQueueSendToBack(xCraneQueue, &xMessage, 0);

  vBrewNextStep();
}

struct GenericMessage xMessagePreChillCloseBoilValve;
struct GenericMessage xMessagePreChillStopBoiler;

const int iCommand45 = 0; // changed to see if its more stable
const int iCommand46 = BOIL_VALVE_CLOSE;
// PRE-CHILL setup function.
//--------------------------
// Makes sure the boil valve is closed and turns on the pump.
void vBrewPreChillSetupFunction(int piParameters[5])
{
  int ii;
  static struct GenericMessage * msgCloseBoilValve = &xMessagePreChillCloseBoilValve;
  static struct GenericMessage * msgStopBoiler = &xMessagePreChillStopBoiler;

  if (ucGetBoilState() == BOILING)
    {
      msgStopBoiler->uiStepNumber = BrewState.ucStep;
      msgStopBoiler->ucFromTask = BREW_TASK_RESET;
      msgStopBoiler->ucToTask = BOIL_TASK;
      msgStopBoiler->pvMessageContent = (void *)&iCommand45;
      if (xBoilQueue != NULL)
        {
          xQueueSendToBack(xBoilQueue, &msgStopBoiler, 5000);
          vConsolePrint("Stopping Boil with duty cycle 0\r\n");
        }
    }
  vTaskDelay(200);
  //-------------------------------------------------
  if(ucGetBoilValveState() != BOIL_VALVE_CLOSED)
    {
      msgCloseBoilValve->ucFromTask = CHILL_TASK;
      msgCloseBoilValve->ucToTask = BOIL_VALVE_TASK;
      msgCloseBoilValve->uiStepNumber = BrewState.ucStep;
      msgCloseBoilValve->pvMessageContent = (void *)&iCommand46;
      xQueueSendToBack(xBoilValveQueue, &msgCloseBoilValve, 1000); // Make sure the boil valve is closed.
    }
  for (ii = 0; ii < BrewParameters.uiChillerPumpPrimingCycles; ii++)
    // try to prime, if not already primed
    {
      vChillerPump(START_CHILLER_PUMP);
      vTaskDelay(BrewParameters.uiChillerPumpPrimingTime*1000);
      vChillerPump(STOP_CHILLER_PUMP);
      vTaskDelay(BrewParameters.uiChillerPumpPrimingTime*1000);
    }

  vChillerPump(START_CHILLER_PUMP); //Pump.


}

// CHILL setup function.
//--------------------------
// 1 Close Boil Valve
// 2 Open Chiller Valve
// 3 Start Recirc Pump
struct GenericMessage xMessageChillCloseBoilValve;

const int iCommand47 = BOIL_VALVE_CLOSE;
void vBrewChillSetupFunction(int piParameters[5])
{
  static struct GenericMessage * msgCloseBoilValve = &xMessageChillCloseBoilValve;

  msgCloseBoilValve->ucFromTask = CHILL_TASK;
  msgCloseBoilValve->ucToTask = BOIL_VALVE_TASK;
  msgCloseBoilValve->uiStepNumber = BrewState.ucStep;
  msgCloseBoilValve->pvMessageContent = (void *)&iCommand47;
  vConsolePrint("Chill Setup Function called\r\n");
  xQueueSendToBack(xBoilValveQueue, &msgCloseBoilValve, 1000); // open boil valve to direct wort through chiller
  vTaskDelay(50);

  vChillerPump(START_CHILLER_PUMP); //Pump.
  vValveActuate(CHILLER_VALVE, OPEN_VALVE);
}

// PRE-CHILL polling function
//----------------------------
// waits for elapsed time, stops the pump, moves on
void vBrewPreChillPollFunction(int piParameters[5])
{
  if (Brew[BrewState.ucStep].uElapsedTime >= BrewParameters.uiSettlingRecircTime*60)
    {
      vChillerPump(STOP_CHILLER_PUMP);
    }

  if (Brew[BrewState.ucStep].uElapsedTime >= BrewParameters.uiSettlingTime*60)
    {
      vChillerPump(STOP_CHILLER_PUMP); // just in case
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();

    }
}

// CHILL Polling function

// 1 Wait for elapsed time and open boil valve.
// 2 Wait another 8 minutes until pumped out

static struct GenericMessage xMessage5;
static struct GenericMessage * xMessage6;

void vBrewChillPollFunction(int piParameters[5])
{
	if (Brew[BrewState.ucStep].uElapsedTime >= BrewParameters.uiChillTime*60)
	{
		Brew[BrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}
}

const int iCommandOpenBoilValve = BOIL_VALVE_OPEN;
void vBrewPumpToFermenterSetupFunction(int piParameters[5])
{

	xMessage6 = &xMessage5;

	vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
	vChillerPump(START_CHILLER_PUMP); //Pump.

	xMessage6->ucFromTask = CHILL_TASK;
	xMessage6->ucToTask = BOIL_VALVE_TASK;
	xMessage6->uiStepNumber = BrewState.ucStep;
	xMessage6->pvMessageContent = (void *)&iCommandOpenBoilValve;
	xQueueSendToBack(xBoilValveQueue, &xMessage6, 1000); // Open boil valve
}

void vBrewPumpToFermenterPollFunction(int piParameters[5])
{
	if (Brew[BrewState.ucStep].uElapsedTime >= BrewParameters.uiChillTime*60)
	{
		vChillerPump(STOP_CHILLER_PUMP); //Pump.
		Brew[BrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}

}

void vBrewMillPollFunction (int piParameters[5])
{
  int iGrindTime = BrewParameters.iGrindTime*60;

  if (Brew[BrewState.ucStep].uElapsedTime >= iGrindTime)
    {
      vMill(STOPPED);
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
    }
}

void vBrewWaitingPollFunction(int piParameters[5])
{
  if (Brew[BrewState.ucStep].uElapsedTime >= piParameters[0])
    {
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
    }
}

volatile  int iMashTime;
volatile  int iStirTime1;
volatile  int iStirTime2;
volatile  int iPumpTime1;
volatile  int iPumpTime2;


void vBrewMashOutSetupFunction(int piParameters[5])
{
  iMashTime = BrewParameters.iMashOutTime * 60;
  iStirTime1 = BrewParameters.iMashOutStirTime1 * 60;
  iStirTime2 = BrewParameters.iMashOutStirTime2 * 60;
  iPumpTime1 = BrewParameters.iMashOutPumpTime1 * 60;
  iPumpTime2 = BrewParameters.iMashOutPumpTime2 * 60;
}


static struct GenericMessage xSpargeWarmBoilerMessage;
  const int iDuty40PctConst = 40; // changed to see if its more stable

void vBrewSpargeSetupFunction(int piParameters[5])
{
  iMashTime = BrewParameters.iSpargeTime * 60;
  iStirTime1 = BrewParameters.iSpargeStirTime1 * 60;
  iStirTime2 = BrewParameters.iSpargeStirTime2 * 60;
  iPumpTime1 = BrewParameters.iSpargePumpTime1 * 60;
  iPumpTime2 = BrewParameters.iSpargePumpTime2 * 60;

  // send message to boil at 40% duty cycle to keep warm
    static struct GenericMessage * xMessage;
    xMessage = &xSpargeWarmBoilerMessage;
    xMessage->ucFromTask = BREW_TASK_SPARGESETUP;
    xMessage->ucToTask = BOIL_TASK;
    xMessage->pvMessageContent = (void *)&iDuty40PctConst;
    if (xBoilQueue != NULL)
      xQueueSendToBack(xBoilQueue, &xMessage, 0);
}


  void vBrewMashSetupFunction(int piParameters[5])
  {

    iMashTime = BrewParameters.iMashTime * 60;
    iStirTime1 = BrewParameters.iStirTime1 * 60;
    iStirTime2 = BrewParameters.iStirTime2 * 60;
    iPumpTime1 = BrewParameters.iPumpTime1 * 60;
    iPumpTime2 = BrewParameters.iPumpTime2 * 60;

    //open close to get air out of pump.
    int iCycles = 2;
    int ii;
    for (ii = 0; ii < iCycles; ii++)
      {
        vValveActuate(MASH_VALVE, OPEN_VALVE);
        vTaskDelay(4000);
        vValveActuate(MASH_VALVE, CLOSE_VALVE);
        vTaskDelay(4000);
      }

  }

  static void vRecordNominalTemps(void)
  {
    if ((Brew[BrewState.ucStep].uElapsedTime == 30*60) && strcmp(Brew[BrewState.ucStep].pcStepName, "Mash") == 0)
      {
        fMashTemp = ds1820_get_temp(MASH);
      }
    else if ((Brew[BrewState.ucStep].uElapsedTime == 5*60) && strcmp(Brew[BrewState.ucStep].pcStepName, "Sparge") == 0)
      {
        fSpargeTemp = ds1820_get_temp(MASH);
      }
    else if ((Brew[BrewState.ucStep].uElapsedTime == 5*60) && strcmp(Brew[BrewState.ucStep].pcStepName, "MashOut") == 0)
      {
        fMashOutTemp = ds1820_get_temp(MASH);
      }
  }

void vBrewMashPollFunction(int piParameters[5])
{

  int iTimeRemaining = iMashTime;
   iTimeRemaining = iMashTime;

  static char buf[50];
  static int iDisplayCtr = 0;
  iTimeRemaining = iMashTime - Brew[BrewState.ucStep].uElapsedTime;

  if (iDisplayCtr++ == 0){
      sprintf(buf, "Remaining: %d, Elapsed: %d, p1: %d, p2: %d, s1 %d, s2 %d\r\n", iTimeRemaining/60, Brew[BrewState.ucStep].uElapsedTime/60, iPumpTime1/60, iPumpTime2/60, iStirTime1/60, iStirTime2/60);
      vConsolePrint(buf);
  }
  else if (iDisplayCtr >= 60) iDisplayCtr = 0;

  vRecordNominalTemps(); // save the temp so it can be given to the UI.
  
  if ((Brew[BrewState.ucStep].uElapsedTime < iPumpTime1) || iTimeRemaining < iPumpTime2)
    vMashPump(START_MASH_PUMP);
  else vMashPump(STOP_MASH_PUMP);

  if ((Brew[BrewState.ucStep].uElapsedTime < iStirTime1) || iTimeRemaining < iStirTime2)
    vStir(STIR_DRIVING); 
  else vStir(STIR_STOPPED);

  if (Brew[BrewState.ucStep].uElapsedTime >= iMashTime)
    {
      vMashPump(STOP_MASH_PUMP);
      vStir(STIR_STOPPED);
      Brew[BrewState.ucStep].ucComplete = 1;
      vBrewNextStep();
    }
}


void vBrewPumpToBoilSetupFunction(int piParameters[5])
{
  int ii = 0;
  vValveActuate(MASH_VALVE, OPEN_VALVE);

  for (ii = 0; ii < BrewParameters.iPumpPrimingCycles; ii++)
    // try to prime, if not already primed
    {
      vMashPump(START_MASH_PUMP);
      vTaskDelay(BrewParameters.iPumpPrimingTime*1000);
      vMashPump(STOP_MASH_PUMP);
      vTaskDelay(BrewParameters.iPumpPrimingTime*1000);
    }
}

void vBrewPumpToBoilPollFunction(int  piParameters[5])
{
  int iPumpToBoilTime = piParameters[1];
  vMashPump(START_MASH_PUMP);
  vValveActuate(MASH_VALVE, OPEN_VALVE);
  //Brew[BrewState.ucStep].uTimeRemaining = (piParameters[1]*60) - Brew[BrewState.ucStep].uElapsedTime;
  if (Brew[BrewState.ucStep].uElapsedTime >= iPumpToBoilTime)
    {
      //-------------------------------------------------------------------------------------------
      // ToDo: SHOULD CHECK THE BOIL LEVEL LIMIT HERE SO THAT WE CAN ASSUME THAT THERE WAS NO STUCK SPARGE
      //-------------------------------------------------------------------------------------------

      Brew[BrewState.ucStep].ucComplete = 1;
      vMashPump(STOP_MASH_PUMP);
      vValveActuate(MASH_VALVE, CLOSE_VALVE);
      vBrewNextStep();
    }
#ifdef TESTING
  if (Brew[BrewState.ucStep].uElapsedTime >= 3)
      {
        Brew[BrewState.ucStep].ucComplete = 1;
        vMashPump(STOP_MASH_PUMP);
        vValveActuate(MASH_VALVE, CLOSE_VALVE);
        vBrewNextStep();
      }
#endif


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
const int i100 = 100;
static struct GenericMessage xBoilMessage1;
static struct GenericMessage * xBoilMessage = NULL;
static struct TextMsg xBoilTextMessage1;
static struct TextMsg * xBoilTextMessage = NULL;

void vBrewBoilSetupFunction(int piParameters[5])
{
  xBoilMessage = &xBoilMessage1;
  xBoilTextMessage = &xBoilTextMessage1;
  char buf[50];

  if (piParameters[2] == 1)
    {
      vTaskDelay(1000);
      iBoilState = BOIL_0;
      xBoilMessage->pvMessageContent = (void *)&piParameters[1];
      xBoilMessage->ucFromTask = BREW_TASK;
      xBoilTextMessage->pcMsgText = "BOIL";
      xBoilTextMessage->ucLine = 5;
      xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
      vValveActuate(MASH_VALVE, CLOSE_VALVE); // Runs water through the other side of the chiller.
    }
  else
    {
      xBoilMessage->pvMessageContent =  (void *)&piParameters[1];
      xBoilMessage->ucFromTask = BREW_TASK_BRING_TO_BOIL;
      xBoilTextMessage->pcMsgText = "Bring To Boil";
      xBoilTextMessage->ucLine = 5;
      xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
      iBoilState = BOIL_7;
    }

  xBoilMessage->ucToTask = BOIL_TASK;
  xBoilMessage->uiStepNumber = BrewState.ucStep;
  vConsolePrint("Boil Setup Function called\r\n");
  xQueueSendToBack(xBoilQueue, &xBoilMessage, 5000);
}

void vBrewBoilPollFunction(int piParameters[5])
{

  int iBoilTime = BrewParameters.uiBoilTime*60;
  int iBringToBoilTime = BrewParameters.uiBringToBoilTime*60;
  int iBoilDuty = piParameters[1];
  int iTimeRemaining = ((iBoilTime - Brew[BrewState.ucStep].uElapsedTime)/60);
  int iBringTimeRemaining= ((iBringToBoilTime - Brew[BrewState.ucStep].uElapsedTime)/60);
  static int iLastTime;
  static char buf[50];
  static int iChillerValveChecked = FALSE;

  //added 14/11/2014 ... sends a message every second so the brew task can check the ADC
  //not ideal, but will do for now.
  // ToDo: Fix this ADC issue
  if(xBoilMessage == NULL)
    {
      xBoilMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
      vConsolePrint("Had to allocate memory for xBoilMessage!!!\r\n");
    }

  // if we are boiling and there is 5 mins remaining, make sure the boil valve is closed and start pumping around sanitising the chiller etc
  if (iBoilState != BOIL_7 && iTimeRemaining <= 5)
    {
      if (iChillerValveChecked == FALSE)
        {
          vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
          iChillerValveChecked = TRUE;
        }
      vChillerPump(START_CHILLER_PUMP);
    }

  // Print time remaining for the boil or bring to boil.
  if (iBoilState == BOIL_7)
    {
      if (iBringTimeRemaining != iLastTime)
        {
          sprintf(buf, "Time Remaining: %d\r\nState = %d\r\n", iBringTimeRemaining, iBoilState);
          vConsolePrint(buf);
          iLastTime = iBringTimeRemaining;
        }
    }
  else
    {
      if (iTimeRemaining != iLastTime)
        {
          sprintf(buf, "Time Remaining: %d\r\nState = %d\r\n", iTimeRemaining, iBoilState);
          vConsolePrint(buf);
          iLastTime = iTimeRemaining;
        }
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
      if (iTimeRemaining <=   BrewParameters.uiHopTimes[5])
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
          xBoilMessage->pvMessageContent = (void*)iBoilDuty;
          Brew[BrewState.ucStep].ucComplete = 1;
          xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);
          vConsolePrint("Boil Completed, sent duty 0 to boil\r\n");
          vBrewNextStep();
        }
      break;
    }
  case BOIL_7: //bringtoboil
    {
      if (Brew[BrewState.ucStep].uElapsedTime >= iBringToBoilTime)
        {
          iBoilDuty = 0;
          xBoilMessage->pvMessageContent = (void*)iBoilDuty;
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
      // hang here, something went really wrong.
    }
  }// switch

}

//----------------------------------------------------------------------------------------------------------------------------
// BREW APPLET - Called from menu
//----------------------------------------------------------------------------------------------------------------------------
void vBrewApplet(int init){

  if (init)
    {

      lcd_DrawRect(TOP_BANNER_X1, TOP_BANNER_Y1, TOP_BANNER_X2, TOP_BANNER_Y2, Cyan);
      lcd_fill(TOP_BANNER_X1+1, TOP_BANNER_Y1+1, TOP_BANNER_W, TOP_BANNER_H, Blue);
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

      if (xBrewAppletTextQueue == NULL)
        {
          xBrewAppletTextQueue = xQueueCreate(5,sizeof(struct TextMsg *));
          if (xBrewAppletTextQueue == NULL)
            vConsolePrint("FATAL Error creating Applet Text Queue\r\n");
          else vConsolePrint("Created Applet Text Queue\r\n");
        }

      if (xBrewTaskHandle == NULL)
        xTaskCreate( vTaskBrew,
            ( signed portCHAR * ) "Brew Task",
            configMINIMAL_STACK_SIZE + 900,
            NULL,
            tskIDLE_PRIORITY+2,
            &xBrewTaskHandle );
      vConsolePrint("Brew Task Created\r\n");

      if (xBrewHLTTaskHandle == NULL)
        xTaskCreate( vTaskBrewHLT,
            ( signed portCHAR * ) "Brew HLT",
            configMINIMAL_STACK_SIZE + 900,
            NULL,
            tskIDLE_PRIORITY,
            &xBrewHLTTaskHandle );
      vConsolePrint("Brew HLT Task Created\r\n");

      xTaskCreate( vBrewAppletDisplay,
          ( signed portCHAR * ) "Brew Display",
          configMINIMAL_STACK_SIZE + 200,
          NULL,
          tskIDLE_PRIORITY,
          &xBrewAppletDisplayHandle );

      if (xBrewGraphAppletDisplayHandle == NULL)
        {
          xTaskCreate( vBrewGraphAppletDisplay,
              ( signed portCHAR * ) "Brew Graph",
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
              ( signed portCHAR * ) "Brew Stats",
              configMINIMAL_STACK_SIZE + 100,
              NULL,
              tskIDLE_PRIORITY,
              &xBrewStatsAppletDisplayHandle );
          vConsolePrint("Stats Applet Task Created\r\n");
        }

      vTaskSuspend(xBrewStatsAppletDisplayHandle);

      if (xBrewResAppletDisplayHandle == NULL)
        {
          xTaskCreate( vBrewResAppletDisplay,
              ( signed portCHAR * ) "Brew Resume",
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
      vTaskDelay(5000);
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
      HltState hlt = GetHltState();
      BoilerState boiler = GetBoilerState();

      CLEAR_APPLET_CANVAS;
      lcd_printf(0,2,20, "HLT Level = %s", hlt.levelStr);
      lcd_printf(1,3,20, "%s", hlt.drainingStr);
      lcd_printf(1,4,20, "%s", hlt.fillingStr);
      lcd_printf(0,5,20, "BOILER %s", boiler.boilStateStr);
      lcd_printf(1,6,20, "%s", boiler.levelStr);
      lcd_printf(1,7,20, "Duty Cycle = %s", boiler.dutyStr);
      lcd_printf(0,8,20, "Litres Dlvrd = %d", uiGetActualLitresDelivered());
      lcd_printf(4,9,20, "MASH Temp = %d", (int)fGetNominalMashTemp());

      vTaskDelay(1000);
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

unsigned int uiGetBrewAppletDisplayHWM()
{
  if (xBrewAppletDisplayHandle)
   {return uxTaskGetStackHighWaterMark(xBrewAppletDisplayHandle);}
  else return 0;
}

unsigned int uiGetBrewTaskHWM()
{
  if (xBrewTaskHandle)
  {return uxTaskGetStackHighWaterMark(xBrewTaskHandle);}
  else return 0;
}

unsigned int uiGetBrewGraphAppletHWM()
{
  if(xBrewGraphAppletDisplayHandle)
  {return uxTaskGetStackHighWaterMark(xBrewGraphAppletDisplayHandle);}
  else return 0;
}
unsigned int uiGetBrewStatsAppletHWM()
{
  if(xBrewStatsAppletDisplayHandle)
  {return uxTaskGetStackHighWaterMark(xBrewStatsAppletDisplayHandle);}
  else return 0;
}

unsigned int uiGetBrewResAppletHWM()
{
  if (xBrewResAppletDisplayHandle)
  {return uxTaskGetStackHighWaterMark(xBrewResAppletDisplayHandle);}
  else return 0;
}

void vBrewAppletDisplay( void *pvParameters){

  static char tog = 0; //toggles each loop
  //unsigned int uiDecimalPlaces = 3;
  float fHLTTemp = 0, fMashTemp = 0, fAmbientTemp = 0, fCabinetTemp;
  float fHLTTempLast = 1, fMashTempLast = 1, fAmbientTempLast = 1, fCabinetTempLast;
  static char buf[8][40];
  static char b[40], a[40];
  struct TextMsg * RcvdTextMsg;
  RcvdTextMsg = (struct TextMsg *)pvPortMalloc(sizeof(struct TextMsg));
  HltLevel hltLevel;
  static unsigned char ucLastStep = 255;
  static unsigned char ucLastState = 255;
  static HltLevel ucLastHLTLevel = HLT_LEVEL_LOW;
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

      hltLevel = xGetHltLevel();

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
      if (ucLastHLTLevel != hltLevel)
        {
          sprintf(buf[3], "HLT Level = %s", pcHLTLevels[hltLevel]);
          ucLastHLTLevel = hltLevel;
        }
      sprintf(buf[2], "Elapsed: %02d:%02d Brew %02d:%02d:%02d",
          ucGetBrewStepMinutesElapsed(), ucGetBrewStepSecondsElapsed(),
          ucGetBrewHoursElapsed(), ucGetBrewMinutesElapsed(), ucGetBrewSecondsElapsed());

      if(tog)
        {
          CLEAR_APPLET_CANVAS;

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

const uint8_t uRun = RUNNING;
void vBrewRemoteStart(){
  vBrewRunStep();
  xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
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

  if (xx == -1 || yy == -1)
    return 0;

  switch (iAppletState)
  {
  case GRAPH:
    {
    //  vConsolePrint("AppletState = GRAPH\r\n");
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
      //vConsolePrint("AppletState = STATS\r\n");
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
      //vConsolePrint("AppletState = RES\r\n");
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
                ucResStep++;
            }
          break;
        }
      case BUTTON_5:
        {
          if (ucResStep > 0)
            {
                ucResStep--;
            }
          break;
        }
      case BUTTON_6: //RESUME Button at bottom
        {
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
          break;
        }
      case BUTTON_4:
        {
          vBrewRunStep();
          xQueueSendToBack(xBrewTaskStateQueue, &uRun, 0);
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
          break;
        }

      case QUIT_BUTTON:
        {
          iAppletState = QUIT;
          xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY); //given back in QUIT state
          vTaskSuspend(xBrewAppletDisplayHandle);
          CLEAR_APPLET_CANVAS;

          lcd_DrawRect(Q_X1, Q_Y1, Q_X2, Q_Y2, White);
          lcd_fill(Q_X1+1, Q_Y1+1, Q_W, Q_H, Blue);
          lcd_printf(13,4,13,"QUIT???");
          lcd_printf(13,5,13,"IF YES PRESS");
          lcd_printf(13,6,13,"QUIT AGAIN");
          lcd_printf(13,7,13,"IF NO, PRESS");
          lcd_printf(13,8,13,"HERE");

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
            if (xBrewAppletDisplayHandle != NULL)
            {
              vTaskDelete(xBrewAppletDisplayHandle);
              vTaskDelay(100);
              xBrewAppletDisplayHandle = NULL;
            }
          vBrewQuitReset();
          //return the semaphore for taking by another task.
          xSemaphoreGive(xBrewAppletRunningSemaphore);
          iAppletState = MAIN;
          vTaskDelay(100);
          return 1;
        }
      else if (xx > Q_X1 && yy > Q_Y1 && xx < Q_X2 && yy < Q_Y2)
        {
          vTaskResume(xBrewAppletDisplayHandle);
          iAppletState = MAIN;
        }
      xSemaphoreGive(xBrewAppletRunningSemaphore);
      break;
    }// case QUIT
  }//Switch iAppletState

  vTaskDelay(10);
  return 0;
}

// 11 seconds per litre pumping to the mash.

// NO Mash out Brew Test - WORKS
static struct BrewStep Brew[] = {
    // TEXT                       SETUP                           POLL                                   PARMS                          TIMEOUT   START ELAPSED COMPLETE WAIT
    {"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 0},
    {"Close D-Valves",       (void *)vBrewValvesSetupFunction,    NULL,                              {0,0,0,0,0},                          1,      0,      0, 0, 0},
    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {BOIL_VALVE_CLOSE,0,0,0,0},           20,      0,      0, 0, 1},
    {"Fill+Heat:Strike",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,STRIKE,0,0,0},   40*60,  0,      0, 0, 0},
    {"Grind Grains",         (void *)vBrewMillSetupFunction,      (void *)vBrewMillPollFunction,     {0,0,0,0,0},                          20*60,   0,      0, 0, 0},
    {"DrainHLTForMash",      (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,STRIKE,0,0,0},       5*60,   0,      0, 0, 1},
    {"Lower Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {DN_INC,0,0,0,0},                     2*60,     0,      0, 0, 1},
    {"Fill+Heat:Sparge1",    (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,SPARGE,0,0,0},   40*60,  0,      0, 0, 1},
    {"Mash",                 (void *)vBrewMashSetupFunction,      (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          60*60,  0,      0, 0, 0},
    {"MashPumpToBoil",        (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,4*60,0,0,0},                      11*60,  0,      0, 0, 0},
    {"DrainForSparge1",       (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,SPARGE,0,0,0},       5*60,  0,      0, 0, 1},
    {"Fill+Heat:Sparge2",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,SPARGE,0,0,0},   40*60,  0,      0, 0, 1},
    {"Sparge1",               (void *)vBrewSpargeSetupFunction,    (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          25*60,   0,      0, 0, 0},
    {"Pump to boil1",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,3*60,0,0,0},                      11*60,  0,      0, 0, 0},
    {"DrainForSparge2",       (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,SPARGE,0,0,0},       5*60,  0,      0, 0, 1},
    {"Sparge2",               (void *)vBrewSpargeSetupFunction,    (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          25*60,   0,      0, 0, 0},
    {"Pump to boil2",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,3*60,0,0,0},                      11*60,  0,      0, 0, 1},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 1},
    {"Fill+Heat:Clean ",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL, CLEAN,0,0,0},   40*60,  0,      0, 0, 0},
    {"BringToBoil",          (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {22,100,0,0,0},                       30*60,  0,      0, 0, 0},
    {"Pump to boil",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,30,0,0,0},                        2*60,  0,      0, 0, 1},
    {"Boil",                 (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {60,55,1,0,0},                        90*60,  0,      0, 0, 1},
    {"SettlingBefChill",     (void *)vBrewPreChillSetupFunction,  (void *)vBrewPreChillPollFunction,  {6*60,0,0,0,0},                      10*60,   0,      0, 0, 0},
    {"Chill",                (void *)vBrewChillSetupFunction,     (void *)vBrewChillPollFunction ,   {8,0,0,0,0},                          10*60,  0,      0, 0, 1},
    {"Pump Out",    (void *)vBrewPumpToFermenterSetupFunction,     (void *)vBrewPumpToFermenterPollFunction ,   {8,0,0,0,0},                          10*60,  0,      0, 0, 1},
    {"BREW FINISHED",        NULL,                                (void *)vBrewWaitingPollFunction,  {1,0,0,0,0},                          2,      0,      0, 0, 0},
    {NULL,                   NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0}
};

//static struct BrewStep Brew[] = {
//    // TEXT                       SETUP                           POLL                                   PARMS                          TIMEOUT   START ELAPSED COMPLETE WAIT
//    {"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
//    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 1},
//    {"Lower Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {DN,0,0,0,0},                         25,     0,      0, 0, 1},
//    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {UP,0,0,0,0},                         25,     0,      0, 0, 1},
//    {"Lower+Stir",           (void *)vBrewCraneSetupFunction,     NULL,                              {DN_INC,0,0,0,0},                     60,     0,      0, 0, 1},
//
//    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {BOIL_VALVE_CLOSE,0,0,0,0},           15,      0,      0, 0, 1},
//    {"Open BoilValve",       (void *)vBrewBoilValveSetupFunction, NULL,                              {BOIL_VALVE_OPEN,0,0,0,0},            15,      0,      0, 0, 1},
//    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {BOIL_VALVE_CLOSE,0,0,0,0},           15,      0,      0, 0, 1},
//
//    {"Grind Grains",         (void *)vBrewMillSetupFunction,      (void *)vBrewMillPollFunction,     {0,0,0,0,0},                          10*60,  0,      0, 0, 1},
//
//    {"BREW FINISHED",        NULL,                                (void *)vBrewWaitingPollFunction,  {1,0,0,0,0},                          2,      0,      0, 0, 1},
//    {NULL,                   NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0}
//};
//
//static struct BrewStep Brew[] = {
//    // TEXT                  SETUP                           POLL                                   PARMS                          TIMEOUT   START ELAPSED COMPLETE WAIT
//    {"Waiting",         NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
//    {"Boil",                 (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {60,55,1,0,0},                        90*60,  0,      0, 0, 1},
//    {"SettlingBefChill",     (void *)vBrewPreChillSetupFunction,  (void *)vBrewPreChillPollFunction,  {6*60,0,0,0,0},                      10*60,   0,      0, 0, 0},
//    {"Chill",                (void *)vBrewChillSetupFunction,     (void *)vBrewChillPollFunction ,   {8,0,0,0,0},                          10*60,  0,      0, 0, 1},
//    {"BREW FINISHED",        NULL,                                (void *)vBrewWaitingPollFunction,  {1,0,0,0,0},                          2,      0,      0, 0, 0},
//    {NULL,              NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0}
//};


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


