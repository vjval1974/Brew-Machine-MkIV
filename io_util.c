/*
 * io_util.c
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"


// Helper Function to debounce the input from the dry contacts on the relays
uint8_t debounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  uint8_t read1, read2;
  read1 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
  vTaskDelay(5);
  read2 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
  if (read1 == read2)
    return read1;
  else return 0;

}

uint16_t uRunTimeCounter;

void vRunTimeTimerSetup(void)
{
  unsigned long ulFrequency;
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;


  /* Enable timer clocks */
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM5, ENABLE );

  /* Initialise timer */
  TIM_DeInit( TIM5 );
  TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

  // SET UP TIMER 5
  TIM_TimeBaseStructure.TIM_Period = 1000;
  TIM_TimeBaseStructure.TIM_Prescaler = 7200; //clock prescaler
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit( TIM5, &TIM_TimeBaseStructure );
  TIM_ARRPreloadConfig( TIM5, ENABLE );

  TIM_SetAutoreload(TIM5, 100000);
  //
  //GPIO_PinRemapConfig( GPIO_Remap_TIM4, ENABLE );

  //TIM_SetCompare1(TIM5, 0);
  TIM_Cmd( TIM5, ENABLE);


}





/*
struct OpenClose
{
  xQueueHandle xTaskQueue;
  int iStateVariable;
  GPIO_TypeDef * xOpenedPort;
  GPIO_TypeDef * xClosedPort;
  uint16_t uOpenedPin;
  uint16_t uClosedPin;
  void (*func)(int iCommand);
};

struct OpenClose CraneStruct = {
    xCraneQueue, iCraneState, CRANE_LIMIT_PORT, CRANE_LIMIT_PORT, CRANE_UPPER_LIMIT_PIN, CRANE_LOWER_LIMIT_PIN, vCraneFunc
};

*/
