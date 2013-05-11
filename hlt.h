/*
 * hlt.h
 *
 *  Created on: Dec 3, 2012
 *      Author: brad
 */

#ifndef HLT_H_
#define HLT_H_


#define HLT_LEVEL_ADC_CHAN 10 // corresponds to PORT C, Bit 0
#define HLT_SSR_PORT GPIOC
#define HLT_SSR_PIN GPIO_Pin_11

#define HLT_LEVEL_CHECK_PIN GPIO_Pin_3
#define HLT_LEVEL_CHECK_PORT GPIOE
#define HLT_FLOW_PORT GPIOE
#define HLT_FLOW_PIN GPIO_Pin_4

#define HLT_MAX_LITRES 16 //full hlt
#define HLT_ANALOGUE_MAX 1800 // change this value so that it corresponds to 16 litres of water

float fGetHLTLevel(void);
void vTaskHeatHLT( void * pvParameters);
void vHLTApplet(int init);
int  HLTKey(int xx, int yy);
void hlt_init(void);
void vHLTAppletCallback (int in_out);
extern xTaskHandle xHeatHLTTaskHandle, xHLTAppletDisplayHandle;
#endif /* HLT_H_ */
