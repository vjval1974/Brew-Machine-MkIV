/*
 * io_util.h
 *
 *  Created on: 01/12/2013
 *      Author: brad
 */

#ifndef IO_UTIL_H_
#define IO_UTIL_H_


uint8_t debounce(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

void vRunTimeTimerSetup(void);

//Commands
#define OPEN 0
#define CLOSE 1
#define STOP -1
#define TOGGLE 255

#endif /* IO_UTIL_H_ */
