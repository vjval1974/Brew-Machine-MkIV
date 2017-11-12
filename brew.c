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
#include "MashWater.h"

#define ARRAY_LENGTH( x ) ( sizeof(x)/sizeof(x[0]) ) // not as yet implemented
#define MAX_BREW_STEPS iMaxBrewSteps()
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


const char  * pcRunningState[2] = {"IDLE", "RUNNING"};
const char  * pcHLTLevels[3] = {"LOW", "MID", "HIGH"};
const char  * pcHLTStates[4] = {"IDLE", "FILL+HEAT", "DRAIN", "AT TEMP"};
const char  * pcCraneStates[6] = {"Top", "Bottom", "Driving up", "Driving Down", "Down Inc", "Stopped"};
const char  * pcTASKS[6] = {"Brew", "Boil", "HLT", "Crane", "BoilValve", "HopDropper"};
const char  * pcBrewMessages[3] = {"Step Complete", "Step Failed", "Wait for previous steps"};

static float fMashTemp = 0.0;
static float fMashStage2Temp = 0.0;
static float fSpargeTemp = 0.0;
static float fMashOutTemp = 0.0;
static unsigned char ucResStep = 0;


// Applet States
typedef enum
{
	MAIN_APPLET = 0,
	GRAPH_APPLET = 1,
	STATS_APPLET = 2,
	RESUME_APPLET = 3,
	QUIT_APPLET = 4,
	SPARE_APPLET = 5
} AppletState;

typedef enum
{
	STRIKE,
	MASH_STAGE_2,
	MASH_OUT,
	SPARGE,
	NO_MASH_OUT,
	CLEAN
} MashStageForTempSetpoint;

typedef struct
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
} BrewStep; //Brew is an array of brew steps

static BrewStep BrewSteps[];
BrewState ThisBrewState;
AppletState xAppletState = MAIN_APPLET;

xTaskHandle xBrewTaskHandle = NULL, xBrewAppletDisplayHandle = NULL, xBrewGraphAppletDisplayHandle = NULL,
	xBrewStatsAppletDisplayHandle = NULL, xBrewResAppletDisplayHandle = NULL;
xSemaphoreHandle xBrewAppletRunningSemaphore;
xQueueHandle xBrewTaskStateQueue = NULL, xBrewTaskReceiveQueue = NULL;
xQueueHandle xBrewTaskStateQueue1 = NULL, xBrewTaskReceiveQueue1 = NULL;
xQueueHandle xBrewAppletTextQueue = NULL;

int iMaxBrewSteps(void);
bool IsEnoughWaterInBoilerForHeating();
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
void vBrewSetHLTStateIdle();
static void vRecordNominalTemps(void);

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
	int iStep = 0, iIncompleteSteps = 0;
	int iNumberOfStepsToComplete;
	static int iPrintToConsoleCtr = 0;

	for (iStep = 0; iStep <= ThisBrewState.ucStep; iStep++)
	{
		if (BrewSteps[iStep].ucComplete == 0)
		{

			iIncompleteSteps++;
			BrewSteps[iStep].uElapsedTime = (ThisBrewState.uSecondCounter - BrewSteps[iStep].uStartTime);

			if (iStep != ThisBrewState.ucStep)
			{
				if (iPrintToConsoleCtr++ == 0) //print once every 10 calls
				{
					sprintf(buf, "Waiting on Step %d:'%s' \r\n\0", iStep, BrewSteps[iStep].pcStepName);
					vConsolePrint(buf);
				}
				else if (iPrintToConsoleCtr >= 10)
				{
					iPrintToConsoleCtr = 0;
				}
			}

			if (BrewSteps[iStep].uElapsedTime > BrewSteps[iStep].uTimeout)
			{
				sprintf(buf, "Step %d: %s - TIMEOUT\r\n\0", iStep, BrewSteps[iStep].pcStepName);
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



//RESET RELATED METHODS
void vBrewReset(void)
{
	static BoilMessage xMessage;
	xMessage.ucFromTask = BREW_TASK_RESET;
	xMessage.iDutyCycle = 0;
	if (xBoilQueue != NULL )
		xQueueSendToBack(xBoilQueue, &xMessage, 0);
	vChillerPump(STOP_CHILLER_PUMP);
	vMashPump(STOP_MASH_PUMP);
	vMill(MILL_STOPPED); // need to change state name.
	vStir(STIR_STOPPED);
	vValveActuate(HLT_VALVE, CLOSE_VALVE);
	vValveActuate(INLET_VALVE, CLOSE_VALVE);
	vValveActuate(MASH_VALVE, CLOSE_VALVE);
	vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
}

//run this at the "brew finished step.
void vBrewResetAndZeroAllStepFlags(void)
{
	vBrewReset();
	for (int i = 0; i < MAX_BREW_STEPS; i++)
	{
		BrewSteps[i].ucComplete = 0;
		BrewSteps[i].uElapsedTime = 0;
		BrewSteps[i].uStartTime = 0;
	}
}

// run this when pressing the quit button
static void vBrewReset_DeleteTasks_ZeroFlags(void)
{
	vBrewResetAndZeroAllStepFlags();
	vBrewSetHLTStateIdle();

	if (xBrewTaskHandle != NULL )
		vTaskDelete(xBrewTaskHandle);
	if (xBrewTaskReceiveQueue)
		vQueueDelete(xBrewTaskReceiveQueue);
	if (xBrewTaskStateQueue)
		vQueueDelete(xBrewTaskStateQueue);

	xBrewTaskHandle = NULL;
	xBrewTaskReceiveQueue = NULL;
	xBrewTaskStateQueue = NULL;
}

unsigned char ucGetBrewHoursElapsed()
{
  return ThisBrewState.uHoursElapsed;
}

unsigned char ucGetBrewMinutesElapsed()
{
  return ThisBrewState.uMinutesElapsed;
}

unsigned char ucGetBrewSecondsElapsed()
{
  return ThisBrewState.uSecondsElapsed;
}

unsigned char ucGetBrewStepMinutesElapsed()
{
  return BrewSteps[ThisBrewState.ucStep].uElapsedTime/60;
}

unsigned char ucGetBrewStepSecondsElapsed()
{
  return BrewSteps[ThisBrewState.ucStep].uElapsedTime%60;
}

unsigned char ucGetBrewStep()
{
  return ThisBrewState.ucStep;
}

unsigned char ucGetBrewState()
{
  return ThisBrewState.xRunningState;
}


//----------------------------------------------------------------------------------------------------------------------------
// Brew Task
//----------------------------------------------------------------------------------------------------------------------------
const char * btx = "Brew Task xXX";
void vTaskBrew(void * pvParameters)
{
	portTickType xBrewStart;
	static uint16_t uBrewSecondsElapsed = 0, uBrewMinutesElapsed = 0, uBrewHoursElapsed = 0;
	xBrewStart = xTaskGetTickCount();

	ThisBrewState.ucStep = 0;
	ThisBrewState.uHoursElapsed = 0;
	ThisBrewState.uMinutesElapsed = 0;
	ThisBrewState.uSecondCounter = 0;
	ThisBrewState.xHLTState.hltBrewState = HLT_STATE_IDLE;
	ThisBrewState.ucHopAddition = 0;
	ThisBrewState.xRunningState = IDLE;
	ThisBrewState.ucTarget = 0;
	ThisBrewState.ucTargetHLTTemp = 0;
	ThisBrewState.ucTargetHLTVolume = 0;
	ThisBrewState.ucTargetWaterVolume = 0;
	ThisBrewState.xLastWakeTime = xTaskGetTickCount();
	static uint8_t uMsg = IDLE;
	BrewStepCommand iContent = 0;
	static int iWaiting = 0, iStepsToComplete = 0;

	BrewMessage xMessage;

	static char buf[50], buf1[50];

	for (;;)
	{

		if (xQueueReceive(xBrewTaskStateQueue, &uMsg, 0) == pdPASS)
		{
			ThisBrewState.xRunningState = uMsg;
		}
		switch (ThisBrewState.xRunningState)
		{
			case IDLE:
				{ // do nothing , idling
				break;
			}
			case RUNNING:
				{
					//TODO: Log the brew time at certain intervals, say 5mins so that the log can be read better.
					// Also think about quizzing another micro that has a RTC on board. Or... get motivated and add an 12C rtc ... the code is almost there for it.
				if (xQueueReceive(xBrewTaskReceiveQueue, &xMessage, 0) == pdPASS)
				{
					iContent = xMessage.xCommand; //casts to int * and then dereferences.
					sprintf(buf, "Brew Task: Msg from %s : %d : %u\r\n\0", pcTASKS[xMessage.ucFromTask - TASK_BASE], xMessage.xCommand, xMessage.iBrewStep);
					vConsolePrint(buf);

					sprintf(buf1, "Brew Task: iContent = %s\r\n\0", pcBrewMessages[iContent]);
					vConsolePrint(buf1);

					switch (iContent)
					{
						case BREW_STEP_COMPLETE:
							{
							BrewSteps[xMessage.iBrewStep].ucComplete = 1;

							sprintf(buf, "Brew Task: Setting Step %d to Complete\r\n\0", xMessage.iBrewStep);
							vConsolePrint(buf);
							break;
						}
						case BREW_STEP_FAILED:
							{

							break;
						}
						case BREW_STEP_WAIT:
							{
							vConsolePrint("WAITING COMMAND RECEIVED\r\n\0");
							iWaiting = 1;

						}
					}
				}


				iStepsToComplete = iBrewStepMonitor(iWaiting); // updates elapsed times and monitors timeouts of steps.

				if (iWaiting) // are we waiting on all previous steps to be completed?
				{
					if (iStepsToComplete) // are there any steps to be completed?
					{
						iWaiting = 1;
					}
					else // if not, we move on...
					{
						iWaiting = 0;
						ThisBrewState.ucStep++;
						printMashTunState();
						printLitresCurrentlyInBoiler();
						vBrewRunStep();
					}
				}

				// If there is a polling function, run it.
				if (!iWaiting)
				{
					if (BrewSteps[ThisBrewState.ucStep].poll)
						BrewSteps[ThisBrewState.ucStep].poll((void *) BrewSteps[ThisBrewState.ucStep].iFuncParams);
				}
				// Update the counters
				ThisBrewState.uSecondCounter++;
				ThisBrewState.uSecondsElapsed++;
				if (ThisBrewState.uSecondsElapsed >= 60)
				{
					ThisBrewState.uSecondsElapsed = 0;
					ThisBrewState.uMinutesElapsed++;
				}

				if (ThisBrewState.uMinutesElapsed >= 60)
				{
					ThisBrewState.uMinutesElapsed = 0;
					ThisBrewState.uHoursElapsed++;
				}

				break;
			}
		}
		vTaskDelayUntil(&ThisBrewState.xLastWakeTime, 1000 / portTICK_RATE_MS);
	}
}
//----------------------------------------------------------------------------------------------------------------------------

// STEP RELATED METHODS
void vBrewNextStep(void)
{
	BrewMessage xMessage11;
	static char buf[50], buf1[50];
	static int iCount = 0;
	iCount++;

	if (BrewSteps[ThisBrewState.ucStep + 1].pcStepName != NULL )
	{
		sprintf(buf, "Requesting next step: %d, %s\r\n\0", ThisBrewState.ucStep + 1, BrewSteps[ThisBrewState.ucStep + 1].pcStepName);
		vConsolePrint(buf);
	}

	if (ThisBrewState.ucStep < MAX_BREW_STEPS && BrewSteps[ThisBrewState.ucStep + 1].ucWait == 1)
	{
		sprintf(buf1, "NextStep:'%s' requires waiting\r\n\0", BrewSteps[ThisBrewState.ucStep + 1].pcStepName);
		vConsolePrint(buf1);
		vTaskDelay(100);
		xMessage11.ucFromTask = BREW_TASK;
		xMessage11.iBrewStep = ThisBrewState.ucStep;
		xMessage11.xCommand = BREW_STEP_WAIT;
		xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage11, 0);
	}
	else if (ThisBrewState.ucStep < MAX_BREW_STEPS - 1)
	{
		ThisBrewState.ucStep++;
		vBrewRunStep();
	}
	else
	{
		vConsolePrint("Brew Finished!\r\n\0");
		ThisBrewState.xRunningState = IDLE;
		vBrewResetAndZeroAllStepFlags();
	}
}

void vBrewRunStep(void)
{
	int iFuncReturnValue = 0;
	static char buf[50];

	vBrewReset();
	BrewSteps[ThisBrewState.ucStep].uElapsedTime = 0;
	BrewSteps[ThisBrewState.ucStep].uStartTime = ThisBrewState.uSecondCounter;
	CLEAR_APPLET_CANVAS
	;
	if (BrewSteps[ThisBrewState.ucStep].func)
		BrewSteps[ThisBrewState.ucStep].func((void *) BrewSteps[ThisBrewState.ucStep].iFuncParams);
}



//===================================================================================================================================================
// BREW HLT RELATED METHODS
//===================================================================================================================================================
void vBrewSetHLTStateIdle()
{
	static HltMessage hltMessage;
	hltMessage.command = HLT_CMD_IDLE;
	sprintf(hltMessage.pcTxt, "SetIdle");
	hltMessage.iData1 = 0;
	hltMessage.iData2 = 0;
	hltMessage.fData3 = 0.0;
	hltMessage.fData4 = 0.0;
	xQueueSendToBack(xHltTaskQueue, &hltMessage, portMAX_DELAY);
}

void vBrewHLTSetupFunction(int piParameters[5])
{
	static HltMessage hltMessage;
	MashStageForTempSetpoint mashStageForTempSetpoint = piParameters[1];
	hltMessage.iData1 = 0;
	hltMessage.iData2 = 0;
	hltMessage.fData3 = 0.0;
	hltMessage.fData4 = 0.0;
	hltMessage.command = HLT_CMD_IDLE;
	sprintf(hltMessage.pcTxt, "blah");
	static int iSpargeNumber = 0;
	static double dSpargeSetpoint;
	char buf[50] = "abcdefghijk\r\n";

	switch ((HltCommand) piParameters[0])
	{
		case HLT_CMD_HEAT_AND_FILL:
			{
			hltMessage.command = HLT_CMD_HEAT_AND_FILL;

			switch (mashStageForTempSetpoint)
			{
				case STRIKE:
					{
					sprintf(hltMessage.pcTxt, "HLTSetup-Fill-Strike");
					hltMessage.fData3 = BrewParameters.fStrikeTemp;
					break;
				}
				case MASH_STAGE_2:
				{
					sprintf(hltMessage.pcTxt, "HLTSetup-Fill-Mash2");
					hltMessage.fData3 = BrewParameters.fMashStage2Temp;
					break;
				}
				case MASH_OUT:
					{
					sprintf(hltMessage.pcTxt, "HLTSetup-MashOut");
					hltMessage.fData3 = BrewParameters.fMashOutTemp;
					break;
				}
				case SPARGE:
					{
					dSpargeSetpoint = dGetSpargeSetpoint(iSpargeNumber);
					sprintf(hltMessage.pcTxt, "HLTSetup-Sparge");

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
					sprintf(hltMessage.pcTxt, "HLTSetup-Clean");
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
			switch (mashStageForTempSetpoint)
			{
				case STRIKE:
					{
					if (BrewParameters.fStrikeLitres > BrewParameters.fHLTMaxLitres)
						BrewParameters.fStrikeLitres = BrewParameters.fHLTMaxLitres;
					sprintf(hltMessage.pcTxt, "HLTSetup-DrainStrike");
					hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fStrikeLitres);
					break;
				}
				case MASH_STAGE_2:
				{
					if (BrewParameters.fMashStage2Litres > BrewParameters.fHLTMaxLitres)
						BrewParameters.fMashStage2Litres = BrewParameters.fHLTMaxLitres;
					sprintf(hltMessage.pcTxt, "HLTSetup-DrainMash2");
					hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fMashStage2Litres);
					break;
				}
				case MASH_OUT:
					{
					sprintf(hltMessage.pcTxt, "HLTSetup-DrainMashOut");
					hltMessage.fData3 = dValidateDrainLitres(BrewParameters.fMashOutLitres);
					break;
				}
				case SPARGE:
					{
					sprintf(hltMessage.pcTxt, "HLTSetup-DrainSparge");
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
	hltMessage.ucStepNumber = ThisBrewState.ucStep;
	xQueueSendToBack(xHltTaskQueue, &hltMessage, portMAX_DELAY);

	vBrewNextStep();
}

double dValidateDrainLitres(double parameter)
{
	if (parameter > BrewParameters.fHLTMaxLitres)
		parameter = BrewParameters.fHLTMaxLitres;
	return parameter;
}

//===================================================================================================================================================
// MASH RELATED METHODS
//===================================================================================================================================================
volatile  int iMashTime;
volatile  int iStirTime1;
volatile  int iStirTime2;
volatile  int iPumpTime1;
volatile  int iPumpTime2;
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

void vBrewMashSetupFunction(int piParameters[5])
{
	BrewParameters.uiCurrentMashStage++;
	switch (BrewParameters.uiCurrentMashStage)
	{
		case 1:
		{
			iMashTime = BrewParameters.iMashTime * 60;
			break;
		}
		case 2:
		{
			iMashTime = BrewParameters.iMashStage2Time * 60;
			break;
		}
		default:
		{
			iMashTime = BrewParameters.iMashTime * 60;
			BrewParameters.uiCurrentMashStage = 1;
			break;
		}
	}
	piParameters[2] = 0;
	piParameters[3] = 0;
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

void vMashMixing(int *onCount, int *offCount)
{
	unsigned int elapsed = BrewSteps[ThisBrewState.ucStep].uElapsedTime;
	const unsigned int initialMixingTime = BrewParameters.uiInitialMixingTime * 60;
	const unsigned int clearingTime = BrewParameters.uiClearingTime * 60;
	const unsigned int mixOnTime = BrewParameters.uiMixOnTime * 60;
	const unsigned int mixOffTime = BrewParameters.uiMixOffTime * 60;
	const unsigned int mashTime = BrewParameters.iMashTime * 60;

	int* _onCount = onCount;
	int* _offCount = offCount;
	//static unsigned int  offCount = 0;
	char buf[50];

	StirState stirState = xGetStirState();
	MashPumpState_t mashPumpState = GetMashPumpState();

	if (elapsed > mashTime - clearingTime)
	{
		vStir(STIR_STOPPED);
		vMashPump(START_MASH_PUMP);
	}
	else if (elapsed < initialMixingTime)
	{
		vMashPump(START_MASH_PUMP);
		vStir(STIR_DRIVING);
	}

	else
	{
		if (*offCount < mixOffTime)
		{
			vStir(STIR_STOPPED);
			vMashPump(STOP_MASH_PUMP);
			*_offCount = *_offCount + 1;
		}
		else
		{
			if (*onCount < mixOnTime)
			{
				vMashPump(START_MASH_PUMP);
				vStir(STIR_DRIVING);
				*_onCount = *_onCount + 1;
			}
			else
			{
				*_offCount = 0;
				*_onCount = 0;
			}
		}
	}

}


void vBrewMashPollFunction(int piParameters[5])
{

	int iTimeRemaining = iMashTime;
	iTimeRemaining = iMashTime;

	static char buf[50];
	static int iDisplayCtr = 0;
	iTimeRemaining = iMashTime - BrewSteps[ThisBrewState.ucStep].uElapsedTime;

	if (iDisplayCtr++ == 0)
	{
		sprintf(buf, "Remaining: %d, Elapsed: %d\r\n\0", iTimeRemaining / 60, BrewSteps[ThisBrewState.ucStep].uElapsedTime / 60);
		vConsolePrint(buf);
	}
	else if (iDisplayCtr >= 60)
		iDisplayCtr = 0;

	vRecordNominalTemps(); // save the temp so it can be given to the UI.
	vMashMixing(&piParameters[2], &piParameters[3]); //using params values so it can be reset from the setup function


	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= iMashTime)
	{
		vMashPump(STOP_MASH_PUMP);
		vStir(STIR_STOPPED);
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}
}

void vBrewMashOutSetupFunction(int piParameters[5])
{
	iMashTime = BrewParameters.iMashOutTime * 60;
	iStirTime1 = BrewParameters.iMashOutStirTime1 * 60;
	iStirTime2 = BrewParameters.iMashOutStirTime2 * 60;
	iPumpTime1 = BrewParameters.iMashOutPumpTime1 * 60;
	iPumpTime2 = BrewParameters.iMashOutPumpTime2 * 60;
}

const int iDuty40PctConst = 40; // changed to see if its more stable

void vBrewSpargeSetupFunction(int piParameters[5])
{
	// Diag Variables can be removed after testing
	char buf[50];

	iMashTime = BrewParameters.iSpargeTime * 60;
	iStirTime1 = BrewParameters.iSpargeStirTime1 * 60;
	iStirTime2 = BrewParameters.iSpargeStirTime2 * 60;
	iPumpTime1 = BrewParameters.iSpargePumpTime1 * 60;
	iPumpTime2 = BrewParameters.iSpargePumpTime2 * 60;

	// send message to boil at 40% duty cycle to keep warm
	vConsolePrint("Sparge Checking if boiler has enough water\r\n\0");
	vTaskDelay(200);
	if (IsEnoughWaterInBoilerForHeating())
	{
		sprintf(buf, "Sparge msg to boil to warm at %d%%\r\n\0", iDuty40PctConst);
		vConsolePrint(buf);
		vTaskDelay(200);

		BoilMessage xMessageBoilAt40Pct;
		xMessageBoilAt40Pct.iBrewStep = ThisBrewState.ucStep;
		xMessageBoilAt40Pct.ucFromTask = BREW_TASK_SPARGESETUP;
		xMessageBoilAt40Pct.iDutyCycle = 40;
		if (xBoilQueue != NULL )
			xQueueSendToBack(xBoilQueue, &xMessageBoilAt40Pct, 0);
	}

}

static void vRecordNominalTemps(void)
{
  if ((BrewSteps[ThisBrewState.ucStep].uElapsedTime == 30*60) && strcmp(BrewSteps[ThisBrewState.ucStep].pcStepName, "Mash") == 0)
    {
      fMashTemp = ds1820_get_temp(MASH);
    }
  if ((BrewSteps[ThisBrewState.ucStep].uElapsedTime == 30*60) && strcmp(BrewSteps[ThisBrewState.ucStep].pcStepName, "Mash2") == 0)
      {
        fMashStage2Temp = ds1820_get_temp(MASH);
      }
  else if ((BrewSteps[ThisBrewState.ucStep].uElapsedTime == 5*60) && strcmp(BrewSteps[ThisBrewState.ucStep].pcStepName, "Sparge") == 0)
    {
      fSpargeTemp = ds1820_get_temp(MASH);
    }
  else if ((BrewSteps[ThisBrewState.ucStep].uElapsedTime == 5*60) && strcmp(BrewSteps[ThisBrewState.ucStep].pcStepName, "MashOut") == 0)
    {
      fMashOutTemp = ds1820_get_temp(MASH);
    }
}

//===================================================================================================================================================
// MILL RELATED METHODS
//===================================================================================================================================================
void vBrewMillSetupFunction(int piParameters[5])
{
	vMill(MILL_DRIVING);
}

void vBrewMillPollFunction(int piParameters[5])
{
	int iGrindTime = BrewParameters.iGrindTime * 60;

	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= iGrindTime)
	{
		vMill(STOPPED);
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}
}

//===================================================================================================================================================
// VALVE RELATED METHODS
//===================================================================================================================================================
void vBrewCloseDiscreteValves(int piParameters[5])
{
	vValveActuate(HLT_VALVE, CLOSE_VALVE);
	vValveActuate(MASH_VALVE, CLOSE_VALVE);
	vValveActuate(INLET_VALVE, CLOSE_VALVE);
	vConsolePrint("Valves Setup Func Called\r\n\0");
	vTaskDelay(50);
	BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
	vBrewNextStep();
}

void vBrewBoilValveSetupFunction(int piParameters[5])
{
	BoilValveMessage xMessage;
	xMessage.ucFromTask = BREW_TASK;
	xMessage.iBrewStep = ThisBrewState.ucStep;
	xMessage.xCommand = piParameters[0];
	vConsolePrint("Boil Valve Setup Function called\r\n\0");
	xQueueSendToBack(xBoilValveQueue, &xMessage, 0);
	vBrewNextStep();
}

void vBrewBoilValveCheckClose()
{
	BoilValveMessage xMessage;
	xMessage.ucFromTask = BREW_TASK_CHECK_BOIL_VALVE;
	xMessage.iBrewStep = ThisBrewState.ucStep;
	xMessage.xCommand = BOIL_VALVE_CMD_CLOSE;
	xQueueSendToBack(xBoilValveQueue, &xMessage, 0);
}

void vBrewBoilValveOpen()
{
	BoilValveMessage xMessage;
	xMessage.ucFromTask = BREW_TASK_NO_REPLY;
	xMessage.iBrewStep = ThisBrewState.ucStep;
	xMessage.xCommand = BOIL_VALVE_CMD_OPEN;
	xQueueSendToBack(xBoilValveQueue, &xMessage, 0);
}

//===================================================================================================================================================
// CRANE RELATED METHODS
//===================================================================================================================================================
void vBrewCraneSetupFunction(int piParameters[5])
{
	CraneMessage xMessage;
	xMessage.ucFromTask = BREW_TASK;
	xMessage.iBrewStep = ThisBrewState.ucStep;
	xMessage.xCommand = piParameters[0];
	xQueueSendToBack(xCraneQueue, &xMessage, 0);
	vBrewNextStep();
}

//===================================================================================================================================================
// CHILL RELATED METHODS
//===================================================================================================================================================

void vBrewPreChillSetupFunction(int piParameters[5])
{
	int ii;

	BoilMessage msgStopBoiler;

	if (ucGetBoilState() == BOILING)
	{
		msgStopBoiler.iBrewStep = ThisBrewState.ucStep;
		msgStopBoiler.ucFromTask = BREW_TASK_RESET;
		msgStopBoiler.iDutyCycle = 0;
		if (xBoilQueue != NULL)
		{
			xQueueSendToBack(xBoilQueue, &msgStopBoiler, 5000);
			vConsolePrint("Stopping Boil with duty cycle 0\r\n\0");
		}
	}
	vTaskDelay(200);
	//-------------------------------------------------
	if(xGetBoilValveState() != BOIL_VALVE_CLOSED)
	{
		vBrewBoilValveCheckClose();
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

void vBrewPreChillPollFunction(int piParameters[5])
{
	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= BrewParameters.uiSettlingRecircTime * 60)
	{
		vChillerPump(STOP_CHILLER_PUMP);
	}

	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= BrewParameters.uiSettlingTime * 60)
	{
		vChillerPump(STOP_CHILLER_PUMP); // just in case
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vBrewNextStep();

	}
}

void vBrewChillSetupFunction(int piParameters[5])
{
	vBrewBoilValveCheckClose();
	vValveActuate(CHILLER_VALVE, OPEN_VALVE);
}

void vBrewChillPollFunction(int piParameters[5])
{
	static unsigned int onCounter = 0;
	static unsigned int offCounter = 0;
	if (onCounter < BrewParameters.uiChillingPumpRecircOnTime)
	{
		onCounter++;
		vChillerPump(START_CHILLER_PUMP);
	}
	else
	{
		offCounter++;
		vChillerPump(STOP_CHILLER_PUMP);
		if (offCounter > BrewParameters.uiChillingPumpRecircOffTime)
		{
			offCounter = 0;
			onCounter = 0;
		}
	}


	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= BrewParameters.uiChillTime * 60)
	{
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}
}

//===================================================================================================================================================
// PUMPING OUT
//===================================================================================================================================================
void vBrewPumpToFermenterSetupFunction(int piParameters[5])
{
	vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
	vChillerPump(START_CHILLER_PUMP); //Pump.

	vBrewBoilValveOpen();
}

void vBrewPumpToFermenterPollFunction(int piParameters[5])
{
	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= BrewParameters.uiPumpToFermenterTime * 60)
	{
		vChillerPump(STOP_CHILLER_PUMP); //Pump.
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
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
		vTaskDelay(BrewParameters.iPumpPrimingTime * 1000);
		vMashPump(STOP_MASH_PUMP);
		vTaskDelay(BrewParameters.iPumpPrimingTime * 1000);
	}

}

void vBrewPumpToBoilPollFunction(int piParameters[5])
{
	int iPumpToBoilTime = piParameters[1];
	vMashPump(START_MASH_PUMP);
	vValveActuate(MASH_VALVE, OPEN_VALVE);

	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= iPumpToBoilTime)
	{
		MashTunHasBeenDrained(); //new
		//-------------------------------------------------------------------------------------------
		// ToDo: SHOULD CHECK THE BOIL LEVEL LIMIT HERE SO THAT WE CAN ASSUME THAT THERE WAS NO STUCK SPARGE
		//--------------------------------------------------------------------------------------------
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vMashPump(STOP_MASH_PUMP);
		vValveActuate(MASH_VALVE, CLOSE_VALVE);
		vBrewNextStep();
	}
}

void vBrewWaitingPollFunction(int piParameters[5])
{
	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= piParameters[0])
	{
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		vBrewNextStep();
	}
}

//===================================================================================================================================================
// BOILING
//===================================================================================================================================================
bool IsEnoughWaterInBoilerForHeating()
{
	if (fGetLitresCurrentlyInBoiler() >= 10.0)
	{
		// check boil level probe..
		if (GetBoilerState().level == HIGH)
		{
			return TRUE;
		}
	}
	return FALSE;
}


static struct TextMsg xBoilTextMessage1;
static struct TextMsg * xBoilTextMessage = NULL;

void vBrewBringToBoilSetupFunction(int piParameters[5])
{
	BoilMessage xBoilMessage;
	xBoilTextMessage = &xBoilTextMessage1;
	char buf[50];

	xBoilMessage.iDutyCycle = piParameters[1];
	xBoilMessage.ucFromTask = BREW_TASK_BRING_TO_BOIL;
	xBoilTextMessage->pcMsgText = "Bring To Boil";
	xBoilTextMessage->ucLine = 5;
	xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);


	xBoilMessage.iBrewStep = ThisBrewState.ucStep;
	vConsolePrint("BringToBoil Setup Called\r\n\0");
	xQueueSendToBack(xBoilQueue, &xBoilMessage, 5000);
}

void vBrewBoilSetupFunction(int piParameters[5])
{
	BoilMessage xBoilMessage;
	xBoilTextMessage = &xBoilTextMessage1;
	char buf[50];

	ThisBrewState.ucHopAddition = 0;
	vTaskDelay(1000);


	xBoilMessage.iDutyCycle = piParameters[1];
	xBoilMessage.ucFromTask = BREW_TASK;
	xBoilTextMessage->pcMsgText = "BOIL";
	xBoilTextMessage->ucLine = 5;
	xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
	vValveActuate(MASH_VALVE, CLOSE_VALVE); // Runs water through the other side of the chiller.


	xBoilMessage.iBrewStep = ThisBrewState.ucStep;
	vConsolePrint("Boil Setup Function called\r\n\0");
	xQueueSendToBack(xBoilQueue, &xBoilMessage, 5000);
}

void vPumpToBoilRecycler(int onTimeInSeconds, int offTimeInSeconds)
{
	static int pumpOnCounter = 0;
	static int pumpOffCounter = 0;
	if (pumpOffCounter < offTimeInSeconds)
	{
		vValveActuate(MASH_VALVE, CLOSE_VALVE);
		vMashPump(STOP_MASH_PUMP);
		pumpOffCounter++;
	}
	else
	{
		if (pumpOnCounter < onTimeInSeconds)
		{
			vValveActuate(MASH_VALVE, OPEN_VALVE);
			vMashPump(START_MASH_PUMP);
			pumpOnCounter++;
		}
		else
		{
			pumpOffCounter = 0;
			pumpOnCounter = 0;
		}
	}
}

void vBrewBringToBoilPollFunction(int piParameters[5])
{
	int iBringToBoilTime = BrewParameters.uiBringToBoilTime * 60;
	int iBringTimeRemaining = ((iBringToBoilTime - BrewSteps[ThisBrewState.ucStep].uElapsedTime) / 60);
	static int iLastTime;
	int iBoilDuty = piParameters[1];
	static char buf[50];
	BoilMessage xBoilMessage;
	xBoilMessage.ucFromTask = BREW_TASK;

	vPumpToBoilRecycler(BrewParameters.uiPumpToBoilRecycleOnTime, BrewParameters.uiPumpToBoilRecycleOffTime);
	if (iBringTimeRemaining != iLastTime)
	{
		sprintf(buf, "B2B Time Remaining: %d\r\n\0", iBringTimeRemaining);
		vConsolePrint(buf);
		iLastTime = iBringTimeRemaining;
	}

	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= iBringToBoilTime)
	{
		iBoilDuty = 0;
		xBoilMessage.iDutyCycle = iBoilDuty;
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);
		vBrewNextStep();
	}

}



void vBrewBoilPollFunction(int piParameters[5])
{
	int iBoilTime = BrewParameters.uiBoilTime * 60;
	int iBoilDuty = piParameters[1];
	int iTimeRemaining = ((iBoilTime - BrewSteps[ThisBrewState.ucStep].uElapsedTime) / 60);
	static int iLastTime;
	static char buf[50];
	static int iChillerValveChecked = FALSE;
	BoilMessage xBoilMessage;
	xBoilMessage.ucFromTask = BREW_TASK;

	// if we are boiling and there is 2 min remaining, make sure the boil valve is closed and start pumping around sanitising the chiller etc
	if (iTimeRemaining <= 2)
	{
		if (iChillerValveChecked == FALSE)
		{
			vValveActuate(CHILLER_VALVE, CLOSE_VALVE);
			iChillerValveChecked = TRUE;
		}
		vChillerPump(START_CHILLER_PUMP);
	}

	if (iTimeRemaining != iLastTime)
	{
		sprintf(buf, "Time Remaining: %d\r\n\0", iTimeRemaining);
		vConsolePrint(buf);
		iLastTime = iTimeRemaining;
	}

	if (iTimeRemaining < BrewParameters.uiHopTimes[ThisBrewState.ucHopAddition])
	{
		xQueueSendToBack(xHopsQueue, (void *)1, 0);

		vConsolePrint("Hops Delivered\r\n\0");
		xBoilTextMessage->pcMsgText = "HOPS DELIVERED";
		xBoilTextMessage->ucLine = 6;
		xQueueSendToBack(xBrewAppletTextQueue, &xBoilTextMessage, 0);
		ThisBrewState.ucHopAddition++;
	}

	if (BrewSteps[ThisBrewState.ucStep].uElapsedTime >= iBoilTime)
	{
		iBoilDuty = 0;
		xBoilMessage.iDutyCycle = iBoilDuty;
		BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
		xQueueSendToBack(xBoilQueue, &xBoilMessage, 0);
		vConsolePrint("Boil Completed, sent duty 0 to boil\r\n\0");
		vBrewNextStep();
	}
}

//----------------------------------------------------------------------------------------------------------------------------
// BREW APPLET - Called from menu
//----------------------------------------------------------------------------------------------------------------------------
void vBrewApplet(int init)
{

	if (init)
	{

		lcd_DrawRect(TOP_BANNER_X1, TOP_BANNER_Y1, TOP_BANNER_X2, TOP_BANNER_Y2, Cyan);
		lcd_fill(TOP_BANNER_X1 + 1, TOP_BANNER_Y1 + 1, TOP_BANNER_W, TOP_BANNER_H, Blue);
		lcd_printf(15, 1, 13, "BREW");

		lcd_DrawRect(BUTTON_1_X1, BUTTON_1_Y1, BUTTON_1_X2, BUTTON_1_Y2, Red);
		lcd_fill(BUTTON_1_X1 + 1, BUTTON_1_Y1 + 1, BUTTON_1_W, BUTTON_1_H, Green);
		lcd_DrawRect(BUTTON_2_X1, BUTTON_2_Y1, BUTTON_2_X2, BUTTON_2_Y2, Red);
		lcd_fill(BUTTON_2_X1 + 1, BUTTON_2_Y1 + 1, BUTTON_2_W, BUTTON_2_H, Green);
		lcd_DrawRect(BUTTON_3_X1, BUTTON_3_Y1, BUTTON_3_X2, BUTTON_3_Y2, Red);
		lcd_fill(BUTTON_3_X1 + 1, BUTTON_3_Y1 + 1, BUTTON_3_W, BUTTON_3_H, Green);

		lcd_DrawRect(BUTTON_4_X1, BUTTON_4_Y1, BUTTON_4_X2, BUTTON_4_Y2, Red);
		lcd_fill(BUTTON_4_X1 + 1, BUTTON_4_Y1 + 1, BUTTON_4_W, BUTTON_4_H, Blue);
		lcd_DrawRect(BUTTON_5_X1, BUTTON_5_Y1, BUTTON_5_X2, BUTTON_5_Y2, Red);
		lcd_fill(BUTTON_5_X1 + 1, BUTTON_5_Y1 + 1, BUTTON_5_W, BUTTON_5_H, Blue);
		lcd_DrawRect(BUTTON_6_X1, BUTTON_6_Y1, BUTTON_6_X2, BUTTON_6_Y2, Red);
		lcd_fill(BUTTON_6_X1 + 1, BUTTON_6_Y1 + 1, BUTTON_6_W, BUTTON_6_H, Blue);

		lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, White);
		lcd_fill(BK_X1 + 1, BK_Y1 + 1, BK_W, BK_H, Red);

		lcd_printf(33, 1, 13, "GRAPH"); //Button1
		lcd_printf(33, 5, 13, "STATS_APPLET"); //Button2
		lcd_printf(33, 8, 13, "RESUME"); //Button3
		lcd_printf(30, 13, 10, "QUIT");
		lcd_printf(1, 13, 5, "START"); //Button4
		lcd_printf(9, 13, 5, "PAUSE"); //Button5
		lcd_printf(17, 13, 5, "SKIP"); //Button6

		vConsolePrint("Brew Applet Opened\r\n\0");
		vSemaphoreCreateBinary(xBrewAppletRunningSemaphore);
		if (xBrewTaskStateQueue == NULL && xBrewTaskReceiveQueue == NULL )
		{
			xBrewTaskStateQueue = xQueueCreate(1, sizeof(BrewRunningState));

			xBrewTaskReceiveQueue = xQueueCreate(5, sizeof(BrewMessage));

			if (xBrewTaskStateQueue == NULL || xBrewTaskReceiveQueue == NULL )
				vConsolePrint("FATAL Error creating Brew Task Queues\r\n\0");
			else
				vConsolePrint("Created Brew Task Queues\r\n\0");
		}

		if (xBrewAppletTextQueue == NULL )
		{
			xBrewAppletTextQueue = xQueueCreate(5, sizeof(struct TextMsg *));
			if (xBrewAppletTextQueue == NULL )
				vConsolePrint("FATAL Error creating Applet Text Queue\r\n\0");
			else
				vConsolePrint("Created Applet Text Queue\r\n\0");
		}

		if (xBrewTaskHandle == NULL )
			xTaskCreate( vTaskBrew,
			    ( signed portCHAR * ) "Brew Task",
			    configMINIMAL_STACK_SIZE + 900,
			    NULL,
			    tskIDLE_PRIORITY+2,
			    &xBrewTaskHandle);
		vConsolePrint("Brew Task Created\r\n\0");

		if (xBrewHLTTaskHandle == NULL )
			xTaskCreate( vTaskBrewHLT,
			    ( signed portCHAR * ) "Brew HLT",
			    configMINIMAL_STACK_SIZE + 900,
			    NULL,
			    tskIDLE_PRIORITY,
			    &xBrewHLTTaskHandle);
		vConsolePrint("Brew HLT Task Created\r\n\0");

		xTaskCreate( vBrewAppletDisplay,
		    ( signed portCHAR * ) "Brew Display",
		    configMINIMAL_STACK_SIZE + 200,
		    NULL,
		    tskIDLE_PRIORITY,
		    &xBrewAppletDisplayHandle);

		if (xBrewGraphAppletDisplayHandle == NULL )
		{
			xTaskCreate( vBrewGraphAppletDisplay,
			    ( signed portCHAR * ) "Brew Graph",
			    configMINIMAL_STACK_SIZE,
			    NULL,
			    tskIDLE_PRIORITY,
			    &xBrewGraphAppletDisplayHandle);
			vConsolePrint("Graph Applet Task Created\r\n\0");
		}
		vTaskSuspend(xBrewGraphAppletDisplayHandle);

		if (xBrewStatsAppletDisplayHandle == NULL )
		{
			xTaskCreate( vBrewStatsAppletDisplay,
			    ( signed portCHAR * ) "Brew Stats",
			    configMINIMAL_STACK_SIZE + 100,
			    NULL,
			    tskIDLE_PRIORITY,
			    &xBrewStatsAppletDisplayHandle);
			vConsolePrint("Stats Applet Task Created\r\n\0");
		}

		vTaskSuspend(xBrewStatsAppletDisplayHandle);

		if (xBrewResAppletDisplayHandle == NULL )
		{
			xTaskCreate( vBrewResAppletDisplay,
			    ( signed portCHAR * ) "Brew Resume",
			    configMINIMAL_STACK_SIZE,
			    NULL,
			    tskIDLE_PRIORITY,
			    &xBrewResAppletDisplayHandle);
			vConsolePrint("Resume Applet Task Created\r\n\0");
		}

		vTaskSuspend(xBrewResAppletDisplayHandle);

		vTaskDelay(200);
	}
	else
		vConsolePrint("Leaving Brew Applet\r\n\0");

}

//----------------------------------------------------------------------------------------------------------------------------
// BREW GRAPH SUB-APPLET
//----------------------------------------------------------------------------------------------------------------------------
void vBrewGraphAppletDisplay(void * pvParameters)
{
	int i = 0;
	CLEAR_APPLET_CANVAS
	;
	for (;;)
	{
		vConsolePrint("Graph Display Applet Running\r\n\0");
		vTaskDelay(5000);
	}
}

//----------------------------------------------------------------------------------------------------------------------------
// BREW STATISTICS SUB-APPLET
//----------------------------------------------------------------------------------------------------------------------------
void vBrewStatsAppletDisplay(void * pvParameters)
{

	CLEAR_APPLET_CANVAS
	;
	for (;;)
	{
		vConsolePrint("Stats Display Applet Running\r\n\0");
		HltState hlt = GetHltState();
		BoilerState boiler = GetBoilerState();

		CLEAR_APPLET_CANVAS
		;
		lcd_printf(0, 2, 20, "HLT Level = %s", hlt.levelStr);
		lcd_printf(1, 3, 20, "%s", hlt.drainingStr);
		lcd_printf(1, 4, 20, "%s", hlt.fillingStr);
		lcd_printf(0, 5, 20, "BOILER %s", boiler.boilStateStr);
		lcd_printf(1, 6, 20, "%s", boiler.levelStr);
		lcd_printf(1, 7, 20, "Duty Cycle = %s", boiler.dutyStr);
		lcd_printf(0, 8, 20, "Litres Dlvrd = %d", uiGetActualLitresDelivered());
		lcd_printf(4, 9, 20, "MASH Temp = %d", (int) fGetNominalMashTemp());

		vTaskDelay(1000);
	}
}

void vBrewResAppletDisplay(void * pvParameters)
{
	static unsigned char ucLastStep;
	ucLastStep = 255;
	CLEAR_APPLET_CANVAS
	;
	DRAW_RESUME_BUTTONS
	;
	DRAW_RESUME_BUTTON_TEXT
	;

	for (;;)
	{
		if (ucLastStep != ucResStep)
		{
			CLEAR_APPLET_CANVAS
			;
			if (ucResStep > 0)
				lcd_printf(1, 4, 10, "  STEP %d: %s", ucResStep - 1, BrewSteps[ucResStep - 1].pcStepName);
			lcd_printf(1, 5, 10, "->STEP %d: %s", ucResStep, BrewSteps[ucResStep].pcStepName);
			if (ucResStep < MAX_BREW_STEPS)
				lcd_printf(1, 6, 10, "  STEP %d: %s", ucResStep + 1, BrewSteps[ucResStep + 1].pcStepName);
			vConsolePrint("Resume Display Applet Running\r\n\0");

			ucLastStep = ucResStep;
		}
		vTaskDelay(200);
	}
}

unsigned int uiGetBrewAppletDisplayHWM()
{
	if (xBrewAppletDisplayHandle)
	{
		return uxTaskGetStackHighWaterMark(xBrewAppletDisplayHandle);
	}
	else
		return 0;
}

unsigned int uiGetBrewTaskHWM()
{
	if (xBrewTaskHandle)
	{
		return uxTaskGetStackHighWaterMark(xBrewTaskHandle);
	}
	else
		return 0;
}

unsigned int uiGetBrewGraphAppletHWM()
{
	if (xBrewGraphAppletDisplayHandle)
	{
		return uxTaskGetStackHighWaterMark(xBrewGraphAppletDisplayHandle);
	}
	else
		return 0;
}
unsigned int uiGetBrewStatsAppletHWM()
{
	if (xBrewStatsAppletDisplayHandle)
	{
		return uxTaskGetStackHighWaterMark(xBrewStatsAppletDisplayHandle);
	}
	else
		return 0;
}

unsigned int uiGetBrewResAppletHWM()
{
	if (xBrewResAppletDisplayHandle)
	{
		return uxTaskGetStackHighWaterMark(xBrewResAppletDisplayHandle);
	}
	else
		return 0;
}

void vBrewAppletDisplay(void *pvParameters)
{

	static char tog = 0; //toggles each loop
	//unsigned int uiDecimalPlaces = 3;
	float fHLTTemp = 0, fMashTemp = 0, fAmbientTemp = 0, fCabinetTemp;
	float fHLTTempLast = 1, fMashTempLast = 1, fAmbientTempLast = 1, fCabinetTempLast;
	static char buf[8][40];
	static char b[40], a[40];
	struct TextMsg * RcvdTextMsg;
	RcvdTextMsg = (struct TextMsg *) pvPortMalloc(sizeof(struct TextMsg));
	HltLevel hltLevel;
	static unsigned char ucLastStep = 255;
	static unsigned char ucLastState = 255;
	static HltLevel ucLastHLTLevel = HLT_LEVEL_LOW;
	lcd_printf(0, 10, 40, "|HLT   |MASH  |CAB   |AMB   |");
	lcd_printf(0, 11, 40, "|      |      |      |      |");
	ucLastStep = ThisBrewState.ucStep + 1;
	ucLastState = ThisBrewState.xRunningState - 1;

	for (;;)
	{
		xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
		//take the semaphore so that the key handler wont
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

		if (ucLastStep != ThisBrewState.ucStep && BrewSteps[ThisBrewState.ucStep].pcStepName != NULL )
		{
			sprintf(buf[0], "STEP %d: %s", ThisBrewState.ucStep, BrewSteps[ThisBrewState.ucStep].pcStepName);
			ucLastStep = ThisBrewState.ucStep;
		}
		if (ucLastState != ThisBrewState.xRunningState)
		{
			sprintf(buf[1], "STATE = %s", pcRunningState[ThisBrewState.xRunningState]);
			ucLastState = ThisBrewState.xRunningState;
		}
		if (ucLastHLTLevel != hltLevel)
		{
			sprintf(buf[3], "HLT Level = %s", pcHLTLevels[hltLevel]);
			ucLastHLTLevel = hltLevel;
		}
		sprintf(buf[2], "Elapsed: %02d:%02d Brew %02d:%02d:%02d",
		    ucGetBrewStepMinutesElapsed(), ucGetBrewStepSecondsElapsed(),
		    ucGetBrewHoursElapsed(), ucGetBrewMinutesElapsed(), ucGetBrewSecondsElapsed());

		if (tog)
		{
			CLEAR_APPLET_CANVAS
			;

			if ((fHLTTempLast != fHLTTemp) || (fMashTempLast != fMashTemp) || (fCabinetTempLast != fCabinetTemp) || (fAmbientTempLast != fAmbientTemp))
			{
				lcd_fill(0, 159, 254, 32, Black);
				lcd_printf(0, 10, 40, "|HLT   |MASH  |CAB   |AMB   |");
				lcd_printf(0, 11, 40, "|      |      |      |      |");
				LCD_FLOAT(1, 11, 2, fHLTTemp);
				LCD_FLOAT(8, 11, 2, fMashTemp);
				LCD_FLOAT(15, 11, 2, fCabinetTemp);
				LCD_FLOAT(22, 11, 2, fAmbientTemp);
				fHLTTempLast = fHLTTemp;
				fMashTempLast = fMashTemp;
				fAmbientTempLast = fAmbientTemp;
				fCabinetTempLast = fCabinetTemp;
			}
			lcd_text_xy(1 * 8, 2 * 16, buf[0], Yellow, Blue2);
			lcd_text_xy(1 * 8, 3 * 16, buf[1], Green, Blue2);
			lcd_text_xy(1 * 8, 4 * 16, buf[2], Blue2, Blue2);
			lcd_text_xy(1 * 8, 5 * 16, buf[3], Blue, Blue2);
			if (buf[4] != NULL )
				lcd_text_xy(1 * 8, 6 * 16, buf[4], Grey, Blue2);
			if (buf[5] != NULL )
				lcd_text_xy(1 * 8, 7 * 16, buf[5], Grey, Blue2);
			if (buf[6] != NULL )
				lcd_text_xy(1 * 8, 8 * 16, buf[6], Grey, Blue2);
			if (buf[7] != NULL )
				lcd_text_xy(1 * 8, 9 * 16, buf[7], Grey, Blue2);

			xSemaphoreGive(xBrewAppletRunningSemaphore);
			//give back the semaphore as its safe to return now.
			vTaskDelay(750);

		}
		else
		{
			CLEAR_APPLET_CANVAS
			;
			xSemaphoreGive(xBrewAppletRunningSemaphore);
			//give back the semaphore as its safe to return now.
			vTaskDelay(250);

		}
		static int j = 0;
		int i = uxTaskGetStackHighWaterMark(xBrewTaskHandle);
		sprintf(a, "BrewTask HWM: %d\r\n\0", i);
		if (j != i)
		{
			vConsolePrint(a);
			j = i;
		}
		tog = tog ^ 1;
	}
}

const BrewRunningState xRun = RUNNING;
const BrewRunningState xIdle = IDLE;

void vBrewRemoteStart()
{
	vBrewRunStep();
	xQueueSendToBack(xBrewTaskStateQueue, &xRun, 0);
}

static uint8_t uEvaluateTouch(int xx, int yy)
{
	if (xx > BUTTON_1_X1 + 5 && xx < BUTTON_1_X2 - 5 && yy > BUTTON_1_Y1 + 5 && yy < BUTTON_1_Y2 - 5)
		return BUTTON_1;
	if (xx > BUTTON_2_X1 + 5 && xx < BUTTON_2_X2 - 5 && yy > BUTTON_2_Y1 + 5 && yy < BUTTON_2_Y2 - 5)
		return BUTTON_2;
	if (xx > BUTTON_3_X1 + 5 && xx < BUTTON_3_X2 - 5 && yy > BUTTON_3_Y1 + 5 && yy < BUTTON_3_Y2 - 5)
		return BUTTON_3;
	if (xx > BUTTON_4_X1 + 5 && xx < BUTTON_4_X2 - 5 && yy > BUTTON_4_Y1 + 5 && yy < BUTTON_4_Y2 - 5)
		return BUTTON_4;
	if (xx > BUTTON_5_X1 + 5 && xx < BUTTON_5_X2 - 5 && yy > BUTTON_5_Y1 + 5 && yy < BUTTON_5_Y2 - 5)
		return BUTTON_5;
	if (xx > BUTTON_6_X1 + 5 && xx < BUTTON_6_X2 - 5 && yy > BUTTON_6_Y1 + 5 && yy < BUTTON_6_Y2 - 5)
		return BUTTON_6;
	if (xx > BK_X1 + 5 && xx < BK_X2 - 5 && yy > BK_Y1 + 5 && yy < BK_Y2 - 5)
		return QUIT_BUTTON;

	return NO_BUTTON;
}

int iBrewKey(int xx, int yy)
{
	uint16_t window = 0;
	static uint8_t w = 5, h = 5;
	static uint16_t last_window = 0;
	int iKeyFromApplet = 0;
	uint8_t uButton;

	uButton = uEvaluateTouch(xx, yy);
	static uint8_t iOneShot = 0;
	static unsigned char ucPause = 0;

	if (xx == -1 || yy == -1)
		return 0;

	switch (xAppletState)
	{
		case GRAPH_APPLET:
			{
			//  vConsolePrint("AppletState = GRAPH_APPLET\r\n\0");
			switch (uButton)
			{
				case BUTTON_1:
					{
					xAppletState = GRAPH_APPLET;
					break;
				}
				case BUTTON_2:
					{
					xAppletState = STATS_APPLET;
					break;
				}
				case BUTTON_3:
					{
					xAppletState = RESUME_APPLET;
					ucResStep = ThisBrewState.ucStep;
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
					xAppletState = MAIN_APPLET;

					break;
				}
				case NO_BUTTON:
					{
					break;
				}
			} // switch uButton
			break;
		} // case GRAPH_APPLET
		case STATS_APPLET:
			{
			//vConsolePrint("AppletState = STATS_APPLET\r\n\0");
			switch (uButton)
			{
				case BUTTON_1:
					{
					xAppletState = GRAPH_APPLET;
					break;
				}
				case BUTTON_2:
					{
					xAppletState = STATS_APPLET;
					break;
				}
				case BUTTON_3:
					{
					xAppletState = RESUME_APPLET;
					ucResStep = ThisBrewState.ucStep;
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
					xAppletState = MAIN_APPLET;
					break;
				}
				case NO_BUTTON:
					{
					break;
				}
			} // switch uButton
			break;
		} // case STATS

		case RESUME_APPLET:
			{
			//vConsolePrint("AppletState = RESUME_APPLET\r\n\0");
			switch (uButton)
			{
				case BUTTON_1:
					{
					xAppletState = GRAPH_APPLET;
					break;
				}
				case BUTTON_2:
					{
					xAppletState = STATS_APPLET;
					break;
				}
				case BUTTON_3:
					{
					xAppletState = RESUME_APPLET;
					ucResStep = ThisBrewState.ucStep;
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
						BrewSteps[i].ucComplete = 1;
					}
					ThisBrewState.ucStep = ucResStep;
					vBrewRunStep();
					xQueueSendToBack(xBrewTaskStateQueue, &xRun, 0);
					break;
				}

				case QUIT_BUTTON:
					{
					xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
					vTaskSuspend(xBrewResAppletDisplayHandle);
					CLEAR_APPLET_CANVAS
					;
					DRAW_RESUME_BUTTONS
					;
					DRAW_MAIN_BUTTON_TEXT
					;

					vTaskResume(xBrewAppletDisplayHandle);
					xSemaphoreGive(xBrewAppletRunningSemaphore);
					xAppletState = MAIN_APPLET;
					break;
				}
				case NO_BUTTON:
					{
					break;
				}
			} // switch uButton
			break;
		} // case RESUME_APPLET
		case MAIN_APPLET:
			{
			switch (uButton)
			{
				case BUTTON_1:
					{
					xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
					vTaskSuspend(xBrewAppletDisplayHandle);
					vTaskSuspend(xBrewStatsAppletDisplayHandle);
					vTaskSuspend(xBrewResAppletDisplayHandle);
					CLEAR_APPLET_CANVAS
					;
					vTaskResume(xBrewGraphAppletDisplayHandle);
					xSemaphoreGive(xBrewAppletRunningSemaphore);
					xAppletState = GRAPH_APPLET;

					break;
				}
				case BUTTON_2:
					{
					xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
					vTaskSuspend(xBrewAppletDisplayHandle);
					vTaskSuspend(xBrewGraphAppletDisplayHandle);
					vTaskSuspend(xBrewResAppletDisplayHandle);
					CLEAR_APPLET_CANVAS
					;
					vTaskResume(xBrewStatsAppletDisplayHandle);
					xSemaphoreGive(xBrewAppletRunningSemaphore);
					xAppletState = STATS_APPLET;

					break;
				}
				case BUTTON_3:
					{
					xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
					vTaskSuspend(xBrewAppletDisplayHandle);
					vTaskSuspend(xBrewGraphAppletDisplayHandle);
					vTaskSuspend(xBrewStatsAppletDisplayHandle);

					CLEAR_APPLET_CANVAS
					;
					DRAW_RESUME_BUTTONS
					;
					DRAW_RESUME_BUTTON_TEXT
					;
					vTaskResume(xBrewResAppletDisplayHandle);
					xSemaphoreGive(xBrewAppletRunningSemaphore);

					xAppletState = RESUME_APPLET;
					ucResStep = ThisBrewState.ucStep;
					break;
				}
				case BUTTON_4:
					{
					vBrewRunStep();
					xQueueSendToBack(xBrewTaskStateQueue, &xRun, 0);
					break;
				}
				case BUTTON_5:
					{
					if (ucPause == 0)
					{
						xQueueSendToBack(xBrewTaskStateQueue, &xIdle, 0);
						ucPause = 1;
					}
					else if (ucPause == 1)
					{
						xQueueSendToBack(xBrewTaskStateQueue, &xRun, 0);
						ucPause = 0;
					}

					break;
				}
				case BUTTON_6:
					{
					BrewSteps[ThisBrewState.ucStep].ucComplete = 1;
					if (ThisBrewState.xRunningState == RUNNING)
					{
						vBrewNextStep();
					}
					else
						ThisBrewState.ucStep++;
					break;
				}

				case QUIT_BUTTON:
					{
					xAppletState = QUIT_APPLET;
					xSemaphoreTake(xBrewAppletRunningSemaphore, portMAX_DELAY);
					//given back in QUIT state
					vTaskSuspend(xBrewAppletDisplayHandle);
					CLEAR_APPLET_CANVAS
					;

					lcd_DrawRect(Q_X1, Q_Y1, Q_X2, Q_Y2, White);
					lcd_fill(Q_X1 + 1, Q_Y1 + 1, Q_W, Q_H, Blue);
					lcd_printf(13, 4, 13, "QUIT???");
					lcd_printf(13, 5, 13, "IF YES PRESS");
					lcd_printf(13, 6, 13, "QUIT AGAIN");
					lcd_printf(13, 7, 13, "IF NO, PRESS");
					lcd_printf(13, 8, 13, "HERE");

					break;
				}
				case NO_BUTTON:
					{
					break;
				}

			} // switch uButton
			break;
		} // case STATS_APPLET
		case QUIT_APPLET:
			{

			if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
			{
				if (xBrewAppletDisplayHandle != NULL )
				{
					vTaskDelete(xBrewAppletDisplayHandle);
					vTaskDelay(100);
					xBrewAppletDisplayHandle = NULL;
				}
				vBrewReset_DeleteTasks_ZeroFlags();
				//return the semaphore for taking by another task.
				xSemaphoreGive(xBrewAppletRunningSemaphore);
				xAppletState = MAIN_APPLET;
				vTaskDelay(100);
				return 1;
			}
			else if (xx > Q_X1 && yy > Q_Y1 && xx < Q_X2 && yy < Q_Y2)
			{
				vTaskResume(xBrewAppletDisplayHandle);
				xAppletState = MAIN_APPLET;
			}
			xSemaphoreGive(xBrewAppletRunningSemaphore);
			break;
		} // case QUIT
	} //Switch iAppletState

	vTaskDelay(10);
	return 0;
}


// 11 seconds per litre pumping to the mash.

// NO Mash out Brew Test - WORKS
static BrewStep BrewSteps[] = {
    // TEXT                       SETUP                           		POLL                                   PARMS                          			TIMEOUT   START 	ELAPSED COMPLETE WAIT
    {"Waiting",              NULL,                                	(void *)vBrewWaitingPollFunction , 		{3,0,0,0,0},                          		20,    		 0,      0, 	0, 		0},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,    	NULL,                              		{CRANE_UP,0,0,0,0},                	        25,     	 0,      0, 	0, 		0},
    {"Close D-Valves",       (void *)vBrewCloseDiscreteValves,   	NULL,                              		{0,0,0,0,0},                          		1,      	 0,      0, 	0, 		0},
    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, 	NULL,                             		 {BOIL_VALVE_CMD_CLOSE,0,0,0,0},          	20,     	 0,      0, 	0, 		1},
    {"Fill+Heat:Strike",     (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_HEAT_AND_FILL,STRIKE,0,0,0},   	40*60,  	 0,      0, 	0, 		0},
    {"Grind Grains",         (void *)vBrewMillSetupFunction,      	(void *)vBrewMillPollFunction,     		{0,0,0,0,0},                          		20*60,   	 0,      0, 	0, 		0},
    {"DrainHLTForMash",      (void *)vBrewHLTSetupFunction,       	NULL,                             		 {HLT_CMD_DRAIN,STRIKE,0,0,0},       		5*60,   	 0,      0, 	0, 		1},
    {"Lower Crane",          (void *)vBrewCraneSetupFunction,     	NULL,                              		{CRANE_DOWN_INCREMENTAL,0,0,0,0},   	    2*60,     	 0,      0, 	0, 		1},
    {"Fill+Heat:Mash2",      (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_HEAT_AND_FILL,MASH_STAGE_2,0,0,0}, 40*60,  	 0,      0, 	0, 		0},
    {"Mash1",                (void *)vBrewMashSetupFunction,      	(void *)vBrewMashPollFunction,     		{0,0,0,0,0},                          		60*60,  	 0,      0, 	0, 		0},
    {"DrainHLTForMash2",      (void *)vBrewHLTSetupFunction,       	NULL,                             		{HLT_CMD_DRAIN,MASH_STAGE_2,0,0,0},   		5*60,   	 0,      0, 	0, 		1},
    {"Fill+Heat:Sparge1",    (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_HEAT_AND_FILL,SPARGE,0,0,0},   	40*60,  	 0,      0, 	0, 		1},
    {"Mash2",                (void *)vBrewMashSetupFunction,      	(void *)vBrewMashPollFunction,     		{0,0,0,0,0},                          		60*60,  	 0,      0, 	0, 		0},
    {"MashPumpToBoil",       (void *)vBrewPumpToBoilSetupFunction,	(void *)vBrewPumpToBoilPollFunction,	{0,3*60,0,0,0},                      		11*60,  	 0,      0, 	0, 		1},
    {"DrainForSparge1",      (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_DRAIN,SPARGE,0,0,0},       		5*60,  		 0,      0, 	0, 		1},
    {"Fill+Heat:Sparge2",    (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_HEAT_AND_FILL,SPARGE,0,0,0},   	40*60,  	 0,      0, 	0, 		1},
    {"Sparge1",              (void *)vBrewSpargeSetupFunction,    	(void *)vBrewMashPollFunction,     		{0,0,0,0,0},                          		25*60,   	 0,      0, 	0, 		0},
    {"Pump to boil1",        (void *)vBrewPumpToBoilSetupFunction,	(void *)vBrewPumpToBoilPollFunction,	{0,2*60,0,0,0},                      		11*60,  	 0,      0, 	0, 		1},
    {"DrainForSparge2",      (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_DRAIN,SPARGE,0,0,0},       		5*60,  		 0,      0, 	0, 		1},
    {"Sparge2",              (void *)vBrewSpargeSetupFunction,    	(void *)vBrewMashPollFunction,     		{0,0,0,0,0},                          		25*60,   	 0,      0, 	0, 		1},
    {"Pump to boil2",        (void *)vBrewPumpToBoilSetupFunction,	(void *)vBrewPumpToBoilPollFunction,	{0,2*60,0,0,0},                      		11*60,  	 0,      0, 	0, 		1},
    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     	NULL,                              		{CRANE_UP,0,0,0,0},                         25,     	 0,      0, 	0, 		1},
    {"Fill+Heat:Clean ",     (void *)vBrewHLTSetupFunction,       	NULL,                              		{HLT_CMD_HEAT_AND_FILL, CLEAN,0,0,0},   	40*60,  	 0,      0, 	0, 		0},
    {"BringToBoil",          (void *)vBrewBringToBoilSetupFunction, (void *)vBrewBringToBoilPollFunction, 	{0,100,0,0,0},                       		30*60,  	 0,      0, 	0, 		0},
    {"Pump to boil",         (void *)vBrewPumpToBoilSetupFunction,	(void *)vBrewPumpToBoilPollFunction,	{0,10,0,0,0},                        		2*60,  	  	 0,      0, 	0, 		1},
    {"Boil",                 (void *)vBrewBoilSetupFunction,      	(void *)vBrewBoilPollFunction ,    		{0,55,0,0,0},                        		90*60,  	 0,      0, 	0, 		1},
    {"SettlingBefChill",     (void *)vBrewPreChillSetupFunction,  	(void *)vBrewPreChillPollFunction,  	{1*60,0,0,0,0},                      		10*60,   	 0,      0, 	0, 		1},
    {"Chill",                (void *)vBrewChillSetupFunction,     	(void *)vBrewChillPollFunction ,   		{0,0,0,0,0},                          		10*60,  	 0,      0, 	0, 		1},
    {"Pump Out",    (void *)vBrewPumpToFermenterSetupFunction,     	(void *)vBrewPumpToFermenterPollFunction,{5,0,0,0,0},                    			10*60,  	 0,      0, 	0, 		1},
    {"BREW FINISHED",        NULL,                                	(void *)vBrewWaitingPollFunction,  		{1,0,0,0,0},                          		2,      	 0,      0, 	0, 		1},
    {NULL,                   NULL,                                	NULL,                              		{0,0,0,0,0},                          		0,      	 0,      0, 	0, 		0}
};

//static BrewStep BrewSteps[] = {
//    // TEXT                       SETUP                           POLL                                   PARMS                          TIMEOUT   START ELAPSED COMPLETE WAIT
//    {"Waiting",              NULL,                                (void *)vBrewWaitingPollFunction , {3,0,0,0,0},                          20,     0,      0, 0, 0},
//    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {CRANE_UP,0,0,0,0},                         25,     0,      0, 0, 0},
//    {"Close D-Valves",       (void *)vBrewCloseDiscreteValves,    NULL,                              {0,0,0,0,0},                          1,      0,      0, 0, 0},
//    {"Close BoilValve",      (void *)vBrewBoilValveSetupFunction, NULL,                              {BOIL_VALVE_CMD_CLOSE,0,0,0,0},           20,      0,      0, 0, 1},
//    {"Fill+Heat:Strike",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,CLEAN,0,0,0},   40*60,  0,      0, 0, 0},
//    {"DrainHLTForMash",      (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,STRIKE,0,0,0},       5*60,   0,      0, 0, 1},
//    {"Lower Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {CRANE_DOWN_INCREMENTAL,0,0,0,0},                     2*60,     0,      0, 0, 1},
//    {"Fill+Heat:Sparge1",    (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,CLEAN,0,0,0},   40*60,  0,      0, 0, 1},
//    {"Mash",                 (void *)vBrewMashSetupFunction,      (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          60*60,  0,      0, 0, 0},
//    {"MashPumpToBoil",        (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,2*60,0,0,0},                      11*60,  0,      0, 0, 0},
//    {"DrainForSparge1",       (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,SPARGE,0,0,0},       5*60,  0,      0, 0, 1},
//    {"Fill+Heat:Sparge2",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL,CLEAN,0,0,0},   40*60,  0,      0, 0, 1},
//    {"Sparge1",               (void *)vBrewSpargeSetupFunction,    (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          25*60,   0,      0, 0, 0},
//    {"Pump to boil1",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,1*60,0,0,0},                      11*60,  0,      0, 0, 0},
//    {"DrainForSparge2",       (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_DRAIN,SPARGE,0,0,0},       5*60,  0,      0, 0, 1},
//    {"Sparge2",               (void *)vBrewSpargeSetupFunction,    (void *)vBrewMashPollFunction,     {0,0,0,0,0},                          25*60,   0,      0, 0, 1},
//    {"Pump to boil2",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,1*60,0,0,0},                      11*60,  0,      0, 0, 1},
//    {"Raise Crane",          (void *)vBrewCraneSetupFunction,     NULL,                              {CRANE_UP,0,0,0,0},                         25,     0,      0, 0, 1},
//    {"Fill+Heat:Clean ",     (void *)vBrewHLTSetupFunction,       NULL,                              {HLT_CMD_HEAT_AND_FILL, CLEAN,0,0,0},   40*60,  0,      0, 0, 0},
//    {"BringToBoil",          (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {5,100,0,0,0},                       30*60,  0,      0, 0, 0},
//    {"Pump to boil",         (void *)vBrewPumpToBoilSetupFunction,(void *)vBrewPumpToBoilPollFunction,{0,10,0,0,0},                        2*60,  0,      0, 0, 1},
//    {"Boil",                 (void *)vBrewBoilSetupFunction,      (void *)vBrewBoilPollFunction ,    {60,55,1,0,0},                        90*60,  0,      0, 0, 1},
//    {"SettlingBefChill",     (void *)vBrewPreChillSetupFunction,  (void *)vBrewPreChillPollFunction,  {1*60,0,0,0,0},                      10*60,   0,      0, 0, 1},
//    {"Pump Out",    (void *)vBrewPumpToFermenterSetupFunction,     (void *)vBrewPumpToFermenterPollFunction ,   {8,0,0,0,0},                          10*60,  0,      0, 0, 1},
//    {"BREW FINISHED",        NULL,                                (void *)vBrewWaitingPollFunction,  {1,0,0,0,0},                          2,      0,      0, 0, 0},
//    {NULL,                   NULL,                                NULL,                              {0,0,0,0,0},                          0,      0,      0, 0, 0}
//};

 int iMaxBrewSteps(void)
{
 return ARRAY_LENGTH(BrewSteps) - 1; // take 1 for the null ending struct
}




