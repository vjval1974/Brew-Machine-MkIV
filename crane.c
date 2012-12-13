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
//-------------------------------------------------------------------------
#define CRANE_UPPER_LIMIT_ADC_CHAN 10
#define CRANE_LOWER_LIMIT_ADC_CHAN 11
#define DRIVING_UP 1
#define DRIVING_DN 2
#define STOPPED 0

xTaskHandle xCraneTaskHandle = NULL, xCraneUpToLimitTaskHandle = NULL,  xCraneDnToLimitTaskHandle = NULL, xCraneAppletDisplayHandle = NULL;

static char crane_state = STOPPED;
int test(int x){
	printf("test\r\n");
	return 0;
}

struct StepperSteps  {
		char * step_name;
		int (*setupFunc)(void);
		int (*pollingFunc)(int x);
}StirSteps[];


struct StepperSteps StirSteps[] = {
		{"brad", NULL, test},
		{NULL, NULL, NULL}
};
//-------------------------------------------------------------------------
void vCraneDnToLimitTask( void *pvParameters );
void vCraneUpToLimitTask( void *pvParameters );

void vCraneInit(void){ 

	unsigned long ulFrequency;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	/* Enable timer clocks */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );

	/* Initialise Ports, pins and timer */
	TIM_DeInit( TIM3 );
	TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

	//CRANE STEPPER DRIVE PINS ARE USING THE ALTERNATE FUNCTION OF THEIR DEFAULTS.
	//THEY TAKE THE OUTPUT COMPARE FUNCTION OF TIMER 3 TO DRIVE WITH PWM VIA REMAPPING
	GPIO_InitStructure.GPIO_Pin =  CRANE_STEP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;// Alt Function - Push Pull
	GPIO_Init( CRANE_DRIVE_PORT, &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin =  CRANE_STIR_STEP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //Alt Function
	GPIO_Init( CRANE_STIR_DRIVE_PORT, &GPIO_InitStructure );

	GPIO_PinRemapConfig( GPIO_FullRemap_TIM3, ENABLE );// Map TIM3_CH3 and CH4 to Step Pins

	//CONTROL PINS
	GPIO_InitStructure.GPIO_Pin =  CRANE_ENABLE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
	GPIO_SetBits(CRANE_CONTROL_PORT, CRANE_ENABLE_PIN);

	GPIO_InitStructure.GPIO_Pin =  CRANE_DIR_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init( CRANE_CONTROL_PORT, &GPIO_InitStructure );
	GPIO_SetBits(CRANE_CONTROL_PORT, CRANE_DIR_PIN);

	GPIO_InitStructure.GPIO_Pin =  CRANE_STIR_DIR_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init( CRANE_STIR_CONTROL_PORT, &GPIO_InitStructure );
	GPIO_SetBits(CRANE_STIR_CONTROL_PORT, CRANE_STIR_DIR_PIN);

	GPIO_InitStructure.GPIO_Pin =  CRANE_STIR_ENABLE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init( CRANE_STIR_CONTROL_PORT, &GPIO_InitStructure );
	GPIO_SetBits(CRANE_STIR_CONTROL_PORT, CRANE_STIR_ENABLE_PIN);

	//LIMIT INPUTS
	GPIO_InitStructure.GPIO_Pin =  CRANE_UPPER_LIMIT_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin =  CRANE_LOWER_LIMIT_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init( CRANE_LIMIT_PORT, &GPIO_InitStructure );



	// SET UP TIMER 3  FOR PWM

	//Period 10000, Prescaler 50, pulse = 26 gives 111Hz with 5us
	//Pulse width.
	TIM_TimeBaseStructure.TIM_Period = 10000;
	TIM_TimeBaseStructure.TIM_Prescaler = 50; //clock prescaler
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV4;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit( TIM3, &TIM_TimeBaseStructure );
	TIM_ARRPreloadConfig( TIM3, ENABLE );

	TIM_OCInitTypeDef TIM_OCInitStruct;
	TIM_OCStructInit( &TIM_OCInitStruct );
	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
	// Initial duty cycle equals 25 which gives pulse of 5us
	TIM_OCInitStruct.TIM_Pulse = 26;
	TIM_OC3Init( TIM3, &TIM_OCInitStruct );
	TIM_OC4Init( TIM3, &TIM_OCInitStruct );
	printf("Crane Initialised\r\n");
}



void vCraneStop(){
	printf("stopping...\r\n");

	crane_state = STOPPED;
	//Kill the up and down tasks
	if (xCraneUpToLimitTaskHandle)
	{
                printf("in stop - found up task handle\r\n");
		vTaskDelete(xCraneUpToLimitTaskHandle);
		vTaskDelay(100);
		xCraneUpToLimitTaskHandle = NULL;
	}
	if (xCraneDnToLimitTaskHandle)
	{
	printf("in stop - found dn task handle\r\n");
		vTaskDelete(xCraneDnToLimitTaskHandle);
		vTaskDelay(100);
		xCraneDnToLimitTaskHandle = NULL;
	}
	TIM_Cmd( TIM3, ENABLE );
	uint16_t currentSpeed = TIM3->ARR, setSpeed;
	//up counting decreases speed... then stop when steps get too large
	for (setSpeed = currentSpeed; setSpeed < 40000; setSpeed+=100)
	{

	        //printf("stopping\r\n");

		TIM_SetAutoreload(TIM3, setSpeed);
                vTaskDelay(1);

	}
	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, 1 );
	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_DIR_PIN, 0 );
	TIM_Cmd( TIM3, DISABLE );

	printf("stopped!\r\n");


}




void vCraneAppletDisplay( void *pvParameters){
  static char tog = 0;
  static char last_state;
  for(;;)
    {
//      printf("%d, %d\r\n", last_state, crane_state);
      switch (crane_state)
      {
      case DRIVING_UP:
        {
          if(tog)
            {
              lcd_fill(1,190, 100,20, Red);
              lcd_printf(1,12,20,"Driving UP!");
            }
          else
            lcd_fill(1,190, 100,20, Green);

          break;
        }
      case DRIVING_DN:
        {
          if(tog)
            {
              lcd_fill(1,190, 100,20, Red);
              lcd_printf(1,12,20,"Driving DN!");
            }
          else
            lcd_fill(1,190, 100,20, Green);


          break;
        }
      case STOPPED:
        {
          if(last_state != crane_state)
            {
              lcd_fill(1,190, 100,20, Red);
              lcd_printf(1,12,20,"STOPPED!");
            }
          //else
          //


          break;
        }
      }
      tog = tog ^ 1;
      //printf("%d,%d\n",last_state, crane_state);
      last_state = crane_state;
      vTaskDelay(300);
      taskYIELD();
    }
}

void vCraneUpToLimitTask( void *pvParameters )
{
	uint16_t speed;
	uint32_t ii = 0;
	unsigned char upper_limit = 0;
	printf("Driving crane up to limit\r\n");
	TIM_Cmd( TIM3, ENABLE );
	crane_state = DRIVING_UP;
	char val = 0;
	//down counting increases speed

	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, 0 ); //pull low to enable drive
	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_DIR_PIN, 1 );

	//Ramp Up Speed
	for (speed = 40000; speed > 11800; speed-=100)
	{

//		printf("Ramp up\r\n");
		vTaskDelay(1);
		TIM_SetAutoreload(TIM3, speed);
		//upper_limit = read_adc(CRANE_UPPER_LIMIT_ADC_CHAN);
		upper_limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);
		if (upper_limit == 0)
		{
			printf("STOPPED on Ramp up\r\n");

			vCraneStop();

		}
	}
	//Now the speed is reached,
	for (;;)
	{
//		printf("Input = %d\r\n", upper_limit);
//		upper_limit = read_adc(CRANE_UPPER_LIMIT_ADC_CHAN);
		upper_limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN);


		if (upper_limit == 0)
		{
			printf("STOPPED while running up \r\n");

			vCraneStop();

		}
		vTaskDelay(50);
	}
}
//------------------------------------------------------------------------------------------------
void vCraneDnToLimitTask( void *pvParameters )
{
	uint16_t speed;
	uint32_t ii = 0;
	unsigned char lower_limit = 0;
	printf("Driving crane down to limit\r\n");
	TIM_Cmd( TIM3, ENABLE );
	crane_state = DRIVING_DN;

	//down counting increases speed
	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_ENABLE_PIN, 0 );
	GPIO_WriteBit( CRANE_CONTROL_PORT, CRANE_DIR_PIN, 0 );

	//Ramp Up Speed
	for (speed = 40000; speed > 11800; speed-=100)
	{
		//printf("Ramp up\r\n");
		vTaskDelay(1);
		TIM_SetAutoreload(TIM3, speed);
		//lower_limit = read_adc(CRANE_LOWER_LIMIT_ADC_CHAN);
		lower_limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);
		if (lower_limit == 0)
		{
			printf("STOPPED on Ramp up\r\n");
			vCraneStop();

			vTaskDelay(10);
		}
	}

	//Now the speed is reached,
	for (;;)
	{
		//printf("running down\r\n");
		//lower_limit = read_adc(CRANE_LOWER_LIMIT_ADC_CHAN);
		lower_limit = GPIO_ReadInputDataBit(CRANE_LIMIT_PORT, CRANE_LOWER_LIMIT_PIN);

		if (lower_limit == 0)
		{
			printf("STOPPED while running down\r\n");
			vCraneStop();
			/*xTaskCreate( vCraneStop,
					( signed portCHAR * ) "crane_stop",
					configMINIMAL_STACK_SIZE +500,
					NULL,
					tskIDLE_PRIORITY+1,
					NULL );*/
			vTaskDelay(100);
		}
		vTaskDelay(50);
	}
}
//----------------------------------------------------------------------------------------------------
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

void manual_crane_applet(int init){
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
		lcd_printf(7,8,4, "DOWN");
		lcd_printf(26,6,4, "STOP");
		lcd_printf(30, 13, 4, "Back");
		//adc_init();
		//adc_init();
		xTaskCreate( vCraneAppletDisplay,
				( signed portCHAR * ) "crane_display",
				configMINIMAL_STACK_SIZE +500,
				NULL,
				tskIDLE_PRIORITY,
				&xCraneAppletDisplayHandle );
	}

}



int manual_crane_key(int xx, int yy){

	uint16_t window = 0;
	static uint8_t w = 5,h = 5;
	static uint16_t last_window = 0;
	//printf("(%d, %d)\r\n", x, y);
	if (xx > UP_X1+1 && xx < UP_X2-1 && yy > UP_Y1+1 && yy < UP_Y2-1)
	{
		printf("Up button Pressed...\r\n");
		if (xCraneUpToLimitTaskHandle == NULL)
		{
		    printf("No previous up task, checking for down task... \r\n");
		    if (xCraneDnToLimitTaskHandle)
		      {
		        printf("down task found, deleting...\r\n");
		       vCraneStop();
		      }
		    printf("Creating Task to drive crane up to upper limit\r\n");
		    xTaskCreate( vCraneUpToLimitTask,
		        ( signed portCHAR * ) "crane_up",
		        configMINIMAL_STACK_SIZE +800,
		        NULL,
		        tskIDLE_PRIORITY+1,
		        &xCraneUpToLimitTaskHandle );
		    // lcd_fill(1, 71, 200, 50, Black);
		}

	}
	else if (xx > DN_X1+1 && xx < DN_X2-1 && yy > DN_Y1+1 && yy < DN_Y2-1)
	  {
	    printf("Dn button Pressed...\r\n");
	    if (xCraneDnToLimitTaskHandle == NULL)
	      {
	        printf("No previous down task, checking for up task...\r\n");
	        if (xCraneUpToLimitTaskHandle)
	          {
	            printf("up task found, deleting...\r\n");
	           vCraneStop();
	          }
	        printf("Creating Task to drive crane Down to upper limit\r\n");

	        xTaskCreate( vCraneDnToLimitTask,
	            ( signed portCHAR * ) "crane_down",
	            configMINIMAL_STACK_SIZE +800,
	            NULL,
	            tskIDLE_PRIORITY+1,
	            &xCraneDnToLimitTaskHandle );

	        //	lcd_fill(1, 126, 200, 50, Black);

	      }

	  }
	else if (xx > ST_X1+1 && xx < ST_X2-1 && yy > ST_Y1+1 && yy < ST_Y2-1)
	  {
	    vCraneStop();

	  }

	else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
	  {

	    if (xCraneAppletDisplayHandle != NULL)
	      {
	        vTaskDelete(xCraneAppletDisplayHandle);

	        xCraneAppletDisplayHandle = NULL;
	      }


	    if (crane_state != STOPPED)
	      vCraneStop();

	    vTaskDelay(101);
	    return 1;

	  }
 {
	vTaskDelay(10);
	return 0;
	//return xx > 200 && yy > 200;
	}
}
