/*
 * Flow1.h
 *
 *  Created on: May 9, 2013
 *      Author: brad
 */

#ifndef FLOW1_H_
#define FLOW1_H_

typedef enum
{
	NOT_MEASURING_FLOW,
	MEASURING_FLOW
} FlowMeasuringState_t;

typedef enum
{
	NOT_FLOWING,
	FLOWING
} FlowState_t;



void vFlow1Init(void);
void vResetFlow1Pulses(void);

void vTaskLitresToBoil( void * pvParameters );
void vTaskLitresToMash( void * pvParameters );
int iFlow1Key(int xx, int yy);
void vFlow1Applet(int init);
float fGetBoilFlowLitres(void);
float fGetMashFlowLitres(void);
void vResetFlow1(void);
FlowMeasuringState_t xGetBoilFlowMeasuringState();
void vSetBoilFlowMeasuringState(FlowMeasuringState_t state);

#endif /* FLOW1_H_ */
