/*
 * hlt.h
 *
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#ifndef HLT_H_
#define HLT_H_
#include "queue.h"
#include "valves.h"

//#define HLT_LEVEL_ADC_CHAN 10 // corresponds to PORT C, Bit 0
#define HLT_SSR_PORT GPIOC
#define HLT_SSR_PIN GPIO_Pin_11

#define HLT_LEVEL_CHECK_PIN GPIO_Pin_4
#define HLT_LEVEL_CHECK_PORT GPIOA
#define HLT_HIGH_LEVEL_PIN GPIO_Pin_0
#define HLT_HIGH_LEVEL_PORT GPIOA

#define HLT_FLOW_PORT GPIOE
#define HLT_FLOW_PIN GPIO_Pin_4

#define HEATING 1
#define OFF 0

typedef enum
{
	HLT_STATE_IDLE,
	HLT_STATE_FILL_HEAT,
	HLT_STATE_DRAIN,
	HLT_STATE_AT_TEMP
} HltBrewState;

void vHLTApplet(int init);
int  HLTKey(int xx, int yy);
void hlt_init(void);
void vHLTAppletCallback (int in_out);
void vTaskBrewHLT(void * pvParameters);
void vTaskHLTLevelChecker( void * pvParameters);
unsigned int uiGetActualLitresDelivered(void);


typedef enum
{
	HLT_LEVEL_LOW,
	HLT_LEVEL_MID,
	HLT_LEVEL_HIGH
} HltLevel;

typedef enum
{
	HLT_NOT_HEATING,
	HLT_HEATING
} HltHeatingState;
typedef enum
{
	HLT_CMD_IDLE,
	HLT_CMD_HEAT_AND_FILL,
	HLT_CMD_DRAIN
} HltCommand;

typedef struct
{
	HltCommand command;
	int iData1;
    int iData2;
    float fData3;
    float fData4;
    unsigned char ucStepNumber;
    char pcTxt[32];
} HltMessage;

typedef struct HltState
{
  HltLevel level;
  HltHeatingState heatingState;
  HltBrewState hltBrewState;
  float temp_float;
  int temp_int;
  char levelStr[25];
  ValveState filling;
  char fillingStr [16];
  ValveState draining;
  char drainingStr [16];
} HltState;


HltLevel xGetHltLevel();
HltState GetHltState();
HltHeatingState xGetHltHeatingState();
void vPrintHltMessage(HltMessage msg);

extern xTaskHandle xHeatHLTTaskHandle, xHLTAppletDisplayHandle;
extern xTaskHandle xBrewHLTTaskHandle;
extern xQueueHandle xBrewTaskHLTQueue;
extern xQueueHandle xHltTaskQueue;
extern xTaskHandle xTaskHLTLevelCheckerTaskHandle;
#endif /* HLT_H_ */
