/*
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_it.h"

#include "adc.h"
#include "ds1820.h"
#include "hlt.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "task.h"
#include "semphr.h"
#include "message.h"
#include "valves.h"
#include "brew.h"
#include "mill.h"
#include "console.h"
#include "Flow1.h"
#include "main.h"
#include "hlt.h"
#include "MashWater.h"

volatile char hlt_state = OFF;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xHLTAppletRunningSemaphore;
xTaskHandle xHeatHLTTaskHandle = NULL, xHLTAppletDisplayHandle = NULL;
xQueueHandle xHltTaskQueue = NULL;
xTaskHandle xBrewHLTTaskHandle = NULL;
xTaskHandle xTaskHLTLevelCheckerTaskHandle = NULL;

volatile float diag_setpoint = 74.5; // when calling the heat_hlt task, we use this value instead of the passed parameter.
#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%0d.%0d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));

const char * pcHltCommands[3] = {"Idle", "Fill and Heat", "Drain"};
const char * pcHltLevels[3] =  {"HLT-Low", "HLT-Medium", "HLT-High"};

static float fGetStableHltTemp(float tolerance);
bool xTickTimer(portTickType * ticksToWait, portTickType setpoint);
HltLevel xGetHltLevel();

unsigned int uiGetHltTemp()
{
  return (unsigned int)ds1820_get_temp(HLT);
}

void vPrintHltMessage(HltMessage msg)
{
	static char buf[80];

	if (msg.pcTxt != NULL)
	{
		sprintf(buf, "From: %s\r\n", msg.pcTxt);
		vConsolePrint(buf );
		vTaskDelay(50);
	}
	sprintf(buf, "Command: %s\r\n", pcHltCommands[msg.command]);
	vConsolePrint(buf );vTaskDelay(50);

	sprintf(buf, "Data1: %d\r\n", msg.iData1);
			vConsolePrint(buf );vTaskDelay(50);

	sprintf(buf, "Data2: %d\r\n", msg.iData2);
	vConsolePrint(buf );vTaskDelay(50);

	sprintf(buf, "Data3: ~%d\r\n", (int)msg.fData3);
	vConsolePrint(buf );vTaskDelay(50);

	sprintf(buf, "Data4: ~%d\r\n", (int)msg.fData4);
	vConsolePrint(buf );vTaskDelay(50);

	sprintf(buf, "Step: %d\r\n", (int)msg.ucStepNumber);
	vConsolePrint(buf );vTaskDelay(50);
}

void vHLTAppletDisplay(void *pvParameters);

void hlt_init()
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_SSR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Output - Push Pull
  GPIO_Init(HLT_SSR_PORT, &GPIO_InitStructure);
  GPIO_ResetBits(HLT_SSR_PORT, HLT_SSR_PIN );

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_LEVEL_CHECK_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Input Pulled-up
  GPIO_Init(HLT_LEVEL_CHECK_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = HLT_HIGH_LEVEL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Input Pulled-up
  GPIO_Init(HLT_HIGH_LEVEL_PORT, &GPIO_InitStructure);
  adc_init(); //initialise the ADC1 so we can use a channel from it for the hlt level

  vSemaphoreCreateBinary(xHLTAppletRunningSemaphore);
  if (xHltTaskQueue == NULL)
  {
	  xHltTaskQueue = xQueueCreate(5, sizeof(HltMessage));
	  if (xHltTaskQueue == NULL)
		  vConsolePrint("FATAL Error creating HLT Task Queue\r\n");
	  else vConsolePrint("Created Brew HLT Task Queues\r\n");
  }

}

static unsigned int uiActualLitresDelivered  = 0;
unsigned int uiGetActualLitresDelivered(void)
{
  return uiActualLitresDelivered;
}

static char cMaintainHighLevelHlt() {
	char cSetpointReached = 0;
	if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN )) // looking for zero volts
	{
		vTaskDelay(2000);
		vValveActuate(INLET_VALVE, CLOSE_VALVE);
		cSetpointReached = 1;
	}
	else
	{
		vValveActuate(INLET_VALVE, OPEN_VALVE);
		cSetpointReached = 0;
	}
	return cSetpointReached;
}

static char cMaintainTempHlt(float fTempSetpoint)
{
	float currentHltTemp = 0.0;
	char cSetpointReached = 0;
	// ensure we have water above the elements
	if (xGetHltLevel() == HLT_LEVEL_MID || xGetHltLevel() == HLT_LEVEL_HIGH)
	{
		currentHltTemp = fGetStableHltTemp(2.0); //delays for approx 1.8 seconds

		if (currentHltTemp < fTempSetpoint)
		{
			GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 1);
		}
		else if (currentHltTemp > fTempSetpoint + 0.1)
		{
			cSetpointReached = 1;
			GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
		}
	}
	else //DONT HEAT... WE WILL BURN OUT THE ELEMENT
	{
		GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
		cSetpointReached = 0;
	}
	return cSetpointReached;
}

//=================================================================================================================================================================
void
vTaskBrewHLT(void * pvParameters)
{
	portTickType xLastWakeTime;

	char buf[50];
	xLastWakeTime = xTaskGetTickCount();
	static uint8_t uFirst = 0;
	float fTempSetpoint = 0.0;
	static float fLitresToDrain = 0.0;
	float fActualLitresDelivered = 0.0;
	portTickType xTaskDelayMillis = 200;
	portTickType xTaskTicksPerSecond = 1000 / xTaskDelayMillis;
	int iValveCloseDelaySeconds = 2;
	static portTickType xTicksToWait = 0;
	bool IsValveOpening = FALSE;
	//float currentHltTemp = 0.0;

	struct TextMsg * NewMessage = (struct TextMsg *) pvPortMalloc(sizeof(struct TextMsg));
	struct GenericMessage * xMsgCmdCompleteToBrew = (struct GenericMessage *) pvPortMalloc(sizeof(struct GenericMessage));

	char pcMessageText[40];

	static unsigned char ucStep = 0;
	static unsigned char ucHeatAndFillMessageSent = 0;
	static unsigned char ucLitresDeliveredDisplayCtr = 0;
	char cHltLevelReached = 0;
	char cHltTempReached = 0;

	const int STEP_COMPLETE = 40;
	const int STEP_FAILED = 41;
	const int STEP_TIMEOUT = 45;

	HltMessage rcvdMsg;

	for (;;)
	{
		if (xQueueReceive(xHltTaskQueue, &rcvdMsg, 0) == pdPASS)
		{
			vPrintHltMessage(rcvdMsg);
			uFirst = 1;
			ucHeatAndFillMessageSent = 0;
			ucStep = rcvdMsg.ucStepNumber;
		}
		else
			uFirst = 0;

		switch (rcvdMsg.command)
		{
			case HLT_CMD_IDLE:
				{
				if (uFirst)
				{
					vConsolePrint("HLT Entered IDLE State\r\n");
					vValveActuate(INLET_VALVE, CLOSE_VALVE);
					vValveActuate(HLT_VALVE, CLOSE_VALVE);
					GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
				}
				break;
			}
			case HLT_CMD_HEAT_AND_FILL:
				{
				if (uFirst)
				{
					fTempSetpoint = rcvdMsg.fData3;
					vConsolePrint("HLT Entered HEAT AND FILL State\r\n");
					BrewState.ucHLTState = HLT_STATE_FILL_HEAT;
					sprintf(pcMessageText, "Heating to %02d.%02d Deg", (unsigned int) floor(fTempSetpoint), (unsigned int) ((fTempSetpoint - floor(fTempSetpoint)) * pow(10, 2)));
					NewMessage->pcMsgText = pcMessageText;
					NewMessage->ucLine = 4;
					xQueueSendToBack(xBrewAppletTextQueue, &NewMessage, 0);
				}
				// make sure the HLT valve is closed
				vValveActuate(HLT_VALVE, CLOSE_VALVE);

				cHltLevelReached = cMaintainHighLevelHlt();

				cHltTempReached = cMaintainTempHlt(fTempSetpoint);
				if (cHltLevelReached == 1 && cHltTempReached == 1)
				{
					if (ucHeatAndFillMessageSent == 0)
					{
						sprintf(buf, "HLT: Temp and level reached, sending msg\r\n");
						vConsolePrint(buf);
						BrewState.ucHLTState = HLT_STATE_AT_TEMP;
						xMsgCmdCompleteToBrew->ucFromTask = HLT_TASK;
						xMsgCmdCompleteToBrew->ucToTask = BREW_TASK;
						xMsgCmdCompleteToBrew->uiStepNumber = rcvdMsg.ucStepNumber;
						xMsgCmdCompleteToBrew->pvMessageContent = (void *) &STEP_COMPLETE;
						xQueueSendToBack(xBrewTaskReceiveQueue, &xMsgCmdCompleteToBrew, 0);
						ucHeatAndFillMessageSent = 1;
					}
				}

				break;

			}
			case HLT_CMD_DRAIN:
				{
				if (uFirst)
				{
					GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
					fLitresToDrain = rcvdMsg.fData3;
					vConsolePrint("HLT: Entered DRAIN State\r\n");
					// Need to set up message to the Applet.
					LCD_FLOAT(10, 10, 1, fTempSetpoint);
					lcd_printf(1, 10, 10, "Setpoint:");
					vValveActuate(HLT_VALVE, OPEN_VALVE);
					IsValveOpening = TRUE;
					vResetFlow1();
					BrewState.ucHLTState = HLT_STATE_DRAIN;

				}
				if (IsValveOpening == TRUE) // cheap timer
				{
					fActualLitresDelivered = fGetBoilFlowLitres();

					if (xTicksToWait++ >= (iValveCloseDelaySeconds * xTaskTicksPerSecond))
					{
						xTicksToWait = 0;
						vResetFlow1();
						IsValveOpening = FALSE;
					}
				}
				else
				{
					vValveActuate(HLT_VALVE, OPEN_VALVE);
					fActualLitresDelivered = fGetBoilFlowLitres();

					if (ucLitresDeliveredDisplayCtr++ >= 5 * xTaskTicksPerSecond) // every 5 seconds.
					{
						sprintf(buf, "Delivered = %dml to Mash Tun\r\n", (int) (fActualLitresDelivered * 1000));
						vConsolePrint(buf);
						ucLitresDeliveredDisplayCtr = 0;
					}
					if (fActualLitresDelivered >= fLitresToDrain)
					{
						vConsolePrint("HLT is DRAINED\r\n");
						vValveActuate(HLT_VALVE, CLOSE_VALVE);
						vTaskDelay(500);
						uiActualLitresDelivered += (unsigned int) (fActualLitresDelivered * 1000);
						MashTunFillingSetpointReached(fActualLitresDelivered);
						xMsgCmdCompleteToBrew->ucFromTask = HLT_TASK;
						xMsgCmdCompleteToBrew->ucToTask = BREW_TASK;
						xMsgCmdCompleteToBrew->uiStepNumber = rcvdMsg.ucStepNumber;
						xMsgCmdCompleteToBrew->pvMessageContent = (void *) &STEP_COMPLETE;
						xQueueSendToBack(xBrewTaskReceiveQueue, &xMsgCmdCompleteToBrew, 0);
						rcvdMsg.command = HLT_CMD_IDLE;
						BrewState.ucHLTState = HLT_STATE_IDLE;
					}
				}
				break;
			}

		}
		vTaskDelayUntil(&xLastWakeTime, xTaskDelayMillis / portTICK_RATE_MS);

	}
}

bool xTickTimer(portTickType * ticksToWait, portTickType setpoint)
{
  if (*ticksToWait++ >= setpoint)
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

HltLevel xGetHltLevel()
{
  if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN ))
    return HLT_LEVEL_HIGH;
  if (!GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN)
      && GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN ))
    return HLT_LEVEL_MID;
  else return HLT_LEVEL_LOW;
}

static float fGetStableHltTemp(float tolerance)
{
	static float LastTemp = 0.0;
	float temp1 = ds1820_get_temp(HLT);
	vTaskDelay(900);
	float temp2 = ds1820_get_temp(HLT);
	vTaskDelay(900);
	float temp3 = ds1820_get_temp(HLT);
	if (temp1 < temp2 + tolerance && temp1 > temp2 - tolerance)
	{
		// temp1 and 2 are similar
		if (temp2 < temp3 + tolerance && temp2 > temp3 - tolerance)
		{
			// temp 2 and temp 3 are similar
			LastTemp = (temp1 + temp2 + temp3)/3;
			return LastTemp;
		}
	}
	return LastTemp;
}

HltState GetHltState()
{
  HltState S;
  S.level = xGetHltLevel();
  if (S.level == HLT_LEVEL_HIGH)
    sprintf(S.levelStr, "HLT Level HIGH");
  else if (S.level == HLT_LEVEL_MID)
    sprintf(S.levelStr, "HLT Level MID");
  else
    sprintf(S.levelStr, "HLT Level Low");

  S.temp_float = ds1820_get_temp(HLT);
  S.temp_int = uiGetHltTemp();
  S.filling = ucGetInletValveState();
  S.draining = ucGetHltValveState();
  if (S.filling == VALVE_OPENED)
    sprintf(S.fillingStr, "HLT Filling");
  else
    sprintf(S.fillingStr, "HLT NOT Filling");
  if (S.draining == VALVE_OPENED)
    sprintf(S.drainingStr, "HLT Draining");
  else
    sprintf(S.drainingStr, "HLT NOT Draining");
  S.heatingState = xGetHltHeatingState();
  return S;
}

HltHeatingState xGetHltHeatingState()
{
	if (GPIO_ReadInputDataBit(HLT_SSR_PORT, HLT_SSR_PIN))
		return HLT_HEATING;
	return HLT_NOT_HEATING;
}

//=================================================================================================================================================================

void vTaskHLTLevelChecker( void * pvParameters)
{
  HltLevel level;
  HltHeatingState heatingState;
  int delay = 500;

  for (;;)
    {
	  level = xGetHltLevel();
	  heatingState = xGetHltHeatingState();
     if (level == HLT_LEVEL_LOW && heatingState == HLT_HEATING)
     {
          level = xGetHltLevel();
          heatingState = xGetHltHeatingState();
          if (level == HLT_LEVEL_HIGH  && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN))
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              vConsolePrint("HLT Level Check Task: INTERVENED\r\n");
              vConsolePrint("HLT Level Check Task: SSR on while level low!\r\n");
            }
        }
      if (level == HLT_LEVEL_HIGH  && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN))
        {
          vTaskDelay(3000);
          level = xGetHltLevel();
          if (level == HLT_LEVEL_HIGH  && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN))
            {
              vValveActuate(INLET_VALVE, CLOSE_VALVE);
              vConsolePrint("HLT Level Check Task: INTERVENED\r\n");
              vConsolePrint("INLET OPENED while level HIGH for >3s!\r\n");
              vTaskPrioritySet(NULL,  tskIDLE_PRIORITY + 5);
              while(1);
            }
        }
      vTaskDelay(delay);
    }
}

void vTaskHeatHLT(void * pvParameters)
{
	float * setpoint = (float *) pvParameters;
	float actual = 0.0;
	uint16_t hlt_level = 0;
	HltLevel hltLevel = HLT_LEVEL_LOW;
	char hlt_ok = 0;
	//choose which value we use for the setpoint.
	if (setpoint == NULL )
		setpoint = (float *) &diag_setpoint;

	for (;;)
	{
		hltLevel = xGetHltLevel();
		if (hltLevel !=  HLT_LEVEL_LOW)
		{
			actual = fGetStableHltTemp(2.0);

			if (actual < *setpoint)
			{
				GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 1);
				hlt_state = HEATING;
			}
			else
			{
				GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
				hlt_state = OFF;
			}
		}
		else
		{
			vConsolePrint("Manual HLT: Level LOW, heating off!\r\n");
			GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
			hlt_state = OFF;
		}
		vTaskDelay(500/portTICK_RATE_MS); // 500ms delay is fine for heating
	}
}

#define SETPOINT_UP_X1 0
#define SETPOINT_UP_Y1 30
#define SETPOINT_UP_X2 100
#define SETPOINT_UP_Y2 100
#define SETPOINT_UP_W (SETPOINT_UP_X2-SETPOINT_UP_X1)
#define SETPOINT_UP_H (SETPOINT_UP_Y2-SETPOINT_UP_Y1)

#define SETPOINT_DN_X1 0
#define SETPOINT_DN_Y1 105
#define SETPOINT_DN_X2 100
#define SETPOINT_DN_Y2 175
#define SETPOINT_DN_W (SETPOINT_DN_X2-SETPOINT_DN_X1)
#define SETPOINT_DN_H (SETPOINT_DN_Y2-SETPOINT_DN_Y1)

#define START_HEATING_X1 155
#define START_HEATING_Y1 30
#define START_HEATING_X2 300
#define START_HEATING_Y2 100
#define START_HEATING_W (START_HEATING_X2-START_HEATING_X1)
#define START_HEATING_H (START_HEATING_Y2-START_HEATING_Y1)

#define STOP_HEATING_X1 155
#define STOP_HEATING_Y1 105
#define STOP_HEATING_X2 300
#define STOP_HEATING_Y2 175
#define STOP_HEATING_W (STOP_HEATING_X2-STOP_HEATING_X1)
#define STOP_HEATING_H (STOP_HEATING_Y2-STOP_HEATING_Y1)

#define BAK_X1 200
#define BAK_Y1 190
#define BAK_X2 315
#define BAK_Y2 235
#define BAK_W (BAK_X2-BAK_X1)
#define BAK_H (BAK_Y2-BAK_Y1)

void vHLTApplet(int init)
{
  if (init)
    {
      lcd_DrawRect(SETPOINT_UP_X1, SETPOINT_UP_Y1, SETPOINT_UP_X2,
          SETPOINT_UP_Y2, Red);
      lcd_fill(SETPOINT_UP_X1 + 1, SETPOINT_UP_Y1 + 1, SETPOINT_UP_W,
          SETPOINT_UP_H, Blue);
      lcd_DrawRect(SETPOINT_DN_X1, SETPOINT_DN_Y1, SETPOINT_DN_X2,
          SETPOINT_DN_Y2, Red);
      lcd_fill(SETPOINT_DN_X1 + 1, SETPOINT_DN_Y1 + 1, SETPOINT_DN_W,
          SETPOINT_DN_H, Blue);
      lcd_DrawRect(STOP_HEATING_X1, STOP_HEATING_Y1, STOP_HEATING_X2,
          STOP_HEATING_Y2, Cyan);
      lcd_fill(STOP_HEATING_X1 + 1, STOP_HEATING_Y1 + 1, STOP_HEATING_W,
          STOP_HEATING_H, Red);
      lcd_DrawRect(START_HEATING_X1, START_HEATING_Y1, START_HEATING_X2,
          START_HEATING_Y2, Cyan);
      lcd_fill(START_HEATING_X1 + 1, START_HEATING_Y1 + 1, START_HEATING_W,
          START_HEATING_H, Green);
      lcd_DrawRect(BAK_X1, BAK_Y1, BAK_X2, BAK_Y2, Cyan);
      lcd_fill(BAK_X1 + 1, BAK_Y1 + 1, BAK_W, BAK_H, Magenta);
      lcd_printf(10, 1, 18, "MANUAL HLT APPLET");
      lcd_printf(3, 4, 11, "SP UP");
      lcd_printf(1, 8, 13, "SP DOWN");
      lcd_printf(22, 4, 13, "START HEATING");
      lcd_printf(22, 8, 12, "STOP HEATING");
      lcd_printf(30, 13, 4, "Back");

      //create a dynamic display task
      vConsolePrint("Creating HLT_Display Task!\r\n");
      xTaskCreate( vHLTAppletDisplay, ( signed portCHAR * ) "hlt_disp",
          configMINIMAL_STACK_SIZE + 200, NULL, tskIDLE_PRIORITY,
          &xHLTAppletDisplayHandle);

    }
  else
    vConsolePrint("n\r\n");
 }



void vHLTAppletDisplay(void *pvParameters)
{
  static char tog = 0; //toggles each loop
 // unsigned char  ucHLTLevel;
  HltState hltState;
  float diag_setpoint1; // = diag_setpoint;
  //float current_temp;
  uint8_t ds10;
  char hlt_ok = 0;
  for (;;)
    {

      xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);

      hltState = GetHltState();
      diag_setpoint1 = diag_setpoint;
      lcd_fill(1, 178, 170, 40, Black);
      lcd_printf(1, 11, 20, pcHltLevels[hltState.level]);
      //display the state and user info (the state will flash on the screen)
      switch (hltState.heatingState)
        {
      case HLT_HEATING:
        {
          if (tog)
            {
              lcd_fill(1, 220, 180, 29, Black);
              lcd_printf(1, 13, 15, "HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(hltState.temp_float),
                  (unsigned int) ((hltState.temp_float - floor(hltState.temp_float))
                      * pow(10, 3)));
              //lcd_printf(1,14,15,"Currently = %2.1fdegC", current_temp);
            }
          else
            {
              lcd_fill(1, 210, 180, 17, Black);
            }
          break;
        }
      case HLT_NOT_HEATING:
        {
          if (tog)
            {
              lcd_fill(1, 210, 180, 29, Black);
              lcd_printf(1, 13, 11, "NOT HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(hltState.temp_float),
                  (unsigned int) ((hltState.temp_float - floor(hltState.temp_float))
                      * pow(10, 3)));
              //lcd_printf(1,14,15,"Currently = %2.1fdegC", current_temp);
            }
          else
            {
              lcd_fill(1, 210, 180, 17, Black);
            }

          break;
        }
      default:
        {
          break;
        }
        }

      tog = tog ^ 1;
      lcd_fill(102, 99, 35, 10, Black);
      //printf("%d, %d, %d\r\n", (uint8_t)diag_setpoint, (diag_setpoint), ((uint8_t)diag_setpoint*10)%5);
      lcd_printf(13, 6, 25, "%d.%d", (unsigned int) floor(diag_setpoint),
          (unsigned int) ((diag_setpoint - floor(diag_setpoint)) * pow(10, 3)));
      //lcd_printf(13,6,15,"%d", (int)diag_setpoint);

      xSemaphoreGive(xHLTAppletRunningSemaphore);
      //give back the semaphore as its safe to return now.
      vTaskDelay(500);

    }
}

void
vHLTAppletCallback(int in_out)
{
//printf(" Called in_out = %d\r\n", in_out);

}
int
HLTKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5, h = 5;
  static uint16_t last_window = 0;
  if (xx > SETPOINT_UP_X1 + 1 && xx < SETPOINT_UP_X2 - 1
      && yy > SETPOINT_UP_Y1 + 1 && yy < SETPOINT_UP_Y2 - 1)
    {
      diag_setpoint += 0.5;
      //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
    }
  else if (xx > SETPOINT_DN_X1 + 1 && xx < SETPOINT_DN_X2 - 1
      && yy > SETPOINT_DN_Y1 + 1 && yy < SETPOINT_DN_Y2 - 1)

    {
      diag_setpoint -= 0.5;
      //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
    }
  else if (xx > STOP_HEATING_X1 + 1 && xx < STOP_HEATING_X2 - 1
      && yy > STOP_HEATING_Y1 + 1 && yy < STOP_HEATING_Y2 - 1)
    {
      //printf("Deleting Heating Task\r\n");
      if (xHeatHLTTaskHandle != NULL )
        {
          vTaskDelete(xHeatHLTTaskHandle);
          xHeatHLTTaskHandle = NULL;

        }
      GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
      hlt_state = OFF;
    }
  else if (xx > START_HEATING_X1 + 1 && xx < START_HEATING_X2 - 1
      && yy > START_HEATING_Y1 + 1 && yy < START_HEATING_Y2 - 1)
    {
      //printf("Checking if task already running...\r\n");
      if (xHeatHLTTaskHandle == NULL )
        {
          // printf("No previous HLT Heating task\r\n");
          vConsolePrint("Creating MANUAL HLT Task\r\n");
          xTaskCreate( vTaskHeatHLT, ( signed portCHAR * ) "HLT_HEAT",
              configMINIMAL_STACK_SIZE +300, NULL, tskIDLE_PRIORITY+1,
              &xHeatHLTTaskHandle);
        }
      else
        {
          //printf("Heating task already running\r\n");
        }
    }
  else if (xx > BAK_X1 && yy > BAK_Y1 && xx < BAK_X2 && yy < BAK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xHLTAppletDisplayHandle != NULL )
        {
          vTaskDelete(xHLTAppletDisplayHandle);
          vTaskDelay(100);
          xHLTAppletDisplayHandle = NULL;
        }
      //delete the heating task
      if (xHeatHLTTaskHandle != NULL )
        {
          // commented out for test so can make beer!
          //vTaskDelete(xHeatHLTTaskHandle);
          //vTaskDelay(100);
          //xHeatHLTTaskHandle = NULL;
        }
      //return the semaphore for taking by another task.
      xSemaphoreGive(xHLTAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}

