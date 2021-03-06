/*

 /* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Library includes. */
#include "stm32f10x.h"
#include "misc.h"
#include "stm32f10x_it.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_i2c.h"
#include "semphr.h"

/*app includes. */
//#include "stm3210e_lcd.h"
#include "leds.h"
#include "touch.h"
#include "lcd.h"
#include "menu.h" 
#include "speaker.h"
#include "timer.h"
#include "ds1820.h"
#include "serial.h"
#include "crane.h"
#include "adc.h"
#include "hlt.h"
#include "mill.h"
#include "mash_pump.h"
#include "valves.h"
#include "diag_temps.h"
#include "hop_dropper.h"
#include "boil.h"
#include "stir.h"
#include "stirApplet.h"
#include "Flow1.h"
#include "diag_pwm.h"
#include "I2C-IO.h"
#include "chiller_pump.h"
#include "console.h"
#include "MenuExamples.h"

#include "brew.h"
#include "parameters.h"
#include "boil_valve.h"
#include "main.h"
#include "FreeRTOS/Source/portable/GCC/ARM_CM3/portmacro.h"

/*-----------------------------------------------------------*/

/* The period of the system clock in nano seconds.  This is used to calculate
 the jitter time in nano seconds. */
#define mainNS_PER_CLOCK ( ( unsigned portLONG ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )
unsigned long ulIdleCycleCount = 0UL;

/*-----------------------------------------------------------*/

/**
 * Configure the hardware for the App.
 */
static void prvSetupHardware(void);

void vTaskWakesUpEverySecond(void *pvParameters);

// Needed by file core_cm3.h
volatile int ITM_RxBuffer;

/*-----------------------------------------------------------*/

void vCheckTask(void *pvParameters);

xTaskHandle xLCDTaskHandle,
    xTouchTaskHandle,
    xBeepTaskHandle,
    xTimerSetupHandle,
    xDS1820TaskHandle,
    xCheckTaskHandle,
    xLitresToBoilHandle,
    xI2C_TestHandle,
    xI2C_SendHandle,
    xLitresToMashHandle,
    xHopsTaskHandle,
    xBrewTaskHandle,
    xBoilValveTaskHandle,
    xSerialHandlerTaskHandle,
		xSerialControlTaskHandle,
		xTaskWakesUpEverySecondHandle;

/*-----------------------------------------------------------*/

struct menu diag_menu[] =
{
	{"Applet",              NULL,                           example_applet,                NULL,                   example_applet_touch_handler},
	{"FlashLED",            NULL,                           NULL,                          item_2_callback,        NULL},
	{"DS1820Diag",          NULL,                           vDS1820DiagApplet,             NULL,                   DS1820DiagKey},
	{"Flow1",               NULL,                           vFlow1Applet,                  NULL,                   iFlow1Key},
	{"PWM Diag",            NULL,                           vDiagPWMApplet,                NULL,                   iDiagPWMKey},
	{"Back",                NULL,                           NULL,                          NULL,                   NULL},
	{NULL,                  NULL,                           NULL,                           NULL,                  NULL}
};

struct menu manual_menu[] =
{
	{"Mill",                NULL,                           vMillApplet,                    NULL,                   iMillKey},
	{"Crane",       	    NULL,			            	vCraneApplet, 	                NULL, 		        	iCraneKey},
	{"Stir",                NULL,                           vStirApplet,                    NULL,                   iStirKey},
	{"HopDropper",          NULL,                           vHopDropperApplet,              NULL,                   iHopDropperKey},
	{"HLT",                 NULL,                           vHLTApplet,                     NULL,                   HLTKey},
	{"Boil",                NULL,                           vBoilApplet,                    NULL,                   iBoilKey},
	{"Back",   	            NULL, 		             		NULL, 				            NULL, 	           		NULL},
	{NULL, 			        NULL, 			             	NULL, 			             	NULL, 		        	NULL}
};


struct menu main_menu[] =
{
	{"Manual Control",      manual_menu,    		        NULL, 	             			NULL, 		        	NULL},
	{"Pumps/Valves" ,       NULL,                   vValvesApplet,                           NULL,                   iValvesKey},
	{"Diagnostics",         diag_menu,                      NULL,                           NULL,                   NULL},
	{"Parameters",          NULL,                           vParametersApplet,              NULL,                   iParametersKey},
	{"BREW",                NULL,                           vBrewApplet,                    NULL,                   iBrewKey},
	{NULL,                  NULL, 		              		NULL,                           NULL, 			        NULL}
};

int main(void)
{
	prvSetupHardware(); // set up peripherals etc

	USARTInit(USART_PARAMS1 );

	xPrintQueue = xQueueCreate(15, sizeof(char *));
	if (xPrintQueue == NULL )
	{
		printf("Failed to make print queue\r\n\0");
		for (;;)
			;
	}

	vParametersInit();
	lcd_init();
	menu_set_root(main_menu);
	vLEDInit();
	vI2C_Init();
	vMillInit();
	vMashPumpInit();
	vValvesInit();
	vDiagTempsInit();
	vHopsInit();
	vBoilInit();
	vStirInit();
	vCraneInit();
	hlt_init();
	vFlow1Init();
	vDiagPWMInit();
	vChillerPumpInit();
	vBoilValveInit();
	lcd_background(Black);

	xTaskCreate( vConsolePrintTask, ( signed portCHAR * ) "PrintTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xPrintTaskHandle);

	xTaskCreate( vTouchTask, ( signed portCHAR * ) "touch    ", configMINIMAL_STACK_SIZE +400, NULL, tskIDLE_PRIORITY, &xTouchTaskHandle);

	xTaskCreate( vTaskDS1820Convert, ( signed portCHAR * ) "TempSensors", configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY, &xDS1820TaskHandle);

	xTaskCreate( vTaskLitresToBoil, ( signed portCHAR * ) "boil_litres", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xLitresToBoilHandle);

	xTaskCreate( vTaskLitresToMash, ( signed portCHAR * ) "mash_litres", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xLitresToMashHandle);

	xTaskCreate(vTaskHops, (signed portCHAR *) "hops    ", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1,
			&xHopsTaskHandle);

	xTaskCreate( vCheckTask, ( signed portCHAR * ) "check     ", configMINIMAL_STACK_SIZE +400, NULL, tskIDLE_PRIORITY, &xCheckTaskHandle);

	xTaskCreate( vTaskCrane, ( signed portCHAR * ) "Crane    ", configMINIMAL_STACK_SIZE + 200, NULL, tskIDLE_PRIORITY+1, &xCraneTaskHandle);

	xTaskCreate( vTaskBoilValve, ( signed portCHAR * ) "boilvalve", configMINIMAL_STACK_SIZE +200, NULL, tskIDLE_PRIORITY+1, &xBoilValveTaskHandle);

	xTaskCreate( vTaskHLTLevelChecker, ( signed portCHAR * ) "hlt_level", configMINIMAL_STACK_SIZE +100, NULL, tskIDLE_PRIORITY, & xTaskHLTLevelCheckerTaskHandle);

	xTaskCreate( vI2C_SendTask, ( signed portCHAR * ) "i2c_send", configMINIMAL_STACK_SIZE +500, NULL, tskIDLE_PRIORITY+2, & xI2C_SendHandle);

	xTaskCreate(vTaskWakesUpEverySecond, (signed portCHAR *) "1secTickTask", configMINIMAL_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + 2, &xTaskWakesUpEverySecondHandle);

	/* Start the scheduler. */
	vTaskStartScheduler();

	printf("FAIL\r\n\0");

	/* Will only get here if there was insufficient memory to create the idle
	 task. */
	return 0;
}

volatile portTickType ticks;

void vTaskWakesUpEverySecond(void *pvParameters)
{
	// Set up
	portTickType xLastWakeTime;
	const portTickType xPeriodInMilliSeconds=1000;

	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime=xTaskGetTickCount();

	for (;;)
	{
		vTaskDelayUntil(&xLastWakeTime, xPeriodInMilliSeconds / portTICK_RATE_MS);
		ticks++;
	}


	// Loop
}



/*-----------------------------------------------------------*/
// HARDWARE SETUP
/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
	/* Start with the clocks in their expected state. */
	RCC_DeInit();

	/* Enable HSE (high speed external clock). */
	RCC_HSEConfig(RCC_HSE_ON );

	/* Wait till HSE is ready. */
	while (RCC_GetFlagStatus(RCC_FLAG_HSERDY ) == RESET)
	{
	}

	/* 2 wait states required on the flash. */
	*((unsigned portLONG *) 0x40022000) = 0x02;

	/* HCLK = SYSCLK */
	RCC_HCLKConfig(RCC_SYSCLK_Div1 );

	/* PCLK2 = HCLK */
	RCC_PCLK2Config(RCC_HCLK_Div1 );

	/* PCLK1 = HCLK/2 */
	RCC_PCLK1Config(RCC_HCLK_Div2 );

	/* PLLCLK = 8MHz * 9 = 72 MHz. */
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9 );

	/* Enable PLL. */
	RCC_PLLCmd(ENABLE);

	/* Wait till PLL is ready. */
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY ) == RESET)
	{
	}

	/* Select PLL as system clock source. */
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK );

	/* Wait till PLL is used as system clock source. */
	while (RCC_GetSYSCLKSource() != 0x08)
	{
	}

	/* Enable GPIOA, GPIOB, GPIOC, GPIOD, GPIOE and AFIO clocks */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
	    RCC_APB2Periph_GPIOB |
	    RCC_APB2Periph_GPIOC |
	    RCC_APB2Periph_GPIOD |
	    RCC_APB2Periph_GPIOE |
	    RCC_APB2Periph_GPIOF |
	    RCC_APB2Periph_AFIO,
	    ENABLE);

	/* SPI2 Periph clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	/* Set the Vector Table base address at 0x08000000 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4 );

	/* Configure HCLK clock as SysTick clock source. */
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK );

}

/*-----------------------------------------------------------*/
// STACK OVERFLOW HOOK -  TURNS ON LED
/*-----------------------------------------------------------*/
void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed portCHAR *pcTaskName)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_WriteBit(GPIOC, GPIO_Pin_7, 1);

	for (;;)
		;
}

void vApplicationMallocFailedHook(void)
{
	vConsolePrint("MALLOC FAILED!");
	for (;;)
		;
}

/*-----------------------------------------------------------*/
void vApplicationIdleHook(void)
{
	ulIdleCycleCount++;
}

/* Keep the linker happy. */
void assert_failed(unsigned portCHAR* pcFile, unsigned portLONG ulLine)
{
	printf("FAILED %s %d\r\n\0", pcFile, ulLine);

	for (;;)
	{
	}
}

size_t uiGetHeapDiff()
{
	static size_t last;
	size_t retval = 0;
	size_t size = xPortGetFreeHeapSize();
	if (size - last != 0)
		retval = size - last;
	last = size;
	return retval;
}
void vCheckTask(void *pvParameters)
{
	char buf[14];
	char pcBrewElapsedTime[14], pcStepElapsedTime[14];
	char pcBrewElapsedHours[14], pcBrewElapsedMinutes[14], pcBrewElapsedSeconds[14], pcBrewStep[14];
	char pcBrewStepElapsedHours[14], pcBrewStepElapsedMinutes[14], pcBrewStepElapsedSeconds[14], pcMashTemp[14], pcHLTTemp[14];
	char pcChillerPumpState[14], pcBoilState[14], pcHeapRemaining[14], pcBrewState[14], pcBoilDuty[14], pcLitresDelivered[14];
	char pcNominalMashTemp[14], pcNominalMashOutTemp[14], pcNominalSpargeTemp[14];
	char pcBoilValveState[14], pcMashValveState[14], pcHltValveState[14], pcChillerValveState[14], pcGrainMillState[14], pcMashPumpState[14], pcInletValveState[14], pcCranePosition[14];

	int ii = 0;
	unsigned int touch, hops, ds1820, timer, litres, check, low_level = 64, print, serial, serialcontrol;
	size_t heapdiff, heapRemaining;
	unsigned int display_applet, stats_applet, res_applet, graph_applet, brew_task;
	static char cBuf[80];
	for (;;)
	{
		vConsolePrint("."); // added just to see if when the screen locks up on serial issue, does the processor lock too? Maybe we can recover if the screen is locked only. I.e. reinit the lcd or touch.
		touch = uxTaskGetStackHighWaterMark(xTouchTaskHandle);
		ds1820 = uxTaskGetStackHighWaterMark(xDS1820TaskHandle);
		timer = uxTaskGetStackHighWaterMark(xTimerSetupHandle);
		litres = uxTaskGetStackHighWaterMark(xLitresToBoilHandle);
		print = uxTaskGetStackHighWaterMark(xPrintTaskHandle);
		hops = uxTaskGetStackHighWaterMark(xHopsTaskHandle);
		check = uxTaskGetStackHighWaterMark(NULL );
		serial = uxTaskGetStackHighWaterMark(xSerialHandlerTaskHandle);
		serialcontrol = uxTaskGetStackHighWaterMark(xSerialControlTaskHandle);
		display_applet = uiGetBrewAppletDisplayHWM();
		res_applet = uiGetBrewResAppletHWM();
		stats_applet = uiGetBrewStatsAppletHWM();
		graph_applet = uiGetBrewGraphAppletHWM();
		brew_task = uiGetBrewTaskHWM();

		heapdiff = uiGetHeapDiff();
		if (heapdiff != 0)
		{
			sprintf(pcCranePosition, "Heap down by %02dBytes\r\n\0", (int) heapdiff);
			vConsolePrint(pcCranePosition);
			heapRemaining = xPortGetFreeHeapSize();
			sprintf(pcHeapRemaining, "Heap:%d\r\n\0", heapRemaining);
			vConsolePrint(pcHeapRemaining);
		}

		if (touch < low_level ||
		    timer < low_level ||
		    litres < low_level ||
		    print < low_level ||
		    hops < low_level ||
		    check < low_level ||
		    ((display_applet < low_level) && (display_applet != 0)) ||
		    ((res_applet < low_level) && (res_applet != 0)) ||
		    ((stats_applet < low_level) && (stats_applet != 0)) ||
		    ((graph_applet < low_level) && (graph_applet != 0)) ||
		    ((brew_task < low_level) && (brew_task != 0)))

		{
			vConsolePrint("=============================\r\n\0");
			sprintf(cBuf, "check task: idle ticks = %d\r\n\0", ulIdleCycleCount);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "touchwm = %d\r\n\0", touch);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "DS1820wm = %d\r\n\0", ds1820);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "TimerSetupwm = %d\r\n\0", timer);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "litreswm = %d\r\n\0", litres);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "hopswm = %d\r\n\0", hops);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "check = %d\r\n\0", check);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "serial = %d\r\n\0", serial);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "serialcontrol = %d\r\n\0", serialcontrol);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "brewtask = %d\r\n\0", brew_task);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "stats = %d\r\n\0", stats_applet);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "res = %d\r\n\0", res_applet);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "graph = %d\r\n\0", graph_applet);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "brew_display = %d\r\n\0", display_applet);
			vConsolePrint(cBuf);
			vTaskDelay(25);
			sprintf(cBuf, "print = %d\r\n\0", print);
			vConsolePrint(cBuf);
			vConsolePrint("=============================\r\n\0");
			//xTaskResumeAll();
			vTaskDelay(800);

		}
		vTaskDelay(800);
		taskYIELD();
	}
}

