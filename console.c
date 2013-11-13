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
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "queue.h"

xQueueHandle xPrintQueue;


void vConsolePrintTask(void * pvParameters)
{
 char * pcMessageToPrint;
 for (;;)
    {
      xQueueReceive(xPrintQueue, &pcMessageToPrint, portMAX_DELAY);
      //sprintf(cBuffer,"%s", &pcMessageToPrint);
      printf(pcMessageToPrint);

    }


}

void vConsolePrint(const char * cX)
{
    xQueueSendToBack(xPrintQueue, &cX, 0);
}
