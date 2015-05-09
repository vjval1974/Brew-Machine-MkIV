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

xQueueHandle xPrintQueue;


void vConsolePrintTask(void * pvParameters)
{
  char * pcMessageToPrint;
  static char * pcLastMessage;
  for (;;)
    {
      xQueueReceive(xPrintQueue, &pcMessageToPrint, portMAX_DELAY);

      //sprintf(cBuffer,"%s", &pcMessageToPrint);

      //if (pcMessageToPrint != pcLastMessage)
        {
          portENTER_CRITICAL();
          printf(pcMessageToPrint);
          fflush(stdout);
          portEXIT_CRITICAL();
      //    pcLastMessage = pcMessageToPrint;
          vTaskDelay(100); //wait for the usart to print before filling it's buffer.
          //maybe can do something with the interrupt flags here?
        }
    }


}

static char pcStringBuffer[100];
const char * pcX = "ConsolePrint failed\r\n";
void vConsolePrint(const char * format, ...)
{

   if ( xQueueSendToBack(xPrintQueue, &format, 10) != pdPASS)
     xQueueSendToBack(xPrintQueue, &pcX, 10);
}

//was sending &buffer over to printf.. changed for static char array
