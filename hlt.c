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
#include "button.h"
#include "macros.h"


volatile char hlt_state = OFF;
volatile char manual_hlt_command = OFF;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xHLTAppletRunningSemaphore;
xTaskHandle xHeatHLTTaskHandle = NULL, xHLTAppletDisplayHandle = NULL;
xQueueHandle xHltTaskQueue = NULL;
xTaskHandle xBrewHLTTaskHandle = NULL;
xTaskHandle xTaskHLTLevelCheckerTaskHandle = NULL;
volatile float diag_setpoint = 74.5; // when calling the heat_hlt task, we use this value instead of the passed parameter.
#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%0d.%0d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));
const char * pcHltCommands[3] = { "Idle", "Fill and Heat", "Drain" };
const char * pcHltLevels[3] = { "HLT-Low", "HLT-Medium", "HLT-High" };


static float fGetStableHltTemp(float tolerance);
bool xTickTimer(portTickType * ticksToWait, portTickType setpoint);
HltLevel xGetHltLevel();
void vHLTAppletDisplay(void *pvParameters);

unsigned int uiGetHltTemp()
{
	return (unsigned int) ds1820_get_temp(HLT);
}

void vPrintHltMessage(HltMessage msg)
{
	static char buf[80];

	if (msg.pcTxt != NULL )
	{
		sprintf(buf, "From: %s\r\n\0", msg.pcTxt);
		vConsolePrint(buf);
		vTaskDelay(50);
	}
	sprintf(buf, "Command: %s\r\n\0", pcHltCommands[msg.command]);
	vConsolePrint(buf);
	vTaskDelay(50);

	sprintf(buf, "Data1: %d\r\n\0", msg.iData1);
	vConsolePrint(buf);
	vTaskDelay(50);

	sprintf(buf, "Data2: %d\r\n\0", msg.iData2);
	vConsolePrint(buf);
	vTaskDelay(50);

	sprintf(buf, "Data3: ~%d\r\n\0", (int) msg.fData3);
	vConsolePrint(buf);
	vTaskDelay(50);

	sprintf(buf, "Data4: ~%d\r\n\0", (int) msg.fData4);
	vConsolePrint(buf);
	vTaskDelay(50);

	sprintf(buf, "Step: %d\r\n\0", (int) msg.ucStepNumber);
	vConsolePrint(buf);
	vTaskDelay(50);
}

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
	if (xHltTaskQueue == NULL )
	{
		xHltTaskQueue = xQueueCreate(5, sizeof(HltMessage));
		if (xHltTaskQueue == NULL )
			vConsolePrint("FATAL Error creating HLT Task Queue\r\n\0");
		else
			vConsolePrint("Created Brew HLT Task Queues\r\n\0");
	}



}

static unsigned int uiActualLitresDelivered = 0;
unsigned int uiGetActualLitresDelivered(void)
{
	return uiActualLitresDelivered;
}

static char cMaintainHighLevelHlt()
{
	char cSetpointReached = 0;
	if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN )) // looking for zero volts
	{
		vTaskDelay(2000);
		vActuateValve(&valves[INLET_VALVE], CLOSE_VALVE);
		cSetpointReached = 1;
	}
	else
	{
		vActuateValve(&valves[INLET_VALVE], OPEN_VALVE);
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
//=================================================================================================================================================================
void
vTaskBrewHLT(void * pvParameters)
{
	portTickType xLastWakeTime;

	char buf[50];
	xLastWakeTime = xTaskGetTickCount();
	static uint8_t uMessageReceivedOnThisIteration = 0;
	float fTempSetpoint = 0.0;
	static float fLitresToDrain = 0.0;
	float fActualLitresDelivered = 0.0;
	portTickType xTaskDelayMillis = 200;
	portTickType xTaskTicksPerSecond = 1000 / xTaskDelayMillis;
	int iValveCloseDelaySeconds = 2;
	static portTickType xTicksToWait = 0;
	bool IsValveOpening = FALSE;
	//float currentHltTemp = 0.0;
	BrewMessage xMsgCmdCompleteToBrew;

	struct TextMsg * NewMessage = (struct TextMsg *) pvPortMalloc(sizeof(struct TextMsg));

	char pcMessageText[40];

	static unsigned char ucStep = 0;
	static unsigned char ucHeatAndFillMessageSent = 0;
	static unsigned char ucLitresDeliveredDisplayCtr = 0;
	char cHltLevelReached = 0;
	char cHltTempReached = 0;

	HltMessage rcvdMsg;

	for (;;)
	{
		if (xQueueReceive(xHltTaskQueue, &rcvdMsg, 0) == pdPASS)
		{
			vPrintHltMessage(rcvdMsg);
			uMessageReceivedOnThisIteration = 1;
			ucHeatAndFillMessageSent = 0;
			ucStep = rcvdMsg.ucStepNumber;
		}
		else
			uMessageReceivedOnThisIteration = 0;

		switch (rcvdMsg.command)
		{
			case HLT_CMD_IDLE:
				{
				if (uMessageReceivedOnThisIteration)
				{
					vConsolePrint("HLT Entered IDLE State\r\n\0");
					vActuateValve(&valves[INLET_VALVE], CLOSE_VALVE);
					vActuateValve(&valves[HLT_VALVE], CLOSE_VALVE);
					GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
				}
				break;
			}
			case HLT_CMD_HEAT_AND_FILL:
				{
				if (uMessageReceivedOnThisIteration)
				{
					fTempSetpoint = rcvdMsg.fData3;
					vConsolePrint("HLT Entered HEAT AND FILL State\r\n\0");
					ThisBrewState.xHLTState.hltBrewState = HLT_STATE_FILL_HEAT;
					sprintf(pcMessageText, "Heating to %02d.%02d Deg", (unsigned int) floor(fTempSetpoint), (unsigned int) ((fTempSetpoint - floor(fTempSetpoint)) * pow(10, 2)));
					NewMessage->pcMsgText = pcMessageText;
					NewMessage->ucLine = 4;
					vConsolePrint(pcMessageText);
					xQueueSendToBack(xBrewAppletTextQueue, &NewMessage, 0);
				}
				// make sure the HLT valve is closed
				vActuateValve(&valves[HLT_VALVE], CLOSE_VALVE);

				cHltLevelReached = cMaintainHighLevelHlt();
				cHltTempReached = cMaintainTempHlt(fTempSetpoint);

				if (cHltLevelReached == 1 && cHltTempReached == 1)
				{
					if (ucHeatAndFillMessageSent == 0)
					{
						sprintf(buf, "HLT: Temp and level reached, sending msg\r\n\0");
						vConsolePrint(buf);
						ThisBrewState.xHLTState.hltBrewState = HLT_STATE_AT_TEMP;
						xMsgCmdCompleteToBrew.ucFromTask = HLT_TASK;
						xMsgCmdCompleteToBrew.iBrewStep = rcvdMsg.ucStepNumber;
						xMsgCmdCompleteToBrew.xCommand = BREW_STEP_COMPLETE;
						xQueueSendToBack(xBrewTaskReceiveQueue, &xMsgCmdCompleteToBrew, 0);
						ucHeatAndFillMessageSent = 1;
					}
				}
				break;
			}
			case HLT_CMD_DRAIN:
				{
				if (uMessageReceivedOnThisIteration)
				{
					GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
					fLitresToDrain = rcvdMsg.fData3;
					vConsolePrint("HLT: Entered DRAIN State\r\n\0");
					//Todo: Send this message to the LCD via the text message queue
					LCD_FLOAT(10, 10, 1, fLitresToDrain * 1000);
					lcd_printf(1, 10, 10, "Draining:");
					vActuateValve(&valves[HLT_VALVE], OPEN_VALVE);
					IsValveOpening = TRUE;
					vResetFlow1();
					ThisBrewState.xHLTState.hltBrewState = HLT_STATE_DRAIN;
					ucLitresDeliveredDisplayCtr = 0;

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
					vActuateValve(&valves[HLT_VALVE], OPEN_VALVE);
					fActualLitresDelivered = fGetBoilFlowLitres();

					if (ucLitresDeliveredDisplayCtr++ >= 5 * xTaskTicksPerSecond) // every 5 seconds.
					{
						sprintf(buf, "Delivered = %dml \r\n\0", (int) (fActualLitresDelivered * 1000));
						vConsolePrint(buf);
						ucLitresDeliveredDisplayCtr = 0;
					}
					if (fActualLitresDelivered >= fLitresToDrain)
					{
						vConsolePrint("HLT is DRAINED\r\n\0");
						vActuateValve(&valves[HLT_VALVE], CLOSE_VALVE);
						vTaskDelay(500);
						uiActualLitresDelivered += (unsigned int) (fActualLitresDelivered * 1000);

						xMsgCmdCompleteToBrew.ucFromTask = HLT_TASK;
						xMsgCmdCompleteToBrew.iBrewStep = rcvdMsg.ucStepNumber;
						xMsgCmdCompleteToBrew.xCommand = BREW_STEP_COMPLETE;
						xQueueSendToBack(xBrewTaskReceiveQueue, &xMsgCmdCompleteToBrew, 0);
						rcvdMsg.command = HLT_CMD_IDLE;
						ThisBrewState.xHLTState.hltBrewState = HLT_STATE_IDLE;
						WaterAddedToMashTun(fActualLitresDelivered);

					}
				}
				break;
			}

		}
		vTaskDelayUntil(&xLastWakeTime, xTaskDelayMillis / portTICK_RATE_MS);

	}
}

#pragma clang diagnostic pop

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
	if (!GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN )
	    && GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN ))
		return HLT_LEVEL_MID;
	else
		return HLT_LEVEL_LOW;
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
			LastTemp = (temp1 + temp2 + temp3) / 3;
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
	S.filling = xGetValveState(&valves[INLET_VALVE]);
	S.draining = xGetValveState(&valves[HLT_VALVE]);
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
	if (GPIO_ReadInputDataBit(HLT_SSR_PORT, HLT_SSR_PIN ))
		return HLT_HEATING;
	return HLT_NOT_HEATING;
}

//=================================================================================================================================================================

void vTaskHLTLevelChecker(void * pvParameters)
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
			if (level == HLT_LEVEL_HIGH && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN ))
			{
				GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
				vConsolePrint("HLT Level Check Task: INTERVENED\r\n\0");
				vConsolePrint("HLT Level Check Task: SSR on while level low!\r\n\0");
			}
		}
		if (level == HLT_LEVEL_HIGH && xGetValveState(&valves[INLET_VALVE]) == VALVE_OPENED)
		{
			vTaskDelay(3000);
			level = xGetHltLevel();
			if (level == HLT_LEVEL_HIGH && xGetValveState(&valves[INLET_VALVE]) == VALVE_OPENED)
			{
				vActuateValve(&valves[INLET_VALVE], CLOSE_VALVE);
				vConsolePrint("HLT Level Check Task: INTERVENED\r\n\0");
				vConsolePrint("INLET OPENED while level HIGH for >3s!\r\n\0");
			}
		}
		vTaskDelay(delay);
	}
}


void vTaskHeatHLT()
{
	float actual = 0.0;
	uint16_t hlt_level = 0;
	HltLevel hltLevel = HLT_LEVEL_LOW;
	char hlt_ok = 0;


	for (;;)
	{
		hltLevel = xGetHltLevel();
		if (hltLevel != HLT_LEVEL_LOW)
		{
			actual = fGetStableHltTemp(2.0);

			if (actual < diag_setpoint)
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
			vConsolePrint("Manual HLT: Level LOW, heating off!\r\n\0");
			GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
			hlt_state = OFF;
		}
		vTaskDelay(500 / portTICK_RATE_MS); // 500ms delay is fine for heating
	}
}


static int SetpointUp()
{
	diag_setpoint += 0.5;
	return 0;
}


static int SetpointDown()
{
	diag_setpoint -= 0.5;
	return 0;
}

static int StartHeating()
{
	manual_hlt_command = HEATING;
	if (xHeatHLTTaskHandle == NULL )
	{
		vConsolePrint("Creating MANUAL HLT Task\r\n\0");
		xTaskCreate( vTaskHeatHLT, ( signed portCHAR * ) "HLT_HEAT",configMINIMAL_STACK_SIZE +300, NULL, tskIDLE_PRIORITY+1, &xHeatHLTTaskHandle);
	}
	return 0;
}

static int StopHeating()
{
	manual_hlt_command = OFF;
	if (xHeatHLTTaskHandle != NULL )
	{
		vTaskDelete(xHeatHLTTaskHandle);
		xHeatHLTTaskHandle = NULL;
	}
	GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
	hlt_state = OFF;
	return 0;
}

static int Back()
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
	//return the semaphore for taking by another task.
	xSemaphoreGive(xHLTAppletRunningSemaphore);
	return 1;
}

static Button HltButtons[] =
{
		{SETPOINT_UP_X1, SETPOINT_UP_Y1, SETPOINT_UP_X2, SETPOINT_UP_Y2, "SP UP", Red, Blue, SetpointUp, ""},
		{SETPOINT_DN_X1, SETPOINT_DN_Y1, SETPOINT_DN_X2, SETPOINT_DN_Y2, "SP DOWN", Red, Blue, SetpointDown, ""},
		{START_HEATING_X1, START_HEATING_Y1, START_HEATING_X2, START_HEATING_Y2, "Start Heating", Red, Blue, StartHeating, ""},
		{STOP_HEATING_X1, STOP_HEATING_Y1, STOP_HEATING_X2, STOP_HEATING_Y2, "Stop Heating", Red, Blue, StopHeating, ""},
		{BAK_X1, BAK_Y1, BAK_X2, BAK_Y2, "BACK", Red, Blue, Back, ""},
};


static int HltButtonCount()
{
	return ARRAY_LENGTH(HltButtons);
}

void vHLTApplet(int init)
{
	vDrawButtons(HltButtons, HltButtonCount() );
	if (init)
	{


		//create a dynamic display task
		vConsolePrint("Creating HLT_Display Task!\r\n\0");
		xTaskCreate( vHLTAppletDisplay, ( signed portCHAR * ) "hlt_disp",
		    configMINIMAL_STACK_SIZE + 200, NULL, tskIDLE_PRIORITY,
		    &xHLTAppletDisplayHandle);

	}
	else
		vConsolePrint("n\r\n\0");
}

void vHLTAppletDisplay(void *pvParameters)
{
	static char tog = 0; //toggles each loop
	HltState hltState;
	float diag_setpoint1; // = diag_setpoint;
	char hlt_ok = 0;
	for (;;)
	{

		xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);

		hltState = GetHltState();
		diag_setpoint1 = diag_setpoint;

		lcd_printf(0, 10, 15, "%d.%d", (unsigned int) floor(diag_setpoint), (unsigned int) ((diag_setpoint - floor(diag_setpoint)) * pow(10, 3)));
		lcd_printf(0, 11, 15, pcHltLevels[hltState.level]);
		lcd_printf(0, 12, 15, manual_hlt_command == HEATING ? "CMD: HEAT" : "CMD: OFF");
		lcd_printf(0, 13, 15, hltState.heatingState == HLT_HEATING ? "HEATING" : "NOT HEATING");
		lcd_printf(0, 14, 20, "Currently = %d.%ddegC", (unsigned int) floor(hltState.temp_float), (unsigned int) ((hltState.temp_float - floor(hltState.temp_float)) * pow(10, 3)));


		xSemaphoreGive(xHLTAppletRunningSemaphore);
		//give back the semaphore as its safe to return now.
		vTaskDelay(500);

	}
}

void vHLTAppletCallback(int in_out)
{
//printf(" Called in_out = %d\r\n\0", in_out);

}

int HLTKey(int xx, int yy)
{
	int retVal = ActionKeyPress(HltButtons, HltButtonCount(), xx, yy);
	vTaskDelay(10);
	return retVal;

}

