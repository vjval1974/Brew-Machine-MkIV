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
#include <math.h>
#include "task.h"
#include "semphr.h"
#include "message.h"
#include "valves.h"
#include "brew.h"
#include "console.h"
#include "Flow1.h"
#include "main.h"

volatile char hlt_state = OFF;

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xHLTAppletRunningSemaphore;
xTaskHandle xHeatHLTTaskHandle = NULL, xHLTAppletDisplayHandle = NULL;
xQueueHandle xBrewTaskHLTQueue = NULL;
xTaskHandle xBrewHLTTaskHandle = NULL;
xTaskHandle xTaskHLTLevelCheckerTaskHandle = NULL;

volatile float diag_setpoint = 74.5; // when calling the heat_hlt task, we use this value instead of the passed parameter.
#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%0d.%0d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));


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
  if (xBrewTaskHLTQueue == NULL)
    {
      xBrewTaskHLTQueue = xQueueCreate(5,sizeof(struct GenericMessage *));
      if (xBrewTaskHLTQueue == NULL)
        vConsolePrint("FATAL Error creating  Brew HLT Task Queue\r\n");
      else vConsolePrint("Created Brew HLT Task Queues\r\n");
    }



}

//=================================================================================================================================================================
void vTaskBrewHLT(void * pvParameters)
{
  portTickType xLastWakeTime;
  struct GenericMessage * gMsg;
  struct HLTMsg * hMsg;
  static uint8_t uRcvdState = HLT_STATE_IDLE;
  const char * pcRcvdMsgText;
  static float fDrainLitres = 0;
  char buf[50];
  xLastWakeTime = xTaskGetTickCount();
  static uint8_t uFirst = 0;
  float  fTempSetpoint = 0.0;
  static float fLitresToDrain = 0.0;
  float fActualLitresDelivered = 0.0;
  float actual = 0.0;
  float hlt_level = 0.0;
  char hlt_ok = 0;
  struct TextMsg  * NewMessage;
  NewMessage = (struct TextMsg *)pvPortMalloc(sizeof(struct TextMsg));
  struct GenericMessage * xMessage;
  xMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  char pcMessageText[40];
  static unsigned char ucStep = 0;
  static unsigned char ucHeatAndFillMessageSent = 0;

  const int STEP_COMPLETE = 40;
  const int STEP_FAILED = 41;
  const int STEP_TIMEOUT = 45;



  for(;;)
    {
      if(xQueueReceive(xBrewTaskHLTQueue, &gMsg, 0) == pdPASS){
          hMsg = (struct HLTMsg *)gMsg->pvMessageContent;
          uRcvdState = hMsg->uState;
          pcRcvdMsgText = hMsg->pcMsgText;
          ucStep = gMsg->uiStepNumber;
          uFirst = 1;
          ucHeatAndFillMessageSent = 0;
          if (hMsg->pcMsgText != NULL)
            {
              sprintf(buf, "HLT Message: From: %s, to: %s\r\n", pcTASKS[gMsg->ucFromTask-TASK_BASE], pcTASKS[gMsg->ucToTask - TASK_BASE]);
              vConsolePrint(buf);
              vConsolePrint(hMsg->pcMsgText);
              vConsolePrint("\r\n");
            }

      }
      else uFirst = 0;

      switch(uRcvdState)
      {
      case HLT_STATE_IDLE:
        {
          if (uFirst){
              vConsolePrint("HLT Entered IDLE State\r\n");
              vValveActuate(INLET_VALVE, CLOSE);
              vValveActuate(HLT_VALVE, CLOSE);
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);

          }

          break;
        }

      case HLT_STATE_FILL_HEAT:
        {
          if (uFirst)
            {
              fTempSetpoint = hMsg->uData1;
              vConsolePrint("HLT Entered HEAT AND FILL State\r\n");
              //LCD_FLOAT(10,3,1,fTempSetpoint);
              //lcd_printf(1,3,10, "Setpoint:");

              //ucHLTFlag = 0;
              BrewState.ucHLTState = HLT_STATE_FILL_HEAT;
              sprintf(pcMessageText, "Heating to %02d.%02d Deg", (unsigned int)floor(fTempSetpoint), (unsigned int)((fTempSetpoint-floor(fTempSetpoint))*pow(10, 2)));
              NewMessage->pcMsgText = pcMessageText;
              NewMessage->ucLine = 4;
              xQueueSendToBack(xBrewAppletTextQueue, &NewMessage, 0);
            }

          // make sure the HLT valve is closed
          vValveActuate(HLT_VALVE, CLOSE);


          if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN)) // looking for zero volts
            {
              vTaskDelay(2000);
              vValveActuate(INLET_VALVE, CLOSE);
              // vConsolePrint("HLT is FULL \r\n");
            }
          else
            vValveActuate(INLET_VALVE, OPEN);

          // ensure we have water above the elements
          if (!GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN)) // looking for zero volts
            {
              //check the temperature
              actual = ds1820_get_temp(HLT);

              // output depending on temp
              if (actual < fTempSetpoint)
                {
                  GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 1);

                }
              else if (actual > fTempSetpoint + 0.1)
                {
                  //vConsolePrint("HLT: Temp reached, checking again\r\n");
                  vTaskDelay(200);
                  if (actual > fTempSetpoint + 0.1) // check again
                    {
                      GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
                      //vConsolePrint("HLT: Temp reached for sure!\r\n");
                      // Send message if we are full and at temp.
                      if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN)) // looking for zero volts
                        {

                          // Only send once
                          if (ucHeatAndFillMessageSent == 0)
                            {
                              vConsolePrint("HLT: Temp and level reached, sending msg\r\n");
                              BrewState.ucHLTState = HLT_STATE_AT_TEMP;
                              xMessage->ucFromTask = HLT_TASK;
                              xMessage->ucToTask = BREW_TASK;
                              xMessage->uiStepNumber = ucStep;
                              xMessage->pvMessageContent = (void *)&STEP_COMPLETE;
                              xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage, 0);
                              ucHeatAndFillMessageSent = 1;
                            }
                        }
                    }
                }
            }
          else //DONT HEAT... WE WILL BURN OUT THE ELEMENT
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
              //vConsolePrint("WARNING, Water not above element in HLT\r\n");
            }
#ifdef TESTING
          if (ucHeatAndFillMessageSent == 0)
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              vValveActuate(INLET_VALVE, OPEN);
              vTaskDelay(1000);
              vValveActuate(INLET_VALVE, CLOSE);
              vValveActuate(HLT_VALVE, OPEN);
              vTaskDelay(4000);
              vValveActuate(HLT_VALVE, CLOSE);
              vTaskDelay(500);

              vConsolePrint("HLT: Temp and level reached, sending msg\r\n");
              BrewState.ucHLTState = HLT_STATE_AT_TEMP;
              xMessage->ucFromTask = HLT_TASK;
              xMessage->ucToTask = BREW_TASK;
              xMessage->uiStepNumber = ucStep;
              xMessage->pvMessageContent = (void *)&STEP_COMPLETE;
              xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage, 0);
              ucHeatAndFillMessageSent = 1;
            }
#endif

          break;
        }
      case HLT_STATE_DRAIN:
        {
          if (uFirst)
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
              vResetFlow1();
              fLitresToDrain = hMsg->uData1;
              vConsolePrint("HLT: Entered DRAIN State\r\n");

              // Need to set up message to the Applet.
              LCD_FLOAT(10,10,1,fTempSetpoint);
              lcd_printf(1,10,10, "Setpoint:");
              vValveActuate(HLT_VALVE, OPEN);
              BrewState.ucHLTState = HLT_STATE_DRAIN;
#ifdef TESTING
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              vValveActuate(INLET_VALVE, OPEN);
              vTaskDelay(1000);
              vValveActuate(INLET_VALVE, CLOSE);
              vValveActuate(HLT_VALVE, OPEN);
              vTaskDelay(4000);
              vValveActuate(HLT_VALVE, CLOSE);
              vTaskDelay(500);
              vValveActuate(HLT_VALVE, CLOSE);
              xMessage->ucFromTask = HLT_TASK;
              xMessage->ucToTask = BREW_TASK;
              xMessage->uiStepNumber = ucStep;
              xMessage->pvMessageContent = (void *)&STEP_COMPLETE;
              xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage, 0);
              vConsolePrint("HLT is DRAINED\r\n");
              uRcvdState = HLT_STATE_IDLE;
#endif
            }
          vValveActuate(HLT_VALVE, OPEN);
          fActualLitresDelivered = fGetBoilFlowLitres();
          if(((int)(fActualLitresDelivered * 1000) % 100) < 10)
            {
              sprintf(buf, "ml delivered = %d\r\n", (int)(fActualLitresDelivered*1000));

              vConsolePrint(buf);
            }
#ifdef TESTING
          fActualLitresDelivered = fLitresToDrain + 1;
#endif
          if (fActualLitresDelivered >= fLitresToDrain)
            {
              vValveActuate(HLT_VALVE, CLOSE);
              xMessage->ucFromTask = HLT_TASK;
              xMessage->ucToTask = BREW_TASK;
              xMessage->uiStepNumber = ucStep;
              xMessage->pvMessageContent = (void *)&STEP_COMPLETE;
              xQueueSendToBack(xBrewTaskReceiveQueue, &xMessage, 0);
              vConsolePrint("HLT is DRAINED\r\n");
              uRcvdState = HLT_STATE_IDLE;

            }

          //sprintf(buf, "Draining!\r\n");
          //vConsolePrint(buf);
          break;
        }
      }

      vTaskDelayUntil(&xLastWakeTime, 200 / portTICK_RATE_MS );

    }

}

//=================================================================================================================================================================

void vTaskHLTLevelChecker( void * pvParameters)
{
  uint16_t hlt_level_low = 0;
  uint16_t hlt_level_high = 0;
  int delay = 500;
  static char cBuffer[1000];

  for (;;)
    {
     hlt_level_low = !GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN);
     hlt_level_high = !GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN);
      if (!hlt_level_low && GPIO_ReadInputDataBit(HLT_SSR_PORT, HLT_SSR_PIN))
        {
          vTaskDelay(500);
          if (!hlt_level_low && GPIO_ReadInputDataBit(HLT_SSR_PORT, HLT_SSR_PIN))
            {
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              vConsolePrint("HLT Level Check Task: INTERVENED\r\n");
              vConsolePrint("HLT Level Check Task: SSR on while level low!\r\n");
             // vTaskPrioritySet(NULL,  tskIDLE_PRIORITY + 5);
              //while(1);
            }
        }
      if (hlt_level_high  && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN))
        {

          vTaskDelay(3000);

          hlt_level_high = !GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN);

          if (hlt_level_high && GPIO_ReadInputDataBit(INLET_VALVE_PORT, INLET_VALVE_PIN))
            {
              vValveActuate(INLET_VALVE, CLOSED);
              vConsolePrint("HLT Level Check Task: INTERVENED\r\n");
              vConsolePrint("INLET OPENED while level HIGH for >3s!\r\n");
              vTaskPrioritySet(NULL,  tskIDLE_PRIORITY + 5);
              while(1);
            }
        }
      vTaskList(cBuffer);
     // vConsolePrint(cBuffer);
      vTaskDelay(delay);

    }

}


//raw_adc_value = read_adc(HLT_LEVEL_ADC_CHAN);
//GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN);

void
vTaskHeatHLT(void * pvParameters)
{
  float * setpoint = (float *) pvParameters;
  float actual = 0.0;
  uint16_t hlt_level = 0;
  char hlt_ok = 0;
  //choose which value we use for the setpoint.
  if (setpoint == NULL )
    setpoint = (float *) &diag_setpoint;

  for (;;)
    {
      hlt_level = !GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN);
      // ensure we have water above the elements

      if (hlt_level)
        {
          //check the temperature
          actual = ds1820_get_temp(HLT);

          // output depending on temp
          if (actual < *setpoint)
            {
              vConsolePrint("Manual HLT: Heating ON\r\n");
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 1);
              hlt_state = HEATING;
            }
          else
            {
              vConsolePrint("Manual HLT: Heating OFF\r\n");
              GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0);
              hlt_state = OFF;
            }
        }
      else //DONT HEAT... WE WILL BURN OUT THE ELEMENT
        {
          vConsolePrint("Manual HLT: Level LOW, heating off!\r\n");
          GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
          hlt_state = OFF;
        }
      vTaskDelay(500/portTICK_RATE_MS); // 500ms delay is fine for heating
//      taskYIELD();
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
void
vHLTApplet(int init)
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
      //vTaskDelay(2000);
      //adc_init();
      //adc_init();
      //create a dynamic display task
      vConsolePrint("Creating HLT_Display Task!\r\n");
      xTaskCreate( vHLTAppletDisplay, ( signed portCHAR * ) "hlt_disp",
          configMINIMAL_STACK_SIZE + 200, NULL, tskIDLE_PRIORITY,
          &xHLTAppletDisplayHandle);
     // vTaskSuspend(xBrewHLTTaskHandle);
    }
  else
    vConsolePrint("n\r\n");
 // vTaskResume(xBrewHLTTaskHandle);

}
#define LOW 0
#define MID 1
#define HIGH 2
const char * HLT_LEVELS[] =  {"HLT-Low", "HLT-Medium", "HLT-High"};
void vHLTAppletDisplay(void *pvParameters)
{
  static char tog = 0; //toggles each loop
  unsigned char  ucHLTLevel;
  float diag_setpoint1; // = diag_setpoint;
  float current_temp;
  uint8_t ds10;
  char hlt_ok = 0;
  for (;;)
    {

      xSemaphoreTake(xHLTAppletRunningSemaphore, portMAX_DELAY);
      //take the semaphore so that the key handler wont
      //return to the menu system until its returned
      current_temp = ds1820_get_temp(HLT);

      if (!GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN))
        {
          ucHLTLevel = MID;
          if (!GPIO_ReadInputDataBit(HLT_HIGH_LEVEL_PORT, HLT_HIGH_LEVEL_PIN))
            {
              ucHLTLevel = HIGH;
            }

        }
      else {
          ucHLTLevel = LOW;
      }
      diag_setpoint1 = diag_setpoint;
      lcd_fill(1, 178, 170, 40, Black);

      //Tell user whether there is enough water to heat
      if (ucHLTLevel == LOW)
        lcd_printf(1, 11, 20, HLT_LEVELS[LOW]);
      else if (ucHLTLevel == MID)
        lcd_printf(1, 11, 20, HLT_LEVELS[MID]);
      else lcd_printf(1, 11, 20, HLT_LEVELS[HIGH]);


      //display the state and user info (the state will flash on the screen)
      switch (hlt_state)
        {
      case HEATING:
        {
          if (tog)
            {
              lcd_fill(1, 220, 180, 29, Black);
              lcd_printf(1, 13, 15, "HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(current_temp),
                  (unsigned int) ((current_temp - floor(current_temp))
                      * pow(10, 3)));
              //lcd_printf(1,14,15,"Currently = %2.1fdegC", current_temp);
            }
          else
            {
              lcd_fill(1, 210, 180, 17, Black);
            }
          break;
        }
      case OFF:
        {
          if (tog)
            {
              lcd_fill(1, 210, 180, 29, Black);
              lcd_printf(1, 13, 11, "NOT HEATING");
              lcd_printf(1, 14, 25, "Currently = %d.%ddegC",
                  (unsigned int) floor(current_temp),
                  (unsigned int) ((current_temp - floor(current_temp))
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
              configMINIMAL_STACK_SIZE +800, NULL, tskIDLE_PRIORITY+1,
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

