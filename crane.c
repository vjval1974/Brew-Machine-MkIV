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
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------


void vCraneInit(void){ 
    
    unsigned long ulFrequency;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
/* Enable timer clocks */
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );
    
/* Initialise Ports, pins and timer */
    TIM_DeInit( TIM3 );
    TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );
    
    GPIO_InitStructure.GPIO_Pin =  CRANE_STEP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;// Alt Function - Push Pull
    GPIO_Init( CRANE_PORT, &GPIO_InitStructure );
    GPIO_PinRemapConfig( GPIO_FullRemap_TIM3, ENABLE );// Map TIM3_CH3
                                                       // to Step Pin    
/*
    GPIO_InitStructure.GPIO_Pin =  CRANE_ENABLE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init( CRANE_PORT, &GPIO_InitStructure );
    GPIO_ResetBits(CRANE_PORT, CRANE_ENABLE_PIN);

    GPIO_InitStructure.GPIO_Pin =  CRANE_DIR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init( CRANE_PORT, &GPIO_InitStructure );
    GPIO_ResetBits(CRANE_PORT, CRANE_DIR_PIN);
*/
    
    // SET UP TIMER 3
    
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
    
}

void vCraneUp(){
    uint16_t speed; 
    uint32_t ii = 0;
    TIM_Cmd( TIM3, ENABLE );
    //down counting increases speed
    for (speed = 40000; speed > 11800; speed-=100)
        {
            vTaskDelay(1);
            TIM_SetAutoreload(TIM3, speed);
        }
    //input logic here. 
    

    //then call vCraneStop();
}

void vCraneStop(){
    
    uint16_t currentSpeed = TIM3->ARR, setSpeed;
    //up counting decreases speed... then stop when steps get too large
    for (setSpeed = currentSpeed; setSpeed < 40000; setSpeed+=100)
    {
        vTaskDelay(1);
        TIM_SetAutoreload(TIM3, setSpeed);
    }
    TIM_Cmd( TIM3, DISABLE );
}

void vCraneTask( void *pvParameters )
{
	printf("Crane Start\r\n");

		unsigned char valid = 0;
		short adc_value = 0;
		adc_init();
	for( ;; )
	{
		adc_value = read_adc(10);
		lcd_fill(0, 20, 150, 40, 0xFF);
		lcd_printf(3,3,20, "ADC = %d", adc_value);
		vTaskDelay( 200 );
	}
}

xTaskHandle xCraneTaskHandle;

void manual_crane_applet(int init){
	if (init)
	{
		lcd_DrawRect(1, 10, 50, 70, Red);
		lcd_DrawRect(51, 101, 151, 201, Green);


		lcd_printf(1,1,20,  "MANUAL CRANE1");
//		lcd_printf(20,2,20, "Green = UP");
//		lcd_printf(20,3,20, "Red = Stop", Green, Black);
		lcd_printf(34, 13, 4, "Back");

		xTaskCreate( vCraneTask,
		                 ( signed portCHAR * ) "crane",
		                 configMINIMAL_STACK_SIZE +1000,
		                 NULL,
		                 tskIDLE_PRIORITY+2,
		                 &xCraneTaskHandle );
	}

  //  lcd_draw_buttons();
}



int manual_crane_key(int x, int y){

    uint16_t window = 0;
    static uint16_t last_window = 0;

    /*
    if (touchIsInWindow(x,y, 0,0, 150,50) == pdTRUE)
        window = 0;
    
    else  if (touchIsInWindow(x,y, 0,50, 150,100) == pdTRUE)
        window = 1;
    
    
    else  if (touchIsInWindow(x,y, 0,100, 150,150) == pdTRUE)
        window = 2;

    
    else  if (touchIsInWindow(x,y, 0,150, 150,200) == pdTRUE)
        window = 3;
    
    else  if (touchIsInWindow(x,y, 0,200, 150,250) == pdTRUE)
        window = 4;
    
    
    else  if (touchIsInWindow(x,y, 0,250, 150,300) == pdTRUE)
        window = 5;
    
    else  if (touchIsInWindow(x,y, 160, 0, 230,100) == pdTRUE)
        window = 6;

    else  if (touchIsInWindow(x,y, 160, 100, 230,200) == pdTRUE)
        window = 7;

    else  if (touchIsInWindow(x,y, 160, 200, 230,300) == pdTRUE)
        window = 8;
    else window = 255;
*/
    
    //Back Button
    if (window == 6)
    {
        vCraneStop();
      
    }
  
    else if (window == 7){
        vCraneUp();
  
    }
  
    else if (window == 8){
        vCraneStop();

    }
    if (x > 200 && y > 200)
    	vTaskDelete(xCraneTaskHandle);
	return x > 200 && y > 200;

}
