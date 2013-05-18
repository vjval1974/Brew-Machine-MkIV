

#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "Flow1.h"

#include "adc.h"
#include "ds1820.h"
#include "hlt.h"
#include "lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "task.h"
#include "semphr.h"

// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xFlow1AppletRunningSemaphore;




xQueueHandle xHLTLitresQueue;

#define FLOWING 1
#define NOT_FLOWING 0
volatile unsigned long pulses = 0;
const float fLitresPerPulseL = (0.0033/2)*1.5;

const float fLitresPerPulseH = (0.0042/2)*1.5;

const unsigned long ulLowerThresh = 26/2;
float fLitresDelivered = 0;
volatile uint8_t flow_state = NOT_FLOWING;




xTaskHandle xLitresDeliveredHandle = NULL, xFlow1AppletDisplayHandle = NULL;


void vTaskLitresDelivered ( void * pvParameters );
void vFlow1AppletDisplay( void *pvParameters);

void vResetFlow1( void );
void vFlow1Init( void )
{


  // ----------------------------------------
  // Setting up PE4 for an EXTERNAL INTERRUPT
  //-----------------------------------------
  GPIO_InitTypeDef GPIO_InitStructure;


  // Set up the input pin configuration for PE4
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_FLOW_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init( HLT_FLOW_PORT, &GPIO_InitStructure );

  // Make sure the peripheral clocks are enabled.
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

  // Set up Port E, Pin 4 as an external interrupt.
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource4);

  // Set up the External interrupt line 4
  EXTI_InitTypeDef EXTI_InitStructure;
  EXTI_InitStructure.EXTI_Line = EXTI_Line4;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  // Set up the Nested Vector Interrupt Controller for EXTI4

  NVIC_InitTypeDef NVIC_InitStructure;

  /* Enable and set EXTI Interrupt to the lowest priority */
  NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

  NVIC_Init(&NVIC_InitStructure);

  // Test code to generate an input on line 4 ... 0x10 = line 4
  //------------------------------------------------------------
  EXTI->SWIER = 0x10; // generates interrupt
  //clears interrupt
  //EXTI_ClearFlag(EXTI_Line4);


  // Create a queue capable of containing 2 unsigned long values.
    xHLTLitresQueue = xQueueCreate( 2, sizeof( unsigned long ) );
    vSemaphoreCreateBinary(xFlow1AppletRunningSemaphore);

    printf("Flow Sensor Initialised\r\n");

}

// interrupt service routine for external input line 4 (PA4,PB4...PF4)
void EXTI4_IRQHandler(void)
{
  pulses++;
  //printf("external interrupt triggered %f \r\n", 5.3221);
  EXTI_ClearITPendingBit(EXTI_Line4); // need to clear the bit before leaving.
  portEND_SWITCHING_ISR(pdFALSE);
}


void vTaskLitresDelivered ( void * pvParameters )
{
  unsigned long * xMessage;
  static unsigned long ulPulsesLast = 0;
  unsigned long ulPulsesSinceLast;
//

    portBASE_TYPE xStatus;
    for (;;)
      {
        // check the queue for a message
        xStatus = xQueueReceive(xHLTLitresQueue, &(xMessage), 500); // wait 0.5 seconds for a message (DONT CHANGE)
        if (xStatus == pdTRUE) // message recieved
          {
            //printf("received message: %d\r\n", *xMessage);
            pulses = 0; // probably what we want to do here
            ulPulsesSinceLast = 0;
            fLitresDelivered = 0.001;
            //vTaskDelay(1000);

          }
        else // no message
          {
            ulPulsesSinceLast = pulses - ulPulsesLast;
            ulPulsesLast = pulses;

            // Calculates the accumulated litres delivered by deriving the amount of pulses
            // that have occurred in the last half second, then determining which multiplier to
            // use to make the next calculation depending on the rate of flow.
            if (ulPulsesSinceLast <= ulLowerThresh)
              {
                fLitresDelivered = fLitresDelivered + ((float)ulPulsesSinceLast * fLitresPerPulseL);
              }
            else
              {
                fLitresDelivered = fLitresDelivered + ((float)ulPulsesSinceLast * fLitresPerPulseH);
              }
            if (fLitresDelivered < 0.01 || fLitresDelivered > 50000)
              fLitresDelivered = 0.001;
            if (ulPulsesSinceLast > 0)
              flow_state = FLOWING;
            else
              flow_state = NOT_FLOWING;
          }
        taskYIELD();



      }

}


void vResetFlow1(void)
{
  // just some test code to do with lockups etc.
 int x = 1;
 int * xx = &x;


 xQueueSendToBack(xHLTLitresQueue, xx, portMAX_DELAY);
}

void vResetFlow2(void)
{

}

float fGetFlow1Litres(void)
{
  //printf("%2.1f\r\n", fLitresDelivered);
  return fLitresDelivered;
}
#define RESET_FLOW1_X1 0
#define RESET_FLOW1_Y1 30
#define RESET_FLOW1_X2 100
#define RESET_FLOW1_Y2 100
#define RESET_FLOW1_W (RESET_FLOW1_X2-RESET_FLOW1_X1)
#define RESET_FLOW1_H (RESET_FLOW1_Y2-RESET_FLOW1_Y1)

#define RESET_FLOW2_X1 0
#define RESET_FLOW2_Y1 105
#define RESET_FLOW2_X2 100
#define RESET_FLOW2_Y2 175
#define RESET_FLOW2_W (RESET_FLOW2_X2-RESET_FLOW2_X1)
#define RESET_FLOW2_H (RESET_FLOW2_Y2-RESET_FLOW2_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)


void vFlow1Applet(int init){
  if (init)
        {

      lcd_DrawRect(RESET_FLOW1_X1, RESET_FLOW1_Y1, RESET_FLOW1_X2, RESET_FLOW1_Y2, Red);
                      lcd_fill(RESET_FLOW1_X1+1, RESET_FLOW1_Y1+1, RESET_FLOW1_W, RESET_FLOW1_H, Blue);
                      lcd_DrawRect(RESET_FLOW2_X1, RESET_FLOW2_Y1, RESET_FLOW2_X2, RESET_FLOW2_Y2, Red);
                      lcd_fill(RESET_FLOW2_X1+1, RESET_FLOW2_Y1+1, RESET_FLOW2_W, RESET_FLOW2_H, Blue);
                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
                lcd_printf(1,4,11,  "Reset Flow1");
                lcd_printf(1,8,13,  "Reset Flow2");
                lcd_printf(30, 13, 4, "Back");
                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task


                xTaskCreate( vFlow1AppletDisplay,
                    ( signed portCHAR * ) "f1_disp",
                    configMINIMAL_STACK_SIZE + 800,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xFlow1AppletDisplayHandle );

        }

}





void vFlow1AppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        double ml;
        unsigned long mlu = 0;
        for(;;)
        {

            xSemaphoreTake(xFlow1AppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                    //return to the menu system until its returned
            ml = fLitresDelivered * 1000;
            mlu = (unsigned long)ml;

            switch (flow_state)
            {
            case FLOWING:
              {
                if(tog)
                  {
                    //printf("%u\r\n",uxTaskGetStackHighWaterMark(NULL));
                    lcd_fill(1,220, 180,29, Black);
                    //lcd_printf(1,13,15,"FLOWING\n");

                    //lcd_printf(1,14,15,"Currently @ %2.1flitres", fLitresDelivered);
                    lcd_printf(1, 13, 25, "Currently @ %d.%dl", (unsigned int)floor(fGetFlow1Litres()), (unsigned int)((fGetFlow1Litres()-floor(fGetFlow1Litres()))*pow(10, 3)));
                    //lcd_printf(1,14,15,"Currently @ %dml", mlu);
                  }
                else{
                    lcd_fill(1,210, 180,17, Black);
                }
                break;
              }
            case NOT_FLOWING:
              {
                if(tog)
                  {
                    //printf("%u\r\n",uxTaskGetStackHighWaterMark(NULL));
                    lcd_fill(1,210, 180,29, Black);
                    //lcd_printf(1,13,11,"NOT FLOWING\n");
                    lcd_printf(1, 13, 25, "Currently @ %d.%d ml", (unsigned int)floor(fGetFlow1Litres()), (unsigned int)((fGetFlow1Litres()-floor(fGetFlow1Litres()))*pow(10, 3)));
                    //lcd_printf(1,14,15,"Currently @ %dml", mlu);

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
              //printf("%d, %d, %d\r\n", (uint8_t)diag_setpoint, (diag_setpoint), ((uint8_t)diag_setpoint*10)%5);
               // lcd_printf(5,1,25, "Flow 1 = %2.1f Litres", fGetFlow1Litres());

                xSemaphoreGive(xFlow1AppletRunningSemaphore); //give back the semaphore as its safe to return now.

                vTaskDelay(200);


        }
}

int iFlow1Key(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  if (xx > RESET_FLOW1_X1+1 && xx < RESET_FLOW1_X2-1 && yy > RESET_FLOW1_Y1+1 && yy < RESET_FLOW1_Y2-1)
      {
        vResetFlow1();
       //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
      }
    else if (xx > RESET_FLOW2_X1+1 && xx < RESET_FLOW2_X2-1 && yy > RESET_FLOW2_Y1+1 && yy < RESET_FLOW2_Y2-1)

      {
        vResetFlow2();
       //printf("Setpoint is now %d\r\n", (uint8_t)diag_setpoint);
      }
    else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xFlow1AppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xFlow1AppletDisplayHandle != NULL)
        {
          vTaskDelete(xFlow1AppletDisplayHandle);
          vTaskDelay(100);
          xFlow1AppletDisplayHandle = NULL;
        }
      //delete the heating task
      xSemaphoreGive(xFlow1AppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}


