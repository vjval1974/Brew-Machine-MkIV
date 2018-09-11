/*
 * io_util.h
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */

#ifndef IO_UTIL_H_
#define IO_UTIL_H_

typedef enum
{
	PIN_STATE_LOW = 0,
	PIN_STATE_HIGH = 1
} PinState;

uint8_t debounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
uint8_t uDebounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, PinState uRequiredState);


#endif /* IO_UTIL_H_ */
