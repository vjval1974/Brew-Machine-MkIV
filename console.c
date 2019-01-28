//-------------------------------------------------------------------------
// Author: Brad Goold
// Date: 11/11/2013
// Email Address: W0085400@umail.usq.edu.au
//
// Purpose:
// Pre:
// Post:
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Included Libraries
//-------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"
#include "console.h"
#include "main.h"
#include "serial.h"

xQueueHandle xPrintQueue;
xTaskHandle xPrintTaskHandle;


void vConsolePrintTask(void * pvParameters)
{
  char * pcMessageToPrint;
  char str[100];
  static char * pcLastMessage;
  for (;;)
    {
      xQueueReceive(xPrintQueue, &pcMessageToPrint, portMAX_DELAY);
     strcpy(str, pcMessageToPrint);
     vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
      //sprintf(cBuffer,"%s", &pcMessageToPrint);

      //if (pcMessageToPrint != pcLastMessage)
        {
          portENTER_CRITICAL();
          if (strlen(str) > 0)
        	 comm_puts(str);

//          fflush(stdout);
          portEXIT_CRITICAL();
      //    pcLastMessage = pcMessageToPrint;
          vTaskDelay(20); //wait for the usart to print before filling it's buffer.
          //maybe can do something with the interrupt flags here?
        }
    }


}

const char * pcX = "ConsolePrint failed\r\n\0";
void vConsolePrint(const char * format)
{
	vTaskPrioritySet(xPrintTaskHandle, tskIDLE_PRIORITY+4);
   if ( xQueueSendToBack(xPrintQueue, &format, 30) != pdPASS)
     xQueueSendToBack(xPrintQueue, &pcX, 30);

}
