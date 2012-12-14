/*
 * stir.c
 *
 *  Created on: Dec 15, 2012
 *      Author: brad
 */
//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "stir.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"


#define CW 1
#define CCW 0
// was going to pass in a struct Step *step on the queue..
// when the queue receives the pointer, we can then maybe get step[0..n]
// if step[n].ucDirection = 0, and step[n].uSpeed = 0 and step[n].uTime = 0, then
// all steps are over and we maybe can send a message back saying we are finished processing
// that sequence
xQueueHandle xStirQueueHandle;

struct Step
{
    unsigned char ucDirection;
    uint16_t uSpeed; //in percent
    uint16_t uTime; //in secs
};


struct Step MashStirSteps[] =
    {
        {CW,        80,    5},
        {CW,        80,    5},
        {CW,        80,    5},
        {0, 0, 0}
    };



void vStirInit(void)
{
  unsigned long ulFrequency;
  GPIO_InitTypeDef GPIO_InitStructure;


  //CONTROL PINS
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_InitStructure.GPIO_Pin =  STIR_STEP_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //Alt Function
  GPIO_Init( STIR_DRIVE_PORT, &GPIO_InitStructure );
  GPIO_SetBits(STIR_DRIVE_PORT, STIR_STEP_PIN);


  GPIO_InitStructure.GPIO_Pin =  STIR_ENABLE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //Alt Function
  GPIO_Init( STIR_CONTROL_PORT, &GPIO_InitStructure );
  GPIO_SetBits(STIR_CONTROL_PORT, STIR_ENABLE_PIN);

  GPIO_InitStructure.GPIO_Pin =  STIR_DIR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //Alt Function
  GPIO_Init( STIR_CONTROL_PORT, &GPIO_InitStructure );
  GPIO_SetBits(STIR_CONTROL_PORT, STIR_DIR_PIN);

  xStirQueueHandle = xQueueCreate(5, sizeof(struct Step *));

}

void vTaskStir(void * pvParameters)
{
struct Step * pstStep;
  for (;;)
    {
      if (xQueueReceive(xStirQueueHandle, pstStep, 10))
        {
          //something in queue
        }
      else
        {
          //queue empty
        }


    }
}
