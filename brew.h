/*
 * brew.h
 *
 *  Created on: 14/11/2013
 *      Author: brad
 */

#ifndef BREW_H_
#define BREW_H_

#include <queue.h>

// Canvas area for brew
// --------------------
#define CANVAS_X1 0
#define CANVAS_X2 254
#define CANVAS_Y1 30
#define CANVAS_Y2 158
#define CANVAS_W (CANVAS_X2-CANVAS_X1)
#define CANVAS_H (CANVAS_Y2-CANVAS_Y1)

// Top Banner (Heading)
// --------------------
#define TOP_BANNER_X1 0
#define TOP_BANNER_X2 254
#define TOP_BANNER_Y1 0
#define TOP_BANNER_Y2 29
#define TOP_BANNER_W (TOP_BANNER_X2-TOP_BANNER_X1)
#define TOP_BANNER_H (TOP_BANNER_Y2-TOP_BANNER_Y1)

// Buttons
// -------
#define BUTTON_1_X1 259
#define BUTTON_1_X2 318
#define BUTTON_1_Y1 0
#define BUTTON_1_Y2 49
#define BUTTON_1_W (BUTTON_1_X2-BUTTON_1_X1)
#define BUTTON_1_H (BUTTON_1_Y2-BUTTON_1_Y1)

#define BUTTON_2_X1 259
#define BUTTON_2_X2 318
#define BUTTON_2_Y1 54
#define BUTTON_2_Y2 104
#define BUTTON_2_W (BUTTON_2_X2-BUTTON_2_X1)
#define BUTTON_2_H (BUTTON_2_Y2-BUTTON_2_Y1)

#define BUTTON_3_X1 259
#define BUTTON_3_X2 318
#define BUTTON_3_Y1 109
#define BUTTON_3_Y2 159
#define BUTTON_3_W (BUTTON_3_X2-BUTTON_3_X1)
#define BUTTON_3_H (BUTTON_3_Y2-BUTTON_3_Y1)

#define BUTTON_4_X1 0
#define BUTTON_4_X2 60
#define BUTTON_4_Y1 192
#define BUTTON_4_Y2 235
#define BUTTON_4_W (BUTTON_4_X2-BUTTON_4_X1)
#define BUTTON_4_H (BUTTON_4_Y2-BUTTON_4_Y1)

#define BUTTON_5_X1 65
#define BUTTON_5_X2 125
#define BUTTON_5_Y1 192
#define BUTTON_5_Y2 235
#define BUTTON_5_W (BUTTON_5_X2-BUTTON_5_X1)
#define BUTTON_5_H (BUTTON_5_Y2-BUTTON_5_Y1)

#define BUTTON_6_X1 130
#define BUTTON_6_X2 190
#define BUTTON_6_Y1 192
#define BUTTON_6_Y2 235
#define BUTTON_6_W (BUTTON_6_X2-BUTTON_6_X1)
#define BUTTON_6_H (BUTTON_6_Y2-BUTTON_6_Y1)

// Resume Brew Sub-Applet alternate pushbuttons
//---------------------------------------------
#define RES_STEP_INC_X1 BUTTON_4_X1
#define RES_STEP_INC_X2 BUTTON_4_X2
#define RES_STEP_INC_Y1 BUTTON_4_Y1
#define RES_STEP_INC_Y2 BUTTON_4_Y2
#define RES_STEP_INC_W (RES_STEP_INC_X2-RES_STEP_INC_X1)
#define RES_STEP_INC_H (RES_STEP_INC_Y2-RES_STEP_INC_Y1)

#define RES_STEP_DEC_X1 BUTTON_5_X1
#define RES_STEP_DEC_X2 BUTTON_5_X2
#define RES_STEP_DEC_Y1 BUTTON_5_Y1
#define RES_STEP_DEC_Y2 BUTTON_5_Y2
#define RES_STEP_DEC_W (RES_STEP_DEC_X2-RES_STEP_DEC_X1)
#define RES_STEP_DEC_H (RES_STEP_DEC_Y2-RES_STEP_DEC_Y1)


// Back Button
//------------
#define BK_X1 200
#define BK_Y1 191
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

// Deny Quit Button
// ----------------
#define Q_X1 100
#define Q_Y1 50
#define Q_X2 200
#define Q_Y2 150
#define Q_W (Q_X2-Q_X1)
#define Q_H (Q_Y2-Q_Y1)

// Button States for key handler
#define BUTTON_1 1
#define BUTTON_2 2
#define BUTTON_3 3
#define BUTTON_4 4
#define BUTTON_5 5
#define BUTTON_6 6
#define QUIT_BUTTON 7
#define NO_BUTTON 255
struct State
{
      unsigned char ucStep;
      portTickType xLastWakeTime;
      uint16_t uSecondsElapsed;
      uint16_t uMinutesElapsed;
      uint16_t uHoursElapsed;
      uint16_t uSecondCounter;
      unsigned char ucRunningState;
      unsigned char ucTarget;
      unsigned char ucHopAddition;
      unsigned char ucTargetHLTVolume;
      unsigned char ucTargetHLTTemp;
      unsigned char ucTargetWaterVolume;
      unsigned char ucHLTState;
};

extern struct State BrewState;
extern const char * pcTASKS[6];
extern const char * pcCraneStates[6];
void vTaskBrew(void * pvParameters);
void vBrewApplet(int init);
int iBrewKey(int xx, int yy);
void vBrewRemoteStart();

extern const int STEP_COMPLETE;
extern const int STEP_FAILED;
extern const int STEP_WAIT;



unsigned char ucGetBrewHoursElapsed();


unsigned char ucGetBrewMinutesElapsed();

unsigned char ucGetBrewSecondsElapsed();

unsigned char ucGetBrewStepMinutesElapsed();
unsigned char ucGetBrewStep();
unsigned char ucGetBrewStepSecondsElapsed();

unsigned int uiGetBrewAppletDisplayHWM();
unsigned int uiGetBrewResAppletHWM();
unsigned int uiGetBrewStatsAppletHWM();
unsigned int uiGetBrewGraphAppletHWM();
unsigned int uiGetBrewTaskHWM();




extern xQueueHandle xBrewTaskReceiveQueue,  xBrewAppletTextQueue;

#endif /* BREW_H_ */
