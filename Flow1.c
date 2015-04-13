

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
#include "console.h"
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xFlow1AppletRunningSemaphore;

xQueueHandle xLitresToMashQueue, xLitresToBoilQueue;

#define FLOWING 1
#define NOT_FLOWING 0


//globals
volatile unsigned long ulBoilFlowPulses = 0, ulMashFlowPulses;
const float fBoilLitresPerPulseL = (0.0033);
const float fMashLitresPerPulseL = (0.0033);
const float fBoilLitresPerPulseH = (0.0042);
const float fMashLitresPerPulseH = (0.0042);
const unsigned long ulLowerThresh = 13; //26 pulses per second is the thresh, but we use 0.5seconds
const unsigned long ulUpperThresh = 30; //if we are over this, there is a problem.. dont record
float fLitresDeliveredToBoil = 0, fLitresDeliveredToMash = 0;
volatile uint8_t uBoilFlowState = NOT_FLOWING, uMashFlowState = NOT_FLOWING;




xTaskHandle xLitresToBoilHandle = NULL, xLitresToMashHandle = NULL, xFlow1AppletDisplayHandle = NULL;

// function prototypes
void vFlow1AppletDisplay( void *pvParameters);
void vResetFlow1( void );
void vResetFlow2( void );


void vFlow1Init( void )
{


  // ----------------------------------------
  // Setting up PE4 for an EXTERNAL INTERRUPT
  //-----------------------------------------
  GPIO_InitTypeDef GPIO_InitStructure;


  // Set up the input pin configuration for PE4
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin =  HLT_FLOW_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
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
    xLitresToMashQueue = xQueueCreate( 2, sizeof( unsigned long ) );
    xLitresToBoilQueue = xQueueCreate( 2, sizeof( unsigned long ) );

    vSemaphoreCreateBinary(xFlow1AppletRunningSemaphore);

    printf("Flow Sensor Initialised\r\n");

}

// interrupt service routine for external input line 4 (PA4,PB4...PF4)
void EXTI4_IRQHandler(void)
{
  ulBoilFlowPulses++;
  //printf("external interrupt triggered %f \r\n", 5.3221);
  EXTI_ClearITPendingBit(EXTI_Line4); // need to clear the bit before leaving.
  portEND_SWITCHING_ISR(pdFALSE);
}

void EXTIX_IRQHandler(void)
{
  // Interrupt for flow sensor going into the mash.
  // Change the "X" above and set up the init function for that interrupt.
  //EXTI_ClearITPendingBit(EXTI_LineX); // need to clear the bit before leaving.
  //  portEND_SWITCHING_ISR(pdFALSE);
}

void vTaskLitresToBoil ( void * pvParameters )
{
  unsigned long * xMessage;
  static unsigned long ulPulsesLast = 0;
  unsigned long ulPulsesSinceLast;
  char buf[50];

//

    portBASE_TYPE xStatus;
    for (;;)
      {
        // check the queue for a message
        xStatus = xQueueReceive(xLitresToBoilQueue, &(xMessage), 500); // wait 0.5 seconds for a message (DONT CHANGE)
        if (xStatus == pdTRUE) // message recieved
          {
           vConsolePrint("FLOW1: received Reset\r\n");
            ulBoilFlowPulses = 0; // probably what we want to do here
            ulPulsesSinceLast = 0;
            fLitresDeliveredToBoil = 0.00;
            //vTaskDelay(1000);

          }
        else // no message
          {
            ulPulsesSinceLast = ulBoilFlowPulses - ulPulsesLast;
            ulPulsesLast = ulBoilFlowPulses;

            // Calculates the accumulated litres delivered by deriving the amount of pulses
            // that have occurred in the last half second, then determining which multiplier to
            // use to make the next calculation depending on the rate of flow.
            if (ulPulsesSinceLast <= ulLowerThresh)
              {
                fLitresDeliveredToBoil = fLitresDeliveredToBoil + ((float)ulPulsesSinceLast * fBoilLitresPerPulseL);
              }
            else if (ulPulsesSinceLast > ulLowerThresh && ulPulsesSinceLast <= ulUpperThresh)
              {
                fLitresDeliveredToBoil = fLitresDeliveredToBoil + ((float)ulPulsesSinceLast * fBoilLitresPerPulseH);
              }
            else // we have recorded a bad value, put in the average now.
              {
                fLitresDeliveredToBoil = fLitresDeliveredToBoil + (15.0 * fBoilLitresPerPulseH);
              }
            if (fLitresDeliveredToBoil < 0.01 || fLitresDeliveredToBoil > 50000)
              fLitresDeliveredToBoil = 0.000;
            if (ulPulsesSinceLast > 0 && ulPulsesSinceLast <= ulUpperThresh)
              {
                uBoilFlowState = FLOWING;
                //sprintf(buf, "last:%d,\r\n", ulPulsesSinceLast);
                //vConsolePrint(buf);
              }
            else
              uBoilFlowState = NOT_FLOWING;
          }
        taskYIELD();

      }

}

void vTaskLitresToMash ( void * pvParameters )
{
  unsigned long * xMessage;
  static unsigned long ulPulsesLast = 0;
  unsigned long ulPulsesSinceLast;
//

    portBASE_TYPE xStatus;
    for (;;)
      {
        // check the queue for a message
        xStatus = xQueueReceive(xLitresToMashQueue, &(xMessage), 500); // wait 0.5 seconds for a message (DONT CHANGE)
        if (xStatus == pdTRUE) // message received
          {
            //printf("received message: %d\r\n", *xMessage);
            ulMashFlowPulses = 0; // probably what we want to do here
            ulPulsesSinceLast = 0;
            fLitresDeliveredToMash = 0.001;
            //vTaskDelay(1000);

          }
        else // no message
          {
            ulPulsesSinceLast = ulMashFlowPulses - ulPulsesLast;
            ulPulsesLast = ulMashFlowPulses;

            // Calculates the accumulated litres delivered by deriving the amount of ulBoilFlowPulses
            // that have occurred in the last half second, then determining which multiplier to
            // use to make the next calculation depending on the rate of flow.
            if (ulPulsesSinceLast <= ulLowerThresh)
              {
                fLitresDeliveredToMash = fLitresDeliveredToMash + ((float)ulPulsesSinceLast * fMashLitresPerPulseL);
              }
            else
              {
                fLitresDeliveredToMash = fLitresDeliveredToMash + ((float)ulPulsesSinceLast * fMashLitresPerPulseH);
              }
            if (fLitresDeliveredToMash < 0.01 || fLitresDeliveredToMash > 50000)
              fLitresDeliveredToMash = 0.001;
            if (ulPulsesSinceLast > 0)
              uMashFlowState = FLOWING;
            else
              uMashFlowState = NOT_FLOWING;
          }
        vTaskDelay(50);
        taskYIELD();



      }

}


void vResetFlow1(void)
{
  // just some test code to do with lockups etc.
 int x = 1;
 int * xx = &x;


 xQueueSendToBack(xLitresToBoilQueue, xx, portMAX_DELAY);
}

void vResetFlow2(void)
{
  int xx = 1;
  xQueueSendToBack(xLitresToMashQueue, &xx, portMAX_DELAY);
}

float fGetBoilFlowLitres(void)
{
  //printf("%2.1f\r\n", fLitresDeliveredToBoil);
  return fLitresDeliveredToBoil;
}

float fGetMashFlowLitres(void)
{
  return fLitresDeliveredToMash;
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
            ml = fLitresDeliveredToBoil * 1000;
            mlu = (unsigned long)ml;

            switch (uBoilFlowState)
            {
            case FLOWING:
              {
                if(tog)
                  {
                    //printf("%u\r\n",uxTaskGetStackHighWaterMark(NULL));
                    lcd_fill(1,220, 180,29, Black);
                    //lcd_printf(1,13,15,"FLOWING\n");

                    //lcd_printf(1,14,15,"Currently @ %2.1flitres", fLitresDeliveredToBoil);
                    lcd_printf(1, 13, 25, "Currently @ %d.%dl", (unsigned int)floor(fGetBoilFlowLitres()), (unsigned int)((fGetBoilFlowLitres()-floor(fGetBoilFlowLitres()))*pow(10, 3)));
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
                    lcd_printf(1, 13, 25, "Currently @ %d.%d l", (unsigned int)floor(fGetBoilFlowLitres()), (unsigned int)((fGetBoilFlowLitres()-floor(fGetBoilFlowLitres()))*pow(10, 3)));
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


