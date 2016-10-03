/*
 * console.h
 *
 *  Created on: 11/11/2013
 *      Author: brad
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_
#include "task.h"
void vConsolePrintTask( void * pvParameters);
void vConsolePrint(const char * format);
extern xQueueHandle xPrintQueue;
extern xTaskHandle xPrintTaskHandle;

#endif /* CONSOLE_H_ */
