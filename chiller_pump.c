/*
 * chiller_pump.c
 *
 *  Created on: Aug 5, 2013
 *      Author: brad
 */

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "I2C-IO.h"
#include "chiller_pump.h"
#include "console.h"
#include "main.h"


ChillerPumpState_t ChillerPumpState = CHILLER_PUMP_STOPPED;

ChillerPumpState_t GetChillerPumpState(){
  return ChillerPumpState;
}

void vChillerPumpInit(void){

  GPIO_InitTypeDef GPIO_InitStructure;
   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
   GPIO_InitStructure.GPIO_Pin =  CHILLER_PUMP_PIN;
   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// Output - Push Pull
   GPIO_Init( CHILLER_PUMP_PORT, &GPIO_InitStructure );
   GPIO_ResetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN); //pull low

}

void vChillerPump(ChillerPumpCommand command )
{
  if (command == START_CHILLER_PUMP && ChillerPumpState != CHILLER_PUMP_PUMPING)
    {
	  vConsolePrint("Starting Chiller Pump\r\n\0");
      GPIO_SetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN);
      ChillerPumpState = CHILLER_PUMP_PUMPING;
    }
  else if (command == STOP_CHILLER_PUMP && ChillerPumpState != CHILLER_PUMP_STOPPED)
    {
	  vConsolePrint("Stopping Chiller Pump\r\n\0");
      GPIO_ResetBits(CHILLER_PUMP_PORT, CHILLER_PUMP_PIN); //pull low
      ChillerPumpState = CHILLER_PUMP_STOPPED;
    }

}

void vToggleChillerPump()
{

	if(ChillerPumpState != CHILLER_PUMP_PUMPING)
	{
		//printf("MPStart\r\n");
		vChillerPump(START_CHILLER_PUMP);
	}
	else
//		printf("MPStop\r\n");
		vChillerPump(STOP_CHILLER_PUMP);
}





