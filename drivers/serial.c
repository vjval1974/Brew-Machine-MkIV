/* Scheduler includes. */
#include "FreeRTOS.h"

/* Library includes. */
#include "stm32f10x.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "serial.h"
#include "buffer.h"
#include "semphr.h"
#include "console.h"
#include "task.h"
#include "menu.h"
#include "brew.h"
#define BUFFERED
/*-----------------------------------------------------------*/
#ifdef BUFFERED
//initialise buffers
volatile FIFO_TypeDef U1Rx, U1Tx;
#endif

xSemaphoreHandle xSerialHandlerSemaphore;

static uint32_t g_usart;
// static USART_TypeDef* g_usart; CAN USE THIS IF TESTED... CLEARS COMPILER WARNINGS...

int comm_test(void)
{
        return ( USART_GetFlagStatus(g_usart, USART_FLAG_RXNE) == RESET ) ? 0 : 1;
}

uint8_t comm_get(void)
{

#ifdef BUFFERED
        uint8_t ch;
        //check if buffer is empty
        while (BufferIsEmpty(U1Rx) ==SUCCESS);
        BufferGet(&U1Rx, &ch);
        return ch;
#else
         while ( USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
                return (uint8_t)USART_ReceiveData(USART1);
#endif
                /*
        while(USART_GetFlagStatus(g_usart, USART_FLAG_RXNE) == RESET) { ; }
        return (char)USART_ReceiveData(g_usart);*/
}




void comm_put(char d)
{

#ifdef BUFFERED
        //put char to the buffer
        BufferPut(&U1Tx, d);
        //enable Transmit Data Register empty interrupt
        USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
#else
          USART_SendData(USART1, (uint8_t) d);
          //Loop until the end of transmission
          while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
          {
          }
#endif
/*
        while(USART_GetFlagStatus(g_usart, USART_FLAG_TXE) == RESET) { ; }
        USART_SendData(g_usart, (uint16_t)d);*/
}

void comm_puts(const char* s)
{
        char c;
        while ( ( c = *s++) != '\0' ) {
                comm_put(c);
        }
}

void USARTInit(uint16_t tx_pin, uint16_t rx_pin, uint32_t usart)
{

#ifdef BUFFERED
  //initialise buffers
  BufferInit(&U1Rx);
  BufferInit(&U1Tx);
#endif

  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  USART_ClockInitTypeDef  USART_ClockInitStructure;
  g_usart = usart; // keep track of which USART we are using

  //enable bus clocks
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
  if (usart == USART1)
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE );
  else
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE);

  //Configure USART1 Tx (PA.09) as alternate function push-pull
  GPIO_InitStructure.GPIO_Pin = tx_pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  //Configure USART1 Rx (PA.10) as input floating
  GPIO_InitStructure.GPIO_Pin = rx_pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* USART1 and USART2 configuration ------------------------------------------------------*/
  /* USART and USART2 configured as follow:
         - BaudRate = 115200 baud
         - Word Length = 8 Bits
         - One Stop Bit
         - No parity
         - Hardware flow control disabled (RTS and CTS signals)
         - Receive and transmit enabled
   */
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl =USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

  /* Configure USART1 */
  USART_Init(g_usart, &USART_InitStructure);

  /* Enable the USART1 */
  USART_Cmd(g_usart, ENABLE);


#ifdef BUFFERED
        //configure NVIC
        NVIC_InitTypeDef NVIC_InitStructure;
        //select NVIC channel to configure
        NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
        //set priority to lowest
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;
        //set sub-priority to lowest
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;
        //enable IRQ channel
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        //update NVIC registers
        NVIC_Init(&NVIC_InitStructure);
        //disable Transmit Data Register empty interrupt
        USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
        //enable Receive Data register not empty interrupt
        USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

       vSemaphoreCreateBinary(xSerialHandlerSemaphore);
        #endif

       // comm_puts("TEST\0\r\n");


}


void USART1_IRQHandler(void)
{
  portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

  // Code from Original ISR
       uint8_t ch;
       //if Receive interrupt
       if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
         {
           ch=(uint8_t)USART_ReceiveData(USART1);
           //put char to the buffer
           BufferPut(&U1Rx, ch);
           xSemaphoreGiveFromISR( xSerialHandlerSemaphore, & xHigherPriorityTaskWoken);
         }
       if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET)
         {
           if (BufferGet(&U1Tx, &ch) == SUCCESS)//if buffer read
             {
               USART_SendData(USART1, ch);
             }
           else//if buffer empty
             {
               //disable Transmit Data Register empty interrupt
               USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
             }
         }



  portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}


#define BUFFER_SIZE 32

xQueueHandle xCommandQueue;

void vSerialHandlerTask ( void * pvParameters)
{
  char buf[BUFFER_SIZE];
  char line[BUFFER_SIZE];
  char index = 0;
  char c;

  // Take the semaphore before entering infinite loop to make sure it's empty.
  xSemaphoreTake(xSerialHandlerSemaphore, 0);

  // initialise buffer
  for (index = 0; index < BUFFER_SIZE; index++){
      buf[index] = 0;
      line[index] = 0;
  }
  index = 0;
  xCommandQueue = xQueueCreate(32, BUFFER_SIZE);

  for (;;)
    {
      xSemaphoreTake(xSerialHandlerSemaphore, portMAX_DELAY);

      //get char
      c = 0;
      c = comm_get();

      if (c != 0)
        {
          //printf("%c", c);

          //save in buffer and increment buffer index
          buf[index++] = c;
          portENTER_CRITICAL();
          //if newline or full, print it and copy the buffer to 'line' string
          if (c == '\r' || c == '\n' || index >= BUFFER_SIZE){
              buf[index] = '\0';
              strcpy(line, buf);
              printf("%s\r\n", buf);
              fflush(stdout);
              for (index = 0; index < BUFFER_SIZE; index++)
                buf[index] = 0;
              index = 0;
              printf("Contents of 'line':%s\r\n", line);
              xQueueSend(xCommandQueue, line, portMAX_DELAY);
          }
          portEXIT_CRITICAL();
        }
      else
        {
          vConsolePrint("Failed READING\r\n");
        }

    }

}

static int result = 0xFE;
char buf[BUFFER_SIZE];
char * input;
char cmp[BUFFER_SIZE];

void vSerialControlCentreTask( void * pvParameters){
int ii = 0;

static char brewStarted = FALSE;

  for(;;)
    {
      xQueueReceive(xCommandQueue, &buf, portMAX_DELAY);
      portENTER_CRITICAL();
      char * input = (char *)pvPortMalloc(strlen(buf)+1);
      strcpy (input, buf);
      result = (strcmp(input, "command\r\0"));
      vConsolePrint("got something\r\n");
      if (result == 0)
        {
          printf("command received\r\n");
          fflush(stdout);
        }
      if(strcmp(input, "sb\r\0") == 0)
        {
          printf("Command to start brew!\r\n");
          if (!brewStarted){
          menu_command(4);
          menu_command(-1);
          printf("Brew Applet entered\r\n");
          vBrewRemoteStart();
          brewStarted = TRUE;
          }
          else {
              printf("Already Started\r\n");
          }
        }
      portEXIT_CRITICAL();
      vTaskDelay(100);
    }


}


