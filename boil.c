/*
 * boil.c
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */

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
#include "message.h"
#include "brew.h"
#include "boil.h"
#include "console.h"
#include "adc.h"
// main.h holds the definition for the preprocessor directive TESTING
#include "main.h"



#define BOIL_PORT GPIOD
#define BOIL_PIN GPIO_Pin_12
#define BOIL_DUTY_ADC_CHAN 10 // This is PC0

#define BOIL_LEVEL_PORT GPIOC
#define BOIL_LEVEL_PIN GPIO_Pin_12

#define BOILING 1
#define OFF 0
#define WAITING_FOR_COMMAND 2

xQueueHandle xBoilQueue = NULL;



#define TIM_ARR_TOP 10000

int diag_duty = 50;
volatile uint8_t uiBoilState = WAITING_FOR_COMMAND;

void vBoilAppletDisplay(void * pvParameters);
void vTaskBoil( void * pvParameters);

unsigned char ucGetBoilState(){
  return uiBoilState;
}

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;

xTaskHandle xBoilTaskHandle = NULL, xBoilAppletDisplayHandle = NULL;

void vBoilInit(void)
{
  unsigned long ulFrequency;
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;


  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_InitStructure.GPIO_Pin =  BOIL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;// Alt Function - Push Pull
  GPIO_Init( BOIL_PORT, &GPIO_InitStructure );


  GPIO_InitStructure.GPIO_Pin =  BOIL_LEVEL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;// Pulled up input
  GPIO_Init( BOIL_LEVEL_PORT, &GPIO_InitStructure );

  /* Enable timer clocks */
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM4, ENABLE );

  /* Initialise Ports, pins and timer */
  TIM_DeInit( TIM4 );
  TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

  // SET UP TIMER 4  FOR PWM
  // ATM gives 1HZ 50% on pin PB8

  TIM_TimeBaseStructure.TIM_Period = 1000;
  TIM_TimeBaseStructure.TIM_Prescaler = 7200; //clock prescaler
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit( TIM4, &TIM_TimeBaseStructure );
  TIM_ARRPreloadConfig( TIM4, ENABLE );

  TIM_OCInitTypeDef TIM_OCInitStruct;
  TIM_OCStructInit( &TIM_OCInitStruct );
  TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
  TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
  TIM_OCInitStruct.TIM_Pulse = 5000; //50% for the start, not enabled until set by calling vBoil()
  TIM_OC1Init( TIM4, &TIM_OCInitStruct ); //Pd12
  //TIM_OC3Init( TIM4, &TIM_OCInitStruct );
  TIM_SetAutoreload(TIM4, TIM_ARR_TOP);
  //
  GPIO_PinRemapConfig( GPIO_Remap_TIM4, ENABLE );

  TIM_SetCompare1(TIM4, 0);
  TIM_Cmd( TIM4, DISABLE );
  GPIO_ResetBits(BOIL_PORT, BOIL_PIN);
  uiBoilState = OFF;

  vSemaphoreCreateBinary(xAppletRunningSemaphore);

  xBoilQueue = xQueueCreate(1, sizeof(struct GenericMessage *));

  if (xBoilQueue != NULL)
    {
      xTaskCreate( vTaskBoil,
          ( signed portCHAR * ) "boil Task",
          configMINIMAL_STACK_SIZE + 500,
          NULL,
          tskIDLE_PRIORITY ,
          &xBoilTaskHandle );
    }
  //else
  //  printf("Boil task could not be created.. memory error?\r\n");


}

uint8_t uGetBoilLevel(void)
{
 // // **************OVERRIDDEN *************************
  //return 1;

  return (GPIO_ReadInputDataBit(BOIL_LEVEL_PORT, BOIL_LEVEL_PIN) == 0);

}

// Return integer percentage of ADC input for boil duty cycle
static unsigned int uiGetADCBoilDuty()
{
  float fDuty = 0.0; // duty cycle as float
  int iDuty = 0; // as int
  uint16_t uADCIn = read_adc(BOIL_DUTY_ADC_CHAN);
  unsigned int uiTimerCompareValue = 0;
  // get adc value as a %
  fDuty = (float)uADCIn/4095.0 * 100.0;
  iDuty = (int)fDuty;
  return iDuty;
}

// return the timer compare value based on the duty cycle.
static unsigned int uiTimerCompare(int iDutyCycle)
{
  return ((TIM_ARR_TOP/100) * iDutyCycle);
}

// start the boiling process based upon the timer compare value
static void vStartBoilPWM(unsigned int uiTimerCompareValue)
{
  TIM_SetCompare1(TIM4, uiTimerCompareValue);
  TIM_Cmd( TIM4, ENABLE );
}

// stop the boil.
static void vStopBoilPWM(void)
{
  TIM_SetCompare1(TIM4, 0);
  TIM_Cmd( TIM4, DISABLE );
  GPIO_ResetBits(BOIL_PORT, BOIL_PIN);
}

static unsigned int uiValidateIntegerPct(int iValue)
{
  if (iValue > 100)
    iValue = 0; //garbage result
  if (iValue < 0)
    iValue = 0;
  return iValue;
}

static unsigned int uiTimerCompareController(unsigned char ucMessageSource, int iDefaultDuty)
{
  static char buf[50];
  unsigned int uiADCDuty = 0;
  int iDuty = 0;
  unsigned int uiTimerCompareValue = 0;
  switch (ucMessageSource)
  {
  case BREW_TASK:
  {
    vConsolePrint("Boil: Message received from BREW TASK\r\n");
    uiADCDuty = uiGetADCBoilDuty();
    // determine which duty cycle to accept
    if (uiADCDuty <= 20)
      {
        iDuty = iDefaultDuty;
      }
    uiTimerCompareValue = uiTimerCompare(iDuty); // set the timer value to the duty cycle %
    break;
  }
  case BREW_TASK_RESET:
    {
      uiTimerCompareValue = 0;
      break;
    }

  default:
    {
      uiTimerCompareValue = uiTimerCompare(iDefaultDuty); // set the timer value to the duty cycle %
      vConsolePrint("Boil: Message received\r\n");
      sprintf(buf, "Boil: Received duty cycle of %d, from ID #%d\r\n", iDefaultDuty, ucMessageSource);
      vConsolePrint(buf);
      break;
    }
  }
 return uiTimerCompareValue;
}

void vBoilStateController(unsigned int uiTimerCompareValue, unsigned int * uiBoilState)
{
  unsigned int boil_level =  uGetBoilLevel(); // each loop iteration, get the level of the boiler.

  switch (*uiBoilState)
       {
       case BOILING:
         {
           //if the boil level is low, ensure the elements are off and leave.
           if (!boil_level)
             {
               *uiBoilState = OFF;
               vConsolePrint("Boil level too low..Boil off. message ignored\r\n");
             }
           else
             {
               vStartBoilPWM(uiTimerCompareValue);
               *uiBoilState = WAITING_FOR_COMMAND;
             }

           break;
         }
       case OFF:
         {
           vStopBoilPWM();
           *uiBoilState = WAITING_FOR_COMMAND;
           break;
         }
       case WAITING_FOR_COMMAND:
         {

           break;
         }
       default:
         {
           vStopBoilPWM();
           *uiBoilState = WAITING_FOR_COMMAND;
         }
       }
}

void vTaskBoil( void * pvParameters)
{
  // Generic message struct for message storage.
  struct GenericMessage * xMessage;
  xMessage = (struct GenericMessage *)pvPortMalloc(sizeof(struct GenericMessage));
  int iDefaultDuty = 0; // receive value from queue.
  int iDuty = 0; // duty in int type
  unsigned int uiADCDuty = 0;
  unsigned int uiBoilState = WAITING_FOR_COMMAND;
  static unsigned int uiTimerCompareValue = 0; //value of the duty cycle when tranformed to timer units
  portBASE_TYPE xStatus; //queue receive status
  static char buf[50], buf1[50]; //text storage buffers
  for(;;)
    {
      // Receive message
      xStatus = xQueueReceive(xBoilQueue, &xMessage, 10); // check the queue and block for 10ms
      if (xStatus == pdPASS) // message received
        {
          //Validate the message contents
          iDefaultDuty = uiValidateIntegerPct(*(int *)xMessage->pvMessageContent);
          // Work out what to do with the duty cycle passed in based on the source of the message
          uiTimerCompareValue = uiTimerCompareController(xMessage->ucFromTask, iDefaultDuty);

          // If the compare value is above zero, change state to boiling
          if (uiTimerCompareValue > 0)
            {
              uiBoilState = BOILING;
              uiBoilState = BOILING;
            }
          else
            {
              uiBoilState = OFF;
              uiBoilState = OFF;
            }
        }
      else //no message received
        {
          vBoilStateController(uiTimerCompareValue, &uiBoilState);
        }

      vTaskDelay(1000);
    }
}

// ==========================


#define DUTY_UP_X1 0
#define DUTY_UP_Y1 30
#define DUTY_UP_X2 100
#define DUTY_UP_Y2 100
#define DUTY_UP_W (DUTY_UP_X2-DUTY_UP_X1)
#define DUTY_UP_H (DUTY_UP_Y2-DUTY_UP_Y1)

#define DUTY_DN_X1 0
#define DUTY_DN_Y1 105
#define DUTY_DN_X2 100
#define DUTY_DN_Y2 175
#define DUTY_DN_W (DUTY_DN_X2-DUTY_DN_X1)
#define DUTY_DN_H (DUTY_DN_Y2-DUTY_DN_Y1)


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
void vBoilApplet(int init){
  if (init)
        {
                lcd_DrawRect(DUTY_UP_X1, DUTY_UP_Y1, DUTY_UP_X2, DUTY_UP_Y2, Red);
                lcd_fill(DUTY_UP_X1+1, DUTY_UP_Y1+1, DUTY_UP_W, DUTY_UP_H, Blue);
                lcd_DrawRect(DUTY_DN_X1, DUTY_DN_Y1, DUTY_DN_X2, DUTY_DN_Y2, Red);
                lcd_fill(DUTY_DN_X1+1, DUTY_DN_Y1+1, DUTY_DN_W, DUTY_DN_H, Blue);
                lcd_DrawRect(STOP_HEATING_X1, STOP_HEATING_Y1, STOP_HEATING_X2, STOP_HEATING_Y2, Cyan);
                lcd_fill(STOP_HEATING_X1+1, STOP_HEATING_Y1+1, STOP_HEATING_W, STOP_HEATING_H, Red);
                lcd_DrawRect(START_HEATING_X1, START_HEATING_Y1, START_HEATING_X2, START_HEATING_Y2, Cyan);
                lcd_fill(START_HEATING_X1+1, START_HEATING_Y1+1, START_HEATING_W, START_HEATING_H, Green);
                lcd_DrawRect(BAK_X1, BAK_Y1, BAK_X2, BAK_Y2, Cyan);
                lcd_fill(BAK_X1+1, BAK_Y1+1, BAK_W, BAK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL Boil APPLET");
                lcd_printf(3,4,11,  "Duty UP");
                lcd_printf(1,8,13,  "Duty DOWN");
                lcd_printf(22,4,13, "START BOIL");
                lcd_printf(22,8,12, "STOP BOIL");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vBoilAppletDisplay,
                    ( signed portCHAR * ) "hlt_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                   tskIDLE_PRIORITY ,
                    &xBoilAppletDisplayHandle );
        }

}


void vBoilAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        uint8_t boil_level;
        float diag_duty1; // = diag_duty;

        for(;;)
        {

            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                    //return to the menu system until its returned
            boil_level = uGetBoilLevel();
            diag_duty1 = diag_duty;
            lcd_fill(1,178, 170,40, Black);

            //Tell user whether there is enough water to heat
            if (boil_level)
              lcd_printf(1,11,20,"level OK");
            else
              lcd_printf(1,11,20,"level LOW");

            //display the state and user info (the state will flash on the screen)
            switch (uiBoilState)
            {
            case BOILING:
              {
                if(tog)
                  {
                    lcd_fill(1,220, 180,29, Black);
                    lcd_printf(1,13,15,"BOILING");
                  }
                else{
                    lcd_fill(1,210, 180,17, Black);
                }
                break;
              }
            case OFF:
              {
                if(tog)
                  {
                    lcd_fill(1,210, 180,29, Black);
                    lcd_printf(1,13,11,"NOT BOILING");
                  }
                else
                  {
                    lcd_fill(1,210, 180,17, Black);
                  }

                break;
              }
            case WAITING_FOR_COMMAND:
              {
                if(tog)
                  {
                    lcd_fill(1,210, 180,29, Black);
                    lcd_printf(1,13,11,"WAITING_FOR_CMD");
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
                lcd_fill(102,99, 35,10, Black);
                lcd_printf(13,6,15,"%d%%", diag_duty);
                xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);


        }
}

struct GenericMessage xMessage1;

int iBoilKey(int xx, int yy)
{
  static struct GenericMessage * xMessage;
  xMessage = &xMessage1;
  xMessage->pvMessageContent = (void *)&diag_duty;
  xMessage->ucFromTask = 0;
  xMessage->uiStepNumber = 0;
  xMessage->ucToTask = BOIL_TASK;
  static int zero = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  if (xx > DUTY_UP_X1+1 && xx < DUTY_UP_X2-1 && yy > DUTY_UP_Y1+1 && yy < DUTY_UP_Y2-1)
    {
      diag_duty+=1;

      //printf("Duty Cycle is now %d\r\n", diag_duty);
      if (uiBoilState == BOILING)
        xQueueSendToBack(xBoilQueue, &xMessage, 0);
    }
  else if (xx > DUTY_DN_X1+1 && xx < DUTY_DN_X2-1 && yy > DUTY_DN_Y1+1 && yy < DUTY_DN_Y2-1)

    {
      if (diag_duty == 0)
        diag_duty = 0;
      else diag_duty-=1;
      //printf("Duty Cycle is now %d\r\n", diag_duty);
      if (uiBoilState == BOILING)
        xQueueSendToBack(xBoilQueue, &xMessage, 0);
    }
  else if (xx > STOP_HEATING_X1+1 && xx < STOP_HEATING_X2-1 && yy > STOP_HEATING_Y1+1 && yy < STOP_HEATING_Y2-1)
    {
      xMessage->pvMessageContent = (void*)&zero;
      xQueueSendToBack(xBoilQueue, &xMessage, 0);
    }
  else if (xx > START_HEATING_X1+1 && xx < START_HEATING_X2-1 && yy > START_HEATING_Y1+1 && yy < START_HEATING_Y2-1)
    {
      xMessage->pvMessageContent = (void*)&diag_duty;
      xQueueSendToBack(xBoilQueue, &xMessage, 0);

    }
  else if (xx > BAK_X1 && yy > BAK_Y1 && xx < BAK_X2 && yy < BAK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xBoilAppletDisplayHandle != NULL)
        {
          vTaskDelete(xBoilAppletDisplayHandle);
          vTaskDelay(100);
          xBoilAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;


}
