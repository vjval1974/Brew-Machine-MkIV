/*
 * io_util.c
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "io_util.h"


// Helper Function to debounce the input from the dry contacts on the relays
uint8_t debounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	uint8_t read1, read2;
	read1 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
	vTaskDelay(5);
	read2 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
	if (read1 == read2)
		return read1;
	else
		return 0;
}

uint8_t uDebounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, PinState uRequiredState)
{
	uint8_t uPinState1, uPinState2;
	uPinState1 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
	vTaskDelay(5);
	uPinState2 = GPIO_ReadInputDataBit(GPIOx, GPIO_Pin);
	// if both values are the same, we are happy.
	if ((uPinState1 == uPinState2) && (uPinState2 == uRequiredState))
	{
		return pdTRUE;
	}

	return pdFALSE;
}
