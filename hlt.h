/*
 * hlt.h
 *
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#ifndef HLT_H_
#define HLT_H_
#include "queue.h"

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

#define HLT_STATE_BASE 20
#define HLT_STATE_IDLE 20
#define HLT_STATE_FILL_HEAT 21
#define HLT_STATE_DRAIN 23
#define HLT_STATE_AT_TEMP 24

#define HLT_LEVEL_LOW 0
#define HLT_LEVEL_MID 1
#define HLT_LEVEL_HIGH 2

#define HLT_MAX_LITRES 16 //full hlt
#define HLT_MIN_LITRES 4 // hlt level at low level sensor
//#define HLT_ANALOGUE_MAX 1800 // change this value so that it corresponds to 16 litres of water
//#define HLT_ANALOGUE_MIN 800 // change this so it corresponds with the hlt min litres.
//float fGetHLTLevel(void);
void vTaskHeatHLT( void * pvParameters);
void vHLTApplet(int init);
int  HLTKey(int xx, int yy);
void hlt_init(void);
void vHLTAppletCallback (int in_out);
void vTaskBrewHLT(void * pvParameters);
void vTaskHLTLevelChecker( void * pvParameters);

extern xTaskHandle xHeatHLTTaskHandle, xHLTAppletDisplayHandle;
extern xTaskHandle xBrewHLTTaskHandle;
extern xQueueHandle xBrewTaskHLTQueue;
extern xTaskHandle xTaskHLTLevelCheckerTaskHandle;
#endif /* HLT_H_ */
