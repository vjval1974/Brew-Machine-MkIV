/*
 * boil.c
 *
 *  Created on: Dec 14, 2012
 *      Author: brad
 */

#include <stdint.h>
#include <stdio.h>
#include "boil.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#define BOIL_PORT GPIOB
#define BOIL_PIN GPIO_Pin_8

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
  // Initial duty cycle equals 25 which gives pulse of 5us
  TIM_OCInitStruct.TIM_Pulse = 5000; // will be 5us for the moment.
  TIM_OC3Init( TIM4, &TIM_OCInitStruct ); //PB8
  //TIM_OC3Init( TIM4, &TIM_OCInitStruct );
  TIM_SetAutoreload(TIM4,10000);
  TIM_Cmd( TIM4, ENABLE );

}
