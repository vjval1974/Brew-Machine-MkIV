//-------------------------------------------------------------------------
// Author: Brad Goold
// Date: 18 Feb 2012
// Email Address: W0085400@umail.usq.edu.au
// 
// Purpose:
// Pre:
// Post:
// RCS $Date$
// RCS $Revision$
// RCS $Source$
// RCS $Log$
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "crane.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"


#define UP 1
#define DN 255
#define DRIVING 1
#define STOPPED -1


#define TIMER_USED TIM3



xQueueHandle xCraneQueueHandle;

xSemaphoreHandle xAppletRunningSemaphore;

xTaskHandle xCraneTaskHandle = NULL, xCraneAppletDisplayHandle = NULL;
void vTaskCrane(void * pvParameters);


struct Step
{
  unsigned char ucDirection; //1 = up, -1 = down
  int uSpeed; //final speed in percent
  int uTime; //acceleration delay (controls rate of acceleration)

  //        char ucMessageID;
  //        char ucData[ 20 ];
};

struct Step LimitStop = {UP, 0, 0};
struct Step STOP_Step = {UP, 0, 5};
struct Step UP_Step = {UP, 100, 10};
struct Step DN_Step = {DN, 100, 10};

//struct Step xSteps[] = {
//      {UP,100,1},
//      {DN,50,1}
//};

//struct Step MashStirSteps[] = {
//      {UP,100,10},
//      {UP,0,10},
//      {DN,50,5},
//      {DN,75,5},
//      {DN,100,5},
//      {DN,0,10},
//      {UP,100,1},
//      {UP,100,1},
//      {UP,100,1},
//      {DN,50,1},
//      {0, 0, 0}
//};

xQueueHandle xCraneQueue1, xCraneQueue2;
#if 1
void vCraneInit(void)
{
  //unsigned long ulFrequency;
  //GPIO_InitTypeDef GPIO_InitStructure;
  unsigned long ulFrequency;
          TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
          NVIC_InitTypeDef NVIC_InitStructure;
          GPIO_InitTypeDef GPIO_InitStructure;

          GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

           //Initialise Ports, pins and timer
          TIM_DeInit( TIMER_USED );
          TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );


          // Enable timer clocks
          RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );



          //CRANE STEPPER DRIVE PINS ARE USING THE ALTERNATE FUNCTION OF THEIR DEFAULTS.
          //THEY TAKE THE OUTPUT COMPARE FUNCTION OF TIMER 3 TO DRIVE WITH PWM VIA REMAPPING
          GPIO_InitStructure.GPIO_Pin =  CRANE_STEP_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;// Alt Function - Push Pull
          GPIO_Init( CRANE_DRIVE_PORT, &GPIO_InitStructure );




          //CONTROL PINS
          GPIO_InitStructure.GPIO_Pin =  CRANE_ENABLE_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
          GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
          GPIO_ResetBits(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN);

          GPIO_InitStructure.GPIO_Pin =  CRANE_DIR_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
          GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
          GPIO_SetBits(CRANE_CONTROL_PORT, CRANE_DIR_PIN);


          //LIMIT INPUTS
          GPIO_InitStructure.GPIO_Pin =  CRANE_UPPER_LIMIT_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
          GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

          GPIO_InitStructure.GPIO_Pin =  CRANE_LOWER_LIMIT_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
          GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

          // SET UP TIMER 3  FOR PWM
          GPIO_PinRemapConfig( GPIO_FullRemap_TIM3, ENABLE );// Map TIM3_CH3 and CH4 to Step Pins
          //Period 10000, Prescaler 50, pulse = 26 gives 111Hz with 16us
          //Pulse width.
          TIM_TimeBaseStructure.TIM_Period = 20000; // this means nothing.. original val loaded into ARR... its changed later
          TIM_TimeBaseStructure.TIM_Prescaler = 50; //clock prescaler
          TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV4; // does fuck all except for using input pin for counter
          TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
          TIM_TimeBaseInit( TIMER_USED, &TIM_TimeBaseStructure );
          TIM_ARRPreloadConfig( TIMER_USED, ENABLE );

          TIM_OCInitTypeDef TIM_OCInitStruct;
          TIM_OCStructInit( &TIM_OCInitStruct );
          TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
          TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
          TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_Low; // Added this line to see if it goes active low.
          // Initial duty cycle equals 25 which gives pulse of 5us
          TIM_OCInitStruct.TIM_Pulse = 25;
          TIM_OC3Init( TIMER_USED, &TIM_OCInitStruct );
          TIM_OC4Init( TIMER_USED, &TIM_OCInitStruct );
          //TIM_Cmd( TIMER_USED, ENABLE );
          printf("Crane Initialised\r\n");

          TIM_SetAutoreload(TIMER_USED, 15000);

          vSemaphoreCreateBinary(xAppletRunningSemaphore);

          //xCraneQueueHandle = xQueueCreate(10, sizeof(struct Step *));

          // Create a queue capable of containing 10 unsigned long values.
          //xCraneQueue1 = xQueueCreate( 10, sizeof( unsigned long ) );

          // Create a queue capable of containing 10 pointers to AMessage structures.
          // These should be passed by pointer as they contain a lot of data.
          xCraneQueue2 = xQueueCreate( 1, sizeof( struct Step * ) );

          xTaskCreate( vTaskCrane,
              ( signed portCHAR * ) "Crane",
              configMINIMAL_STACK_SIZE +500,
              NULL,
              tskIDLE_PRIORITY,
              &xCraneTaskHandle );

}
#endif
// *************************************************************
// Testing below.
// *************************************************************
#if 0
void vCraneInit(void)
{
  //unsigned long ulFrequency;
  //GPIO_InitTypeDef GPIO_InitStructure;
  unsigned long ulFrequency;
          TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
          NVIC_InitTypeDef NVIC_InitStructure;
          GPIO_InitTypeDef GPIO_InitStructure;

          GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

           //Initialise Ports, pins and timer
          TIM_DeInit( TIM1 );
          TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );


          // Enable timer clocks
          RCC_APB1PeriphClockCmd( RCC_APB2Periph_TIM1, ENABLE );



          //CRANE STEPPER DRIVE PINS ARE USING THE ALTERNATE FUNCTION OF THEIR DEFAULTS.
          //THEY TAKE THE OUTPUT COMPARE FUNCTION OF TIMER 3 TO DRIVE WITH PWM VIA REMAPPING
          GPIO_InitStructure.GPIO_Pin =  CRANE_STEP_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;// Alt Function - Push Pull
          GPIO_Init( CRANE_DRIVE_PORT, &GPIO_InitStructure );




          //CONTROL PINS
          GPIO_InitStructure.GPIO_Pin =  CRANE_ENABLE_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
          GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
          GPIO_ResetBits(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN);

          GPIO_InitStructure.GPIO_Pin =  CRANE_DIR_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
          GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
          GPIO_SetBits(CRANE_CONTROL_PORT, CRANE_DIR_PIN);


          //LIMIT INPUTS
          GPIO_InitStructure.GPIO_Pin =  CRANE_UPPER_LIMIT_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
          GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

          GPIO_InitStructure.GPIO_Pin =  CRANE_LOWER_LIMIT_PIN;
          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
          GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

          // SET UP TIMER 3  FOR PWM
          //GPIO_PinRemapConfig( GPIO_FullRemap_TIM3, ENABLE );// Map TIM3_CH3 and CH4 to Step Pins
          //Period 10000, Prescaler 50, pulse = 26 gives 111Hz with 16us
          //Pulse width.
          TIM_TimeBaseStructure.TIM_Period = 20000; // this means nothing.. original val loaded into ARR... its changed later
          TIM_TimeBaseStructure.TIM_Prescaler = 50; //clock prescaler
          TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV4; // does fuck all except for using input pin for counter
          TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
          TIM_TimeBaseInit( TIM1, &TIM_TimeBaseStructure );
          TIM_ARRPreloadConfig( TIM1, ENABLE );

          TIM_OCInitTypeDef TIM_OCInitStruct;
          TIM_OCStructInit( &TIM_OCInitStruct );
          TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
          TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
          TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_Low; // Added this line to see if it goes active low.
          // Initial duty cycle equals 25 which gives pulse of 5us
          TIM_OCInitStruct.TIM_Pulse = 25;
          TIM_OC3Init( TIM1, &TIM_OCInitStruct );
          TIM_OC4Init( TIM1, &TIM_OCInitStruct );
          //TIM_Cmd( TIMER_USED, ENABLE );
          printf("Crane Initialised\r\n");

          TIM_SetAutoreload(TIM1, 15000);

          vSemaphoreCreateBinary(xAppletRunningSemaphore);

          //xCraneQueueHandle = xQueueCreate(10, sizeof(struct Step *));

          // Create a queue capable of containing 10 unsigned long values.
          //xCraneQueue1 = xQueueCreate( 10, sizeof( unsigned long ) );

          // Create a queue capable of containing 10 pointers to AMessage structures.
          // These should be passed by pointer as they contain a lot of data.
          xCraneQueue2 = xQueueCreate( 1, sizeof( struct Step * ) );

          xTaskCreate( vTaskCrane,
              ( signed portCHAR * ) "Crane",
              configMINIMAL_STACK_SIZE +500,
              NULL,
              tskIDLE_PRIORITY,
              &xCraneTaskHandle );

}

// *************************************************************
#endif



volatile int8_t crane_state = STOPPED;

#define MAX_SPEED_ARR 11800 //The maximum speed. (the lower, the faster)
#define MIN_SPEED_ARR 35000 //Motor stopped ARR value
#define SPEED_RANGE (MIN_SPEED_ARR - MAX_SPEED_ARR)

void vCraneFunc(struct Step * step)
{
  TIM_Cmd( TIMER_USED, ENABLE );
  uint16_t uInitialSpeed = TIMER_USED->ARR, uSetSpeed;
  static uint16_t uCurrentSpeed = MIN_SPEED_ARR;
 // printf("-----------------------\r\n");
 // printf("**vCraneFunc called\r\n");
  uint8_t limit = 0xFF;
  if (step->uSpeed == 0 && step->uTime == 0 && step->ucDirection == UP )
  //  printf("limit stop step called\r\n");

  if (step->ucDirection == UP)
    GPIO_WriteBit(CRANE_CONTROL_PORT, CRANE_DIR_PIN, 1);
  else
    GPIO_WriteBit(CRANE_CONTROL_PORT, CRANE_DIR_PIN, 0);

  GPIO_WriteBit(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, ON);

  uSetSpeed = (MIN_SPEED_ARR - (step->uSpeed * (SPEED_RANGE/100))); //convert speed % to ARR value
  uCurrentSpeed = TIMER_USED->ARR;

  if (uSetSpeed > uInitialSpeed) //are we increasing from here or decreasing?
    {
    //  printf("Decreasing\r\n");
      //Decrease Speed
      //up counting decreases speed... then stop when steps get too large
      for (uCurrentSpeed = uInitialSpeed; uCurrentSpeed <= uSetSpeed ; uCurrentSpeed+=100)
        {
          limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN) &
              GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

          if (limit == 0 || uCurrentSpeed >= MIN_SPEED_ARR || step->uTime == 0)
            {
              TIM_Cmd(TIMER_USED, DISABLE);
              TIM_SetAutoreload(TIMER_USED, MIN_SPEED_ARR);
              crane_state = STOPPED;
      //        printf("timer3 disabled\r\n");
              GPIO_WriteBit(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, OFF);
              break;
            }
          TIM_SetAutoreload(TIMER_USED, uCurrentSpeed);
          vTaskDelay(step->uTime); //controls the acceleration time
        }

    }
  else
    {
      //printf("Increasing\r\n");
      //increase speed
      for (uCurrentSpeed = uInitialSpeed; uCurrentSpeed > uSetSpeed; uCurrentSpeed-=100)
        {

          limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN) &
              GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
          vTaskDelay(step->uTime);
          TIM_SetAutoreload(TIMER_USED, uCurrentSpeed);
          //printf("Counter = %d\r\n", TIMER_USED->CNT);
          if (limit == 0)
            {
              TIM_SetAutoreload(TIMER_USED, MIN_SPEED_ARR);

              TIM_Cmd(TIMER_USED, DISABLE);
              crane_state = STOPPED; // Not correct code... used for testing at the moment
              //GPIO_WriteBit(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, OFF);
              break;
            }

        }
    }
  uint8_t l_limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
  limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
  //printf("initial speed was = %u\r\n", uInitialSpeed);
  //printf("current speed is = %u\r\n", uCurrentSpeed);
  //printf("set speed is  = %u\r\n", uSetSpeed);
  //printf("current state of the upper limit is %u\r\n", limit );
  //printf("current state of the lower limit is %u\r\n", l_limit );
  if (uCurrentSpeed < MIN_SPEED_ARR)
    crane_state = DRIVING;
  else crane_state =  STOPPED;

  //printf("**vCraneFunc Returning\r\n");
  //printf("---------------------------\r\n\n");
}


void vTaskCrane(void * pvParameters)
{
  struct Step * xMessage;
  uint8_t limit = 0xFF; //neither on or off.
  portBASE_TYPE xStatus;
  for (;;)
    {


      xStatus = xQueueReceive(xCraneQueue2, &(xMessage), 500); // Check the queue every 0.5 second for a new command
      if (xStatus == pdTRUE)
        {
    //      printf("received step: %d\r\n", xMessage->uTime);
          vCraneFunc(xMessage);
          //need to check for the limit at the top/bottom
          //... but also need to code some logic in case the limit is reached whilst accelerating etc.
          ////

          while (crane_state == DRIVING)
            {
              //printf("%d, ", xMessage->ucDirection);

              if (xMessage->ucDirection == UP)
                {
                  limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);

                  if (limit == 0)
                    {
      //                printf("Stopping on limit\r\n");
                      vCraneFunc(&LimitStop); //stop the crane on an instant.

                      break;

                    }
                }
              else if (xMessage->ucDirection == DN)
                {
                  limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

                  if (limit == 0)
                    {
        //              printf("Stopping on limit\r\n");
                      vCraneFunc(&LimitStop); //stop the crane on an instant.

                      break;
                    }
                }
              taskYIELD();
            }
        }
      else
        {
         // printf("Waiting!\r\n");
          //queue empty
        }


    }
}



void vCraneAppletDisplay( void *pvParameters){
  static char tog = 0; //toggles each loop
  for(;;)
    {

      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
      //return to the menu system until its returned

      //display the state and user info (the state will flash on the screen)
      switch (crane_state)
      {
      case DRIVING:
        {
          if(tog)
            {
              lcd_fill(1,220, 180,29, Black);
              lcd_printf(1,13,15,"DRIVING");
            }
          else{
              lcd_fill(1,210, 180,17, Black);
          }
          break;
        }
      case STOPPED:
        {
          if(tog)
            {
              lcd_fill(1,210, 180,29, Black);
              lcd_printf(1,13,11,"NOT DRIVING");
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

      xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
      vTaskDelay(500);


    }
}

#define UP_X1 0
#define UP_Y1 30
#define UP_X2 150
#define UP_Y2 100
#define UP_W (UP_X2-UP_X1)
#define UP_H (UP_Y2-UP_Y1)

#define DN_X1 0
#define DN_Y1 105
#define DN_X2 150
#define DN_Y2 175
#define DN_W (DN_X2-DN_X1)
#define DN_H (DN_Y2-DN_Y1)



#define ST_X1 155
#define ST_Y1 30
#define ST_X2 300
#define ST_Y2 175
#define ST_W (ST_X2-ST_X1)
#define ST_H (ST_Y2-ST_Y1)

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

void vCraneApplet(int init){
  if (init)
    {
      lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, Red);
      lcd_fill(UP_X1+1, UP_Y1+1, UP_W, UP_H, Blue);
      lcd_DrawRect(DN_X1, DN_Y1, DN_X2, DN_Y2, Red);
      lcd_fill(DN_X1+1, DN_Y1+1, DN_W, DN_H, Blue);
      lcd_DrawRect(ST_X1, ST_Y1, ST_X2, ST_Y2, Cyan);
      lcd_fill(ST_X1+1, ST_Y1+1, ST_W, ST_H, Red);
      lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
      lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);
      lcd_printf(10,1,12,  "MANUAL CRANE");
      lcd_printf(8,4,2, "UP");
      lcd_printf(8,8,2, "DN");
      lcd_printf(26,6,4, "STOP");
      lcd_printf(30, 13, 4, "Back");

      xTaskCreate( vCraneAppletDisplay,
          ( signed portCHAR * ) "Crane_display",
          configMINIMAL_STACK_SIZE +500,
          NULL,
          tskIDLE_PRIORITY + 1,
          &xCraneAppletDisplayHandle );
    }

}



int iCraneKey(int xx, int yy){

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  struct Step *pxMessage;


  if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
    {
      //printf("up pressed\r\n");
      //pxMessage = &xSteps[0];
      //pxMessage = &MashStirSteps[0];
      pxMessage = &UP_Step;
      //while (pxMessage->ucDirection != 0)
      // {
      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, 0 );
      //pxMessage++;
      // }

    }
  else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
    {
      //printf("down pressed\r\n");
      pxMessage = &DN_Step;
      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, 0 );

    }
  else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
    {
      //printf("stop pressed\r\n");
      pxMessage = &STOP_Step;

     // scrappy... needs state to be stopped to break into while loop in vTaskCrane
      crane_state = STOPPED;

      xQueueSendToBack( xCraneQueue2, ( void * ) &pxMessage, portMAX_DELAY );
    }

  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {

      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xCraneAppletDisplayHandle != NULL)
        {
          vTaskDelete(xCraneAppletDisplayHandle);
          vTaskDelay(100);
          xCraneAppletDisplayHandle = NULL;
        }


      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      return 1;

    }

  vTaskDelay(10);
  return 0;

}


