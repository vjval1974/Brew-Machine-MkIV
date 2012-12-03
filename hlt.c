/*
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "adc.h"
#include "ds1820.h"
#include "hlt.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include "task.h"

#define HEATING 1
#define OFF 0
static char hlt_state = OFF;

xTaskHandle xHeatHLTTaskHandle = NULL, xHLTAppletDisplayHandle = NULL;
static float diag_setpoint = 74;

void vHLTAppletDisplay( void *pvParameters);


void hlt_init()
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_SSR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
  GPIO_Init( HLT_SSR_PORT, &GPIO_InitStructure );
  GPIO_ResetBits(HLT_SSR_PORT, HLT_SSR_PIN);

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_LEVEL_CHECK_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;// Input Pulled-up
  GPIO_Init( HLT_LEVEL_CHECK_PORT, &GPIO_InitStructure );

  adc_init(); //initialise the ADC1 so we can use a channel from it for the hlt level

}

float fGetHLTLevel(void){
  short raw_adc_value = read_adc(HLT_LEVEL_ADC_CHAN);
  float litres = 0;
  //scale the adc value to indicate the amount of litres left in the HLT
  //printf("rawADC = %l , %2.2f\r\n", raw_adc_value, (float)raw_adc_value);
  litres = (float)((float)raw_adc_value/(float)HLT_ANALOGUE_MAX) * (float)HLT_MAX_LITRES;
  if ((GPIO_ReadInputDataBit(HLT_LEVEL_CHECK_PORT, HLT_LEVEL_CHECK_PIN)^1) || (litres < 4.0))
      {
      return litres;
      }
  return 0;

}

void vTaskHeatHLT( void * pvParameters)
{


  float * setpoint = (float *)pvParameters;
  float actual = 0.0;
  float hlt_level = 0.0;
  char hlt_ok = 0;
  if (setpoint == NULL)
    setpoint = &diag_setpoint;


 for(;;)
    {
     hlt_level =  fGetHLTLevel();
    // ensure we have water above the elements

   if (hlt_level > 4.0)
        {
          //check the temperature
          //printf("heating task - checking temp\r\n");
          actual = ds1820_get_temp(HLT);
          //printf("heating task - checking setpoint\r\n");
          // output depending on temp
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
      else //DONT HEAT... WE WILL BURN OUT THE ELEMENT
        {
          GPIO_WriteBit(HLT_SSR_PORT, HLT_SSR_PIN, 0); //make sure its off
  //        printf("HLT Level low\r\n");
          hlt_state = OFF;
        }
      vTaskDelay(100); // 100us delay is fine for heating
      taskYIELD();
    }



}


#define SETPOINT_UP_X1 0
#define SETPOINT_UP_Y1 30
#define SETPOINT_UP_X2 150
#define SETPOINT_UP_Y2 100
#define SETPOINT_UP_W (SETPOINT_UP_X2-SETPOINT_UP_X1)
#define SETPOINT_UP_H (SETPOINT_UP_Y2-SETPOINT_UP_Y1)

#define SETPOINT_DN_X1 0
#define SETPOINT_DN_Y1 105
#define SETPOINT_DN_X2 150
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

#define BK_X1 200
#define BK_Y1 200
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)
volatile char self_destruct = 0;
void vHLTApplet(int init){
  if (init)
        {
                lcd_DrawRect(SETPOINT_UP_X1, SETPOINT_UP_Y1, SETPOINT_UP_X2, SETPOINT_UP_Y2, Red);
                lcd_fill(SETPOINT_UP_X1+1, SETPOINT_UP_Y1+1, SETPOINT_UP_W, SETPOINT_UP_H, Blue);
                lcd_DrawRect(SETPOINT_DN_X1, SETPOINT_DN_Y1, SETPOINT_DN_X2, SETPOINT_DN_Y2, Red);
                lcd_fill(SETPOINT_DN_X1+1, SETPOINT_DN_Y1+1, SETPOINT_DN_W, SETPOINT_DN_H, Blue);
                lcd_DrawRect(STOP_HEATING_X1, STOP_HEATING_Y1, STOP_HEATING_X2, STOP_HEATING_Y2, Cyan);
                lcd_fill(STOP_HEATING_X1+1, STOP_HEATING_Y1+1, STOP_HEATING_W, STOP_HEATING_H, Red);
                lcd_DrawRect(START_HEATING_X1, START_HEATING_Y1, START_HEATING_X2, START_HEATING_Y2, Cyan);
                lcd_fill(START_HEATING_X1+1, START_HEATING_Y1+1, START_HEATING_W, START_HEATING_H, Green);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(10,1,18,  "MANUAL HLT APPLET");
                lcd_printf(4,4,11,  "SETPOINT_UP");
                lcd_printf(4,8,13,  "SETPOINT DOWN");
                lcd_printf(22,4,13, "START HEATING");
                lcd_printf(22,8,12, "STOP HEATING");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vHLTAppletDisplay,
                    ( signed portCHAR * ) "hlt_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xHLTAppletDisplayHandle );
                self_destruct = 0;
        }

}


void vHLTAppletDisplay( void *pvParameters){
        static char tog = 0;
        static char last_state;
        float hlt_level;
        float diag_setpoint1; // = diag_setpoint;
        char hlt_ok = 0;
        for(;;)
        {

            hlt_level = fGetHLTLevel();
            diag_setpoint1 = diag_setpoint;
            lcd_fill(1,176, 180,40, Black);
//
            if (hlt_level > 4.0)
              lcd_printf(1,11,20,"level OK");
            else
              lcd_printf(1,11,20,"level LOW");

            printf("0wm = %d\r\n",uxTaskGetStackHighWaterMark(NULL));

            printf("1wm = %d\r\n",uxTaskGetStackHighWaterMark(NULL));
            lcd_printf(1,12,30,"level = %2.2f litres", hlt_level);
            printf("2wm = %d\r\n",uxTaskGetStackHighWaterMark(NULL));
            //xTaskResumeAll();
            printf("state %d,  wm = %d\r\n", last_state, uxTaskGetStackHighWaterMark(xHLTAppletDisplayHandle));
                switch (hlt_state)
                {
                case HEATING:
                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,20, Black);
                                lcd_printf(1,13,15,"HEATING to degC");
                        }
                        else{
                                lcd_fill(1,210, 180,20, Black);
                        }
                        break;
                }
                case OFF:
                {
                        if(tog)
                        {
                                lcd_fill(1,210, 180,20, Black);
                                lcd_printf(1,13,11,"NOT HEATING");
                        }
                        else
                          {
                                lcd_fill(1,210, 180,20, Black);
                          }

                        break;
                }
                default:
                {
                        break;
                }
                }
                //              taskYIELD();
                tog = tog ^ 1;
                //printf("%d,%d\n",last_state, crane_state);
                last_state = hlt_state;

                vTaskDelay(500);
                if (self_destruct)
                  vTaskDelete(NULL);
                //lcd_printf(2,11,20,"SP=%2.2lfdeg", 43.22);


        }
}


int HLTKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  //printf("(%d, %d)\r\n", xx, yy);
  if (xx > SETPOINT_UP_X1+1 && xx < SETPOINT_UP_X2-1 && yy > SETPOINT_UP_Y1+1 && yy < SETPOINT_UP_Y2-1)
    {
      //printf("Setpoint-Up button Pressed...\r\n");
      diag_setpoint+=0.5;
      printf("Setpoint is now %2.2f\r\n", diag_setpoint);
      //lcd_printf(10,13, 10, "SP = %2.2f", diag_setpoint);
    }
  else if (xx > SETPOINT_DN_X1+1 && xx < SETPOINT_DN_X2-1 && yy > SETPOINT_DN_Y1+1 && yy < SETPOINT_DN_Y2-1)

    {
      //printf("Setpoint-Dn button Pressed...\r\n");
      diag_setpoint-=0.5;
      printf("Setpoint is now %2.2f\r\n", diag_setpoint);
     // lcd_printf(10,13, 10, "SP = %2.2f", diag_setpoint);
    }
  else if (xx > STOP_HEATING_X1+1 && xx < STOP_HEATING_X2-1 && yy > STOP_HEATING_Y1+1 && yy < STOP_HEATING_Y2-1)
    {
      printf("Deleting Heating Task\r\n");
      if (xHeatHLTTaskHandle != NULL)
        {
          vTaskDelete(xHeatHLTTaskHandle);
          xHeatHLTTaskHandle = NULL;
        }

    }
  else if (xx > START_HEATING_X1+1 && xx < START_HEATING_X2-1 && yy > START_HEATING_Y1+1 && yy < START_HEATING_Y2-1)
    {
      printf("Checking if task already running...\r\n");
      if (xHeatHLTTaskHandle == NULL)
        {
          printf("No previous HLT Heating task\r\n");
          printf("Creating Task\r\n");
          xTaskCreate( vTaskHeatHLT,
              ( signed portCHAR * ) "HLT_HEAT",
              configMINIMAL_STACK_SIZE +800,
              NULL,
              tskIDLE_PRIORITY+2,
              &xHeatHLTTaskHandle );
        }
      else
        {
          printf("Heating task already running\r\n");
        }
    }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      if (xHLTAppletDisplayHandle != NULL)
        {
          self_destruct = 1;
        /*  vTaskDelete(xHLTAppletDisplayHandle);
          taskYIELD();
          vTaskDelay(100);*/
          xHLTAppletDisplayHandle = NULL;
        }

      if (xHeatHLTTaskHandle != NULL)
        {
          vTaskDelete(xHeatHLTTaskHandle);
          taskYIELD();
          vTaskDelay(100);
          xHeatHLTTaskHandle = NULL;
        }
      return 1;

    }

  vTaskDelay(10);
  return 0;

}


