/*

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Library includes. */
#include "stm32f10x.h"
#include "misc.h"
#include "stm32f10x_it.h"
#include "stm32f10x_rcc.h"

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
#include "hlt_pump.h"
#include "mash_pump.h"
#include "valves.h"
#include "diag_temps.h"
#include "hop_dropper.h"
#include "boil.h"
#include "stir.h"
#include "Flow1.h"

/*-----------------------------------------------------------*/

/* The period of the system clock in nano seconds.  This is used to calculate
the jitter time in nano seconds. */
#define mainNS_PER_CLOCK ( ( unsigned portLONG ) ( ( 1.0 / ( double ) configCPU_CLOCK_HZ ) * 1000000000.0 ) )
unsigned long ulIdleCycleCount = 0UL;

/*-----------------------------------------------------------*/

/**
 * Configure the hardware for the App.
 */
static void prvSetupHardware( void );

/*-----------------------------------------------------------*/

xTaskHandle xLCDTaskHandle, 
    xTouchTaskHandle, 

    xBeepTaskHandle, 
    xTimerSetupHandle,
    xDS1820TaskHandle,
    xCheckTaskHandle,
    xLitresDeliveredHandle ;



// Needed by file core_cm3.h
volatile int ITM_RxBuffer;

/*-----------------------------------------------------------*/

void item_1_callback(unsigned char button_down)
{
	if (button_down)
		vLEDToggle(D4_PORT, D4_PIN);
}

void item_2_callback(unsigned char button_down)
{
  printf("Button %d\r\n", button_down);
  printf("HLT Display Water Mark from main = %u\r\n", uxTaskGetStackHighWaterMark(xHLTAppletDisplayHandle));
  vLEDSet(D4_PORT, D4_PIN, button_down);
}

void example_applet(int init)
{
	if (init)
	{
		lcd_printf(20, 1, 20, "Example Applet");
		lcd_printf(34, 13, 4, "Back");

	}
}

int example_applet_touch_handler(int xx, int yy)
{
	lcd_fill(0, 0, 150, 40, 0xFF);
	lcd_printf(1,1, 20, "Touch %d,%d", xx, yy);

	// return non-zero to exit applet
	return xx > 200 && yy > 200;
}

void vCheckTask(void *pvParameters)
{

  unsigned int touch, adc, ds1820, timer, litres, check, low_level = 100, heap;
  for (;;){

      touch = uxTaskGetStackHighWaterMark(xTouchTaskHandle);

      ds1820 =  uxTaskGetStackHighWaterMark(xDS1820TaskHandle);
      timer = uxTaskGetStackHighWaterMark(xTimerSetupHandle);
      litres = uxTaskGetStackHighWaterMark(xLitresDeliveredHandle);
      check = uxTaskGetStackHighWaterMark(NULL);
      heap = xPortGetFreeHeapSize();

      if (touch < low_level ||
          adc < low_level ||
          ds1820 < low_level ||
          timer < low_level)
        {
          //vTaskSuspendAll();
          printf("check task: idle ticks = %lu\r\n", ulIdleCycleCount);
          printf("touchwm = %d\r\n", touch);
          printf("DS1820wm = %d\r\n", ds1820);
          printf("TimerSetupwm = %d\r\n", timer);
          printf("litreswm = %d\r\n", litres);
          printf("check = %d\r\n", check);
          printf("Free Heap Size = %d\r\n", heap);
          //xTaskResumeAll();
          vTaskDelay(100);

        }

      vTaskDelay(1000);
      taskYIELD();
  }



}

struct menu diag_menu[] =
    {
        {"Applet",              NULL,                           example_applet,                 NULL,                        example_applet_touch_handler},
        {"FlashLED",            NULL,                           NULL,                           item_2_callback,        NULL},
        {"DS1820Diag",           NULL,                           vDS1820DiagApplet,              NULL,                  DS1820DiagKey},
        {"Flow1",                NULL,                           vFlow1Applet,                  NULL,                   iFlow1Key},
        {"Back",                 NULL,                           NULL,                           NULL,                   NULL},
        {NULL,                  NULL,                           NULL,                           NULL,                   NULL}
    };


struct menu manual_menu[] =
    {
        {"Crane",       	NULL,				vCraneApplet, 	        NULL, 			iCraneKey},
        {"Valves",              NULL,                           vValvesApplet,                  NULL,                   iValvesKey},
        {"HopDropper",          NULL,                           vHopDropperApplet,              NULL,                   iHopDropperKey},
        {"HLT",                 NULL,                           vHLTApplet,                     vHLTAppletCallback,     HLTKey},
        {"HLT Pump",            NULL,                           vHLTPumpApplet,                 NULL,                   iHLTPumpKey},
        {"Mash Pump",           NULL,                           vMashPumpApplet,                NULL,                   iMashPumpKey},
        {"Mill",                NULL,                           vMillApplet,                    NULL,                   iMillKey},
        {"Back",   	        NULL, 				NULL, 				NULL, 			NULL},
        {NULL, 			NULL, 				NULL, 				NULL, 			NULL}
    };

struct menu main_menu[] =
    {
        {"Manual Control",      manual_menu,    		NULL, 				NULL, 			NULL},
        {"Boil",                NULL,                           vBoilApplet,                    NULL,                   iBoilKey},
        {"Stir",                NULL,                           vStirApplet,                    NULL,                   iStirKey},
        {"Diagnostics",         diag_menu,                      NULL,                           NULL,                   NULL},
        {NULL,                  NULL, 				NULL,                           NULL, 			NULL}
    };

/**
 * Main.
 */ 
int main( void )
{
    prvSetupHardware();// set up peripherals etc 
    USARTInit(USART_PARAMS1);



    lcd_init();          
    vLEDInit();


    vMillInit();
    vHLTPumpInit();
    vMashPumpInit();
    vValvesInit();
    vDiagTempsInit();
    vHopsInit();
    vBoilInit();
    vStirInit();
    vCraneInit();
    vStirInit();
    hlt_init();
    vFlow1Init();


    menu_set_root(main_menu);

    xTaskCreate( vTouchTask, 
                 ( signed portCHAR * ) "touch", 
                 configMINIMAL_STACK_SIZE +400,
                 NULL, 
                 tskIDLE_PRIORITY+1,
                 &xTouchTaskHandle );

    // Create you application tasks if needed here
    xTaskCreate( vTaskDS1820Convert,
        ( signed portCHAR * ) "DS1820",
        configMINIMAL_STACK_SIZE ,
        NULL,
        tskIDLE_PRIORITY,
        &xDS1820TaskHandle );

    xTaskCreate( vTaskLitresDelivered,
        ( signed portCHAR * ) "hlt_litres",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY ,
        &xLitresDeliveredHandle );


    xTaskCreate( vCheckTask,
        ( signed portCHAR * ) "check",
        configMINIMAL_STACK_SIZE +200,
        NULL,
        tskIDLE_PRIORITY,
        &xCheckTaskHandle );

    /* Start the scheduler. */
    vTaskStartScheduler();
    
    printf("FAIL\r\n");

    /* Will only get here if there was insufficient memory to create the idle
       task. */
    return 0;
}


/*-----------------------------------------------------------*/
// HARDWARE SETUP
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    /* Start with the clocks in their expected state. */
    RCC_DeInit();
    
    /* Enable HSE (high speed external clock). */
    RCC_HSEConfig( RCC_HSE_ON );
    
    /* Wait till HSE is ready. */
    while( RCC_GetFlagStatus( RCC_FLAG_HSERDY ) == RESET )
    {
    }
    
    /* 2 wait states required on the flash. */
    *( ( unsigned portLONG * ) 0x40022000 ) = 0x02;

	/* HCLK = SYSCLK */
	RCC_HCLKConfig( RCC_SYSCLK_Div1 );

	/* PCLK2 = HCLK */
	RCC_PCLK2Config( RCC_HCLK_Div1 );

	/* PCLK1 = HCLK/2 */
	RCC_PCLK1Config( RCC_HCLK_Div2 );

	/* PLLCLK = 8MHz * 9 = 72 MHz. */
	RCC_PLLConfig( RCC_PLLSource_HSE_Div1, RCC_PLLMul_9 );

	/* Enable PLL. */
	RCC_PLLCmd( ENABLE );

	/* Wait till PLL is ready. */
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
	{
	}

	/* Select PLL as system clock source. */
	RCC_SYSCLKConfig( RCC_SYSCLKSource_PLLCLK );

	/* Wait till PLL is used as system clock source. */
	while( RCC_GetSYSCLKSource() != 0x08 )
	{
	}

	/* Enable GPIOA, GPIOB, GPIOC, GPIOD, GPIOE and AFIO clocks */
	RCC_APB2PeriphClockCmd(	RCC_APB2Periph_GPIOA | 
                                RCC_APB2Periph_GPIOB |
                                RCC_APB2Periph_GPIOC | 
                                RCC_APB2Periph_GPIOD | 
                                RCC_APB2Periph_GPIOE | 
                                RCC_APB2Periph_GPIOF | 
                                RCC_APB2Periph_AFIO, 
                                ENABLE );
        
	/* SPI2 Periph clock enable */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_SPI2, ENABLE );
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );
        RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM4, ENABLE );

	/* Set the Vector Table base address at 0x08000000 */
	NVIC_SetVectorTable( NVIC_VectTab_FLASH, 0x0 );
        
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );

	/* Configure HCLK clock as SysTick clock source. */
	SysTick_CLKSourceConfig( SysTick_CLKSource_HCLK );
                
}

/*-----------------------------------------------------------*/
// STACK OVERFLOW HOOK -  TURNS ON LED
/*-----------------------------------------------------------*/
void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName )
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure );
    GPIO_WriteBit( GPIOC, GPIO_Pin_7, 1 );
    
    for( ;; );
}

/*-----------------------------------------------------------*/
void vApplicationIdleHook(void){
   ulIdleCycleCount++;
}



/* Keep the linker happy. */
void assert_failed( unsigned portCHAR* pcFile, unsigned portLONG ulLine )
{
    printf("FAILED %s %d\r\n", pcFile, ulLine);

	for( ;; )
	{
	}
}

