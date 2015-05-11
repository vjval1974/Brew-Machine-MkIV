/*
 * diag_pwm.c
 *
 *  Created on: May 19, 2013
 *      Author: brad
 */




/*
 * diag_temps.c
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "diag_temps.h"
#include "ds1820.h"


// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xPWMAppletRunningSemaphore;

void  vTaskDiagPWMApplet( void *pvParameters);

xTaskHandle xTaskDiagPWMAppletHandle;
////////////////////////////////////////////////////////////////////////////

void vDiagPWMInit(void)
{
  vSemaphoreCreateBinary(xPWMAppletRunningSemaphore);

}
#define ARR_UP_X1 0
#define ARR_UP_Y1 30
#define ARR_UP_X2 100
#define ARR_UP_Y2 100
#define ARR_UP_W (ARR_UP_X2-ARR_UP_X1)
#define ARR_UP_H (ARR_UP_Y2-ARR_UP_Y1)

#define PRESCALE_UP_X1 105
#define PRESCALE_UP_Y1 30
#define PRESCALE_UP_X2 205
#define PRESCALE_UP_Y2 100
#define PRESCALE_UP_W (PRESCALE_UP_X2-PRESCALE_UP_X1)
#define PRESCALE_UP_H (PRESCALE_UP_Y2-PRESCALE_UP_Y1)

#define PULSE_UP_X1 210
#define PULSE_UP_Y1 30
#define PULSE_UP_X2 315
#define PULSE_UP_Y2 100
#define PULSE_UP_W (PULSE_UP_X2-PULSE_UP_X1)
#define PULSE_UP_H (PULSE_UP_Y2-PULSE_UP_Y1)

#define ARR_DN_X1 0
#define ARR_DN_Y1 105
#define ARR_DN_X2 100
#define ARR_DN_Y2 175
#define ARR_DN_W (ARR_DN_X2-ARR_DN_X1)
#define ARR_DN_H (ARR_DN_Y2-ARR_DN_Y1)

#define PRESCALE_DN_X1 105
#define PRESCALE_DN_Y1 105
#define PRESCALE_DN_X2 205
#define PRESCALE_DN_Y2 175
#define PRESCALE_DN_W (PRESCALE_DN_X2-PRESCALE_DN_X1)
#define PRESCALE_DN_H (PRESCALE_DN_Y2-PRESCALE_DN_Y1)

#define PULSE_DN_X1 210
#define PULSE_DN_Y1 105
#define PULSE_DN_X2 315
#define PULSE_DN_Y2 175
#define PULSE_DN_W (PULSE_DN_X2-PULSE_DN_X1)
#define PULSE_DN_H (PULSE_DN_Y2-PULSE_DN_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)



//static applet
void vDiagPWMApplet(int init){

        if (init){
            lcd_DrawRect(ARR_UP_X1, ARR_UP_Y1, ARR_UP_X2, ARR_UP_Y2, Red);
            lcd_fill(ARR_UP_X1+1, ARR_UP_Y1+1, ARR_UP_W, ARR_UP_H, Blue);
            lcd_DrawRect(ARR_DN_X1, ARR_DN_Y1, ARR_DN_X2, ARR_DN_Y2, Red);
            lcd_fill(ARR_DN_X1+1, ARR_DN_Y1+1, ARR_DN_W, ARR_DN_H, Blue);

            lcd_DrawRect(PRESCALE_UP_X1, PRESCALE_UP_Y1, PRESCALE_UP_X2, PRESCALE_UP_Y2, Red);
            lcd_fill(PRESCALE_UP_X1+1, PRESCALE_UP_Y1+1, PRESCALE_UP_W, PRESCALE_UP_H, Blue);
            lcd_DrawRect(PRESCALE_DN_X1, PRESCALE_DN_Y1, PRESCALE_DN_X2, PRESCALE_DN_Y2, Red);
            lcd_fill(PRESCALE_DN_X1+1, PRESCALE_DN_Y1+1, PRESCALE_DN_W, PRESCALE_DN_H, Blue);

            lcd_DrawRect(PULSE_UP_X1, PULSE_UP_Y1, PULSE_UP_X2, PULSE_UP_Y2, Red);
            lcd_fill(PULSE_UP_X1+1, PULSE_UP_Y1+1, PULSE_UP_W, PULSE_UP_H, Blue);
            lcd_DrawRect(PULSE_DN_X1, PULSE_DN_Y1, PULSE_DN_X2, PULSE_DN_Y2, Red);
            lcd_fill(PULSE_DN_X1+1, PULSE_DN_Y1+1, PULSE_DN_W, PULSE_DN_H, Blue);


            lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
            lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
            lcd_printf(10,1,18,  "DIAG PWM APPLET");
            lcd_printf(3,4,11,  "ARR UP");
            lcd_printf(3,8,13,  "ARR DN");
            lcd_printf(14,4,13, "PreScale UP");
            lcd_printf(14,8,12, "PreScale DN");
            lcd_printf(27,4,13, "Pulse UP");
            lcd_printf(27,8,12, "Pulse DN");

            lcd_printf(30, 13, 4, "Back");



            //lcd_clear(Black);
        xTaskCreate( vTaskDiagPWMApplet,
                         ( signed portCHAR * ) "diag_pwm",
                         configMINIMAL_STACK_SIZE + 500,
                         NULL,
                         tskIDLE_PRIORITY,
                         &xTaskDiagPWMAppletHandle );
        }
}

volatile unsigned int uARR = 11999;
volatile unsigned int uPrescaler = 35;
volatile unsigned int uPulse = 10;
const double lfClock = 72000000;

int iDiagPWMKey(int xx, int yy){

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStruct;
    // get current values.. so we dont change whats happening outside...
    uPrescaler = TIM3->PSC;
    uPulse = TIM3-> CCR3;
    uARR = TIM3->ARR;

  //////////////////////////////////////////////////////////////
            if (xx > ARR_UP_X1+1 && xx < ARR_UP_X2-1 && yy > ARR_UP_Y1+1 && yy < ARR_UP_Y2-1)
                {
                uARR += 100;
                }

            else if (xx > ARR_DN_X1+1 && xx < ARR_DN_X2-1 && yy > ARR_DN_Y1+1 && yy < ARR_DN_Y2-1)
                {
                uARR-=100;
                }
            if (xx > PRESCALE_UP_X1+1 && xx < PRESCALE_UP_X2-1 && yy > PRESCALE_UP_Y1+1 && yy < PRESCALE_UP_Y2-1)
                {
                uPrescaler+=1;

                }

            else if (xx > PRESCALE_DN_X1+1 && xx < PRESCALE_DN_X2-1 && yy > PRESCALE_DN_Y1+1 && yy < PRESCALE_DN_Y2-1)
                {
                uPrescaler-=1;
                }
            if (xx > PULSE_UP_X1+1 && xx < PULSE_UP_X2-1 && yy > PULSE_UP_Y1+1 && yy < PULSE_UP_Y2-1)
                {
                uPulse+=10;
                }

            else if (xx > PULSE_DN_X1+1 && xx < PULSE_DN_X2-1 && yy > PULSE_DN_Y1+1 && yy < PULSE_DN_Y2-1)
                {
                uPulse-=10;
                }

              else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
                {

                  //try to take the semaphore from the display applet. wait here if we cant take it.
                  xSemaphoreTake(xPWMAppletRunningSemaphore, portMAX_DELAY);
                  //delete the display applet task if its been created.
                  if (xTaskDiagPWMAppletHandle != NULL)
                    {
                      vTaskDelete(xTaskDiagPWMAppletHandle);
                      vTaskDelay(100);
                      xTaskDiagPWMAppletHandle = NULL;
                    }


                  //return the semaphore for taking by another task.
                  xSemaphoreGive(xPWMAppletRunningSemaphore);
                  return 1;

                }


            ///////////////////////////////////////////////////
            TIM_TimeBaseStructure.TIM_Period = uARR;
            TIM_TimeBaseStructure.TIM_Prescaler = uPrescaler; //clock prescaler
            TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV4; // does fuck all with timer unless using input pins to clock

            TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
            TIM_TimeBaseInit( TIM3, &TIM_TimeBaseStructure );
            TIM_ARRPreloadConfig( TIM3, ENABLE );

            TIM_OCStructInit( &TIM_OCInitStruct );
            TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
            TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
            TIM_OCInitStruct.TIM_Pulse = uPulse; // value loaded into the capture compare register for this channel.
            TIM_OC3Init( TIM3, &TIM_OCInitStruct ); // init OC3
            //TIM_OCInitStruct.TIM_Pulse = 0xF0;  // value loaded into the capture compare register for this channel.
            TIM_OC4Init( TIM3, &TIM_OCInitStruct ); // init OC4
            //TIM_Cmd( TIM3, ENABLE );
            //printf("%x, %x\r\n", TIM3->PSC, TIM3->ARR );

          vTaskDelay(10);
        return 0; // xx > 200 && yy > 200;
}







//dynamic applet
void  vTaskDiagPWMApplet( void *pvParameters){

        //char lcd_string[20];
        static int count = 0;
        float fTempHLT = 0, fTempMash = 0, fTempCabinet = 0;

        for (;;)
        {
            xSemaphoreTake(xPWMAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                            //return to the menu system until its returned
            double fTickFreq = lfClock/(uPrescaler+1);
            double fWrapFreq = lfClock/((uPrescaler+1) * (uARR+1));
            double fPulseWidth = uPulse/fTickFreq;
            count++;
                //portENTER_CRITICAL();
                lcd_fill(1,179, 300, 59, Black);
                lcd_printf(1, 11, 10, "tf = %2.2u.%u", (unsigned int)floor(fTickFreq), (unsigned int)((fTickFreq-floor(fTickFreq))*pow(10, 3)));
                lcd_printf(17, 11, 10,"wf = %2.2u.%u", (unsigned int)floor(fWrapFreq), (unsigned int)((fWrapFreq-floor(fWrapFreq))*pow(10, 3)));

                lcd_printf(1, 12, 10, "ARR = %u", uARR);
                lcd_printf(1, 13, 10, "Prescaler = %u",uPrescaler);
                lcd_printf(1, 14, 10, "Pulse = %u, %u.%u", uPulse, (unsigned int)floor(fPulseWidth), (unsigned int)((fPulseWidth-floor(fPulseWidth))*pow(10, 3)));

                //portEXIT_CRITICAL();

                xSemaphoreGive(xPWMAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);

        }
}
////////////////////////////////////////////////////////////////////////////



