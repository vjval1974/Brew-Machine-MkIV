/*
 * console.h
 *
 *  Created on: 11/11/2013
 *      Author: brad
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

void vConsolePrintTask( void * pvParameters);
<<<<<<< HEAD
void vConsolePrint(const char * format, ...);
=======
void vConsolePrint(const char * format);
>>>>>>> PCF8574_bug
extern xQueueHandle xPrintQueue;

#endif /* CONSOLE_H_ */
