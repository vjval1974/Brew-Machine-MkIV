/*
 * parameters.c
 *
 *  Created on: 26/11/2013
 *      Author: brad
 */


#include "parameters.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "lcd.h"
#include "task.h"
#include "adc.h"
#include "leds.h"
#include "semphr.h"
#include "console.h"

struct Parameters BrewParameters;
void vParametersAppletDisplay( void *pvParameters);

#define LCD_FLOAT( x, y, dp , var ) lcd_printf(x, y, 4, "%d.%d", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));

//#define LCD_FLOAT( x, y, dp,  var ) lcd_printf(x, y, 4, "%d.%04d", (unsigned int)(var), trunc( ( (float)(var - (unsigned int)(var)) ) * 10000) );
xTaskHandle xParametersAppletDisplayHandle = NULL;
// semaphore that stops the returning from the applet to the menu system until the applet goes into the blocked state.
xSemaphoreHandle xAppletRunningSemaphore;

// NEW CODE
#define INT_TYPE 0
#define FLOAT_TYPE 2
#define CHAR_TYPE 3
struct UserParameters
{
    void * element; // use this to point to the parameters that we
                    // want to change
    char type;
    char * text;
};

//this struct to define pointers to the parameters.
struct UserParameters UserParametersList[] =
{
    {&BrewParameters.iGrindTime, INT_TYPE, "Milling Time"},
    {&BrewParameters.fStrikeTemp, FLOAT_TYPE, "Strike Temp"},
    {&BrewParameters.fStrikeLitres, FLOAT_TYPE, "Strike(l)"},
    {&BrewParameters.fMashOutTemp, FLOAT_TYPE, "Mash Out Temp"},
    {&BrewParameters.fMashOutLitres, FLOAT_TYPE, "Mash Out(l)"},
    {&BrewParameters.fSpargeTemp, FLOAT_TYPE, "Sparge Temp"},
    {&BrewParameters.fSpargeLitres, FLOAT_TYPE, "Sparge(l)"},
    {&BrewParameters.iMashTime, INT_TYPE, "Mash Time"},
    {&BrewParameters.iMashOutTime, INT_TYPE, "Mash Out Time"},
    {&BrewParameters.iSpargeTime, INT_TYPE, "Sparge Time"},
    {&BrewParameters.uiBoilTime, INT_TYPE, "Boil Time"},
    {&BrewParameters.uiHopTimes[0], INT_TYPE, "Hop Time 1"},
    {&BrewParameters.uiHopTimes[1], INT_TYPE, "Hop Time 2"},
    {&BrewParameters.uiHopTimes[2], INT_TYPE, "Hop Time 3"},
    {&BrewParameters.uiHopTimes[3], INT_TYPE, "Hop Time 4"},
    {&BrewParameters.uiHopTimes[4], INT_TYPE, "Hop Time 5"},
    {&BrewParameters.uiHopTimes[5], INT_TYPE, "Hop Time 6"},
    {&BrewParameters.uiChillTime, INT_TYPE, "Chill Time "},
    {NULL, NULL, NULL}
};

// END OF NEW CODE



// BREW PARAMETERS
void vParametersInit(void)
{
  //Grind
  BrewParameters.iGrindTime = 8;

  //Mash
  BrewParameters.fHLTMaxLitres = 22.0; // This is the max amount that can be drained
  BrewParameters.fStrikeTemp = 79.6;
  BrewParameters.fMashOutTemp = 99.9;
  BrewParameters.fSpargeTemp = 78.0;
  BrewParameters.fCleanTemp = 70.0; // less than the temp of a new strike. For 2 brews.
  BrewParameters.fStrikeLitres = 10.73;
  BrewParameters.fMashOutLitres = 9.66;
  BrewParameters.fSpargeLitres = 17.41;
  BrewParameters.iMashTime = 60;
  BrewParameters.iPumpTime1 = 15;
  BrewParameters.iStirTime1 = 15;
  BrewParameters.iPumpTime2 = 15;
  BrewParameters.iStirTime2 = 0;

  //Mash Out
  BrewParameters.iMashOutTime = 15;
    BrewParameters.iMashOutPumpTime1 = 3;
    BrewParameters.iMashOutStirTime1 = 5;
    BrewParameters.iMashOutPumpTime2 = 10;
    BrewParameters.iMashOutStirTime2 = 0;

    //Sparge
    BrewParameters.iSpargeTime = 15;
      BrewParameters.iSpargePumpTime1 = 3;
      BrewParameters.iSpargeStirTime1 = 5;
      BrewParameters.iSpargePumpTime2 = 10;
      BrewParameters.iSpargeStirTime2 = 0;


  //Pump
  BrewParameters.iPumpPrimingCycles = 3;
  BrewParameters.iPumpPrimingTime = 2;

  //Boil
  BrewParameters.uiBoilTime = 90;//60;
  BrewParameters.uiBringToBoilTime = 18;
  BrewParameters.uiHopTimes[0] = 90;
  BrewParameters.uiHopTimes[1] = 60;
  BrewParameters.uiHopTimes[2] = 45;
  BrewParameters.uiHopTimes[3] = 15;
  BrewParameters.uiHopTimes[4] = 10;
  BrewParameters.uiHopTimes[5] = 5;

  //Chill
  BrewParameters.uiSettlingRecircTime = 2; //mins
  BrewParameters.uiSettlingTime = 6; //mins
  BrewParameters.uiChillTime = 8; //mins
  BrewParameters.uiChillerPumpPrimingCycles = 3;
  BrewParameters.uiChillerPumpPrimingTime = 3; //seconds
}

//// PARAMETER SET FOR CHECKING
//void vParametersInit(void)
//{
//  //Grind
//  BrewParameters.iGrindTime = 1;
//
//  //Mash
//  BrewParameters.fHLTMaxLitres = 22.0; // This is the max amount that can be drained
//  BrewParameters.fStrikeTemp = 30;
//  BrewParameters.fMashOutTemp = 31;
//  BrewParameters.fSpargeTemp = 32;
//  BrewParameters.fCleanTemp = 33; // less than the temp of a new strike. For 2 brews.
//  BrewParameters.fStrikeLitres = 8.0;
//  BrewParameters.fMashOutLitres = 8.0;
//  BrewParameters.fSpargeLitres = 8.0;
//  BrewParameters.iMashTime = 3;
//  BrewParameters.iPumpTime1 = 1;
//  BrewParameters.iStirTime1 = 1;
//  BrewParameters.iPumpTime2 = 1;
//  BrewParameters.iStirTime2 = 1;
//
//  //Mash Out
//  BrewParameters.iMashOutTime = 2;
//    BrewParameters.iMashOutPumpTime1 = 1;
//    BrewParameters.iMashOutStirTime1 = 1;
//    BrewParameters.iMashOutPumpTime2 = 1;
//    BrewParameters.iMashOutStirTime2 = 0;
//
//    //Sparge
//    BrewParameters.iSpargeTime = 2;
//      BrewParameters.iSpargePumpTime1 = 1;
//      BrewParameters.iSpargeStirTime1 = 1;
//      BrewParameters.iSpargePumpTime2 = 1;
//      BrewParameters.iSpargeStirTime2 = 0;
//
//
//  //Pump
//  BrewParameters.iPumpPrimingCycles = 3;
//  BrewParameters.iPumpPrimingTime = 2;
//
//  //Boil
//  BrewParameters.uiBoilTime = 2;//60;
//  BrewParameters.uiBringToBoilTime = 2;
//  BrewParameters.uiHopTimes[0] = 2;
//  BrewParameters.uiHopTimes[1] = 2;
//  BrewParameters.uiHopTimes[2] = 2;
//  BrewParameters.uiHopTimes[3] = 2;
//  BrewParameters.uiHopTimes[4] = 2;
//  BrewParameters.uiHopTimes[5] = 1; // dont use 0 at moment
//
//  //Chill
//  BrewParameters.uiSettlingRecircTime = 1; //mins
//  BrewParameters.uiSettlingTime = 1; //mins
//  BrewParameters.uiChillTime = 3; //mins
//  BrewParameters.uiChillerPumpPrimingCycles = 3;
//  BrewParameters.uiChillerPumpPrimingTime = 3; //seconds
//}

#define ONE_X1 1
#define ONE_X2 50
#define ONE_Y1 1
#define ONE_Y2 50

#define TWO_X1 51
#define TWO_X2 100
#define TWO_Y1 1
#define TWO_Y2 50

#define THREE_X1 101
#define THREE_X2 150
#define THREE_Y1 1
#define THREE_Y2 50

#define FOUR_X1 1
#define FOUR_X2 50
#define FOUR_Y1 51
#define FOUR_Y2 100

#define FIVE_X1 51
#define FIVE_X2 100
#define FIVE_Y1 51
#define FIVE_Y2 100

#define SIX_X1 101
#define SIX_X2 150
#define SIX_Y1 51
#define SIX_Y2 100

#define SEVEN_X1 1
#define SEVEN_X2 50
#define SEVEN_Y1 101
#define SEVEN_Y2 150

#define EIGHT_X1 51
#define EIGHT_X2 100
#define EIGHT_Y1 101
#define EIGHT_Y2 150

#define NINE_X1 101
#define NINE_X2 150
#define NINE_Y1 101
#define NINE_Y2 150

#define ZERO_X1 1
#define ZERO_X2 50
#define ZERO_Y1 151
#define ZERO_Y2 200

#define DOT_X1 51
#define DOT_X2 100
#define DOT_Y1 150
#define DOT_Y2 200

#define C_X1 101
#define C_X2 150
#define C_Y1 151
#define C_Y2 200

#define BK_X1 200
#define BK_Y1 190
#define BK_X2 315
#define BK_Y2 235
#define BK_W (BK_X2-BK_X1)
#define BK_H (BK_Y2-BK_Y1)

#define UP_X1 200
#define UP_Y1 140
#define UP_X2 315
#define UP_Y2 185
#define UP_W (UP_X2-UP_X1)
#define UP_H (UP_Y2-UP_Y1)

#define DN_X1 200
#define DN_Y1 90
#define DN_X2 315
#define DN_Y2 135
#define DN_W (DN_X2-DN_X1)
#define DN_H (DN_Y2-DN_Y1)

#define SUBMIT_X1 200
#define SUBMIT_Y1 40
#define SUBMIT_X2 315
#define SUBMIT_Y2 85
#define SUBMIT_W (SUBMIT_X2-SUBMIT_X1)
#define SUBMIT_H (SUBMIT_Y2-SUBMIT_Y1)

volatile char input[32];

void vParametersApplet(int init){
  int ii;
  if (init)
        {
                lcd_DrawRect(ONE_X1, ONE_Y1, ONE_X2, ONE_Y2, White);
                lcd_DrawRect(TWO_X1, TWO_Y1, TWO_X2, TWO_Y2, White);
                lcd_DrawRect(THREE_X1, THREE_Y1, THREE_X2, THREE_Y2, White);
                lcd_DrawRect(FOUR_X1, FOUR_Y1, FOUR_X2, FOUR_Y2, White);
                lcd_DrawRect(FIVE_X1, FIVE_Y1, FIVE_X2, FIVE_Y2, White);
                lcd_DrawRect(SIX_X1, SIX_Y1, SIX_X2, SIX_Y2, White);
                lcd_DrawRect(SEVEN_X1, SEVEN_Y1, SEVEN_X2, SEVEN_Y2, White);
                lcd_DrawRect(EIGHT_X1, EIGHT_Y1, EIGHT_X2, EIGHT_Y2, White);
                lcd_DrawRect(NINE_X1, NINE_Y1, NINE_X2, NINE_Y2, White);
                lcd_DrawRect(ZERO_X1, ZERO_Y1, ZERO_X2, ZERO_Y2, White);
                lcd_DrawRect(DOT_X1, DOT_Y1, DOT_X2, DOT_Y2, White);
                lcd_DrawRect(C_X1, C_Y1, C_X2, C_Y2, White);

                lcd_DrawRect(BK_X1, BK_Y1, BK_X2, BK_Y2, Cyan);
                lcd_fill(BK_X1+1, BK_Y1+1, BK_W, BK_H, Magenta);

                lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, Cyan);
                lcd_fill(UP_X1+1, UP_Y1+1, UP_W, UP_H, Blue);

                lcd_DrawRect(DN_X1, DN_Y1, DN_X2, DN_Y2, Cyan);
                lcd_fill(DN_X1+1, DN_Y1+1, DN_W, DN_H, Blue);

                lcd_DrawRect(SUBMIT_X1, SUBMIT_Y1, SUBMIT_X2, SUBMIT_Y2, Cyan);
                lcd_fill(SUBMIT_X1+1, SUBMIT_Y1+1, SUBMIT_W, SUBMIT_H, Green);

                for (ii =0; ii < 32; ii++)
                  {
                    input[ii] = '\0';
                  }
                input[31] = '\0';
                //vConsolePrint("%c\r\n", input[1]);

                lcd_printf(22,1,30,  "PARAMETERS");

                lcd_printf(30, 13, 4, "Back");
                lcd_printf(30, 10, 4, "NEXT");
                lcd_printf(30, 7, 4, "PREV");
                lcd_printf(30, 4, 4, "SUBMIT");
                lcd_printf(3, 1, 4, "1");
                lcd_printf(9, 1, 4, "2");
                lcd_printf(15, 1, 4, "3");

                lcd_printf(3, 4, 4, "4");
                lcd_printf(9, 4, 4, "5");
                lcd_printf(15,4, 4, "6");

                lcd_printf(3, 7, 4, "7");
                lcd_printf(9, 7, 4, "8");
                lcd_printf(15, 7, 4, "9");

                lcd_printf(3, 10, 4, "0");
                lcd_printf(9, 10, 4, ".");
                lcd_printf(15,10, 4, "C");




                //vTaskDelay(2000);
                //adc_init();
                //adc_init();
                //create a dynamic display task
                xTaskCreate( vParametersAppletDisplay,
                    ( signed portCHAR * ) "par_disp",
                    configMINIMAL_STACK_SIZE + 500,
                    NULL,
                    tskIDLE_PRIORITY ,
                    &xParametersAppletDisplayHandle );
        }
vConsolePrint("Parameters Applet\r\n");
}
static int indexToList = 0;
//lcd_DrawRect(UP_X1, UP_Y1, UP_X2, UP_Y2, GREEN);
void vParametersAppletDisplay( void *pvParameters){
        static char tog = 0; //toggles each loop
        for(;;)
        {

            xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY); //take the semaphore so that the key handler wont
                                                                               //return to the menu system until its returned
            //lcd_printf(1,13,15,input);

//                switch (uMashPumpState)
//                {
//                case PUMPING:
//                {
                        if(tog)
                        {
                              lcd_fill(1,220, 180,29, Black);
                              LCD_FLOAT(1, 13, 2, atof(input));
                              lcd_printf(1,14,2, UserParametersList[indexToList].text);
                              LCD_FLOAT(1, 15, 2, atof(UserParametersList[indexToList].element));
                              switch (UserParametersList[indexToList].type)
                                           {
                                           case INT_TYPE:
                                           {
                                             lcd_printf(15,14,2, "%d", *(int*)UserParametersList[indexToList].element);

                                               break;
                                           }

                                           case CHAR_TYPE:
                                           {
                                               printf("char\r\n");
                                               break;
                                           }
                                           case FLOAT_TYPE:
                                           {


                                             LCD_FLOAT(15, 14, 2, *(float*)UserParametersList[indexToList].element);



                                               break;
                                           }
                                           default:
                                           {

                                           }
                                           }
                        }
                        else{
                                lcd_fill(1,210, 180,17, Black);
                        }
//                        break;
//                }
//                case STOPPED:
//                {
//                        if(tog)
//                        {
//                                lcd_fill(1,210, 180,29, Black);
//                                lcd_printf(1,13,11,"MASH_PUMP STOPPED");
//                        }
//                        else
//                          {
//                                lcd_fill(1,210, 180,17, Black);
//                          }
//
//                        break;
//                }
//                default:
//                {
//                        break;
//                }
//                }

                tog = tog ^ 1;
                xSemaphoreGive(xAppletRunningSemaphore); //give back the semaphore as its safe to return now.
                vTaskDelay(500);


        }
}

int iParametersKey(int xx, int yy)
{

  uint16_t window = 0;
  static uint8_t w = 5,h = 5;
  static uint16_t last_window = 0;
  static int indexToChar = 0;
int ii = 0;


  if (xx > ONE_X1 && xx < ONE_X2 && yy > ONE_Y1 && yy < ONE_Y2)
    {
      input[indexToChar] = '1';
      vConsolePrint("ONE\r\n");
      indexToChar++;
    }
  else if (xx > TWO_X1 && xx < TWO_X2 && yy > TWO_Y1 && yy < TWO_Y2)
    {
      input[indexToChar] = '2';
      vConsolePrint("TWO\r\n");
      indexToChar++;
    }
  else if (xx > THREE_X1 && xx < THREE_X2 && yy > THREE_Y1 && yy < THREE_Y2)
    {
      input[indexToChar] = '3';
      vConsolePrint("THREE\r\n");
      indexToChar++;
    }
  else if (xx > FOUR_X1 && xx < FOUR_X2 && yy > FOUR_Y1 && yy < FOUR_Y2)
    {
      input[indexToChar] = '4';
      vConsolePrint("FOUR\r\n");
      indexToChar++;
    }
  else if (xx > FIVE_X1 && xx < FIVE_X2 && yy > FIVE_Y1 && yy < FIVE_Y2)
    {
      input[indexToChar] = '5';
      vConsolePrint("FIVE\r\n");
      indexToChar++;
    }
  else if (xx > SIX_X1 && xx < SIX_X2 && yy > SIX_Y1 && yy < SIX_Y2)
    {
      input[indexToChar] = '6';
      vConsolePrint("SIX\r\n");
      indexToChar++;
    }
  else if (xx > SEVEN_X1 && xx < SEVEN_X2 && yy > SEVEN_Y1 && yy < SEVEN_Y2)
    {
      input[indexToChar] = '7';
      vConsolePrint("SEVEN\r\n");
      indexToChar++;
    }
  else if (xx > EIGHT_X1 && xx < EIGHT_X2 && yy > EIGHT_Y1 && yy < EIGHT_Y2)
    {
      input[indexToChar] = '8';
      vConsolePrint("EIGHT\r\n");
      indexToChar++;
    }
  else if (xx > NINE_X1 && xx < NINE_X2 && yy > NINE_Y1 && yy < NINE_Y2)
    {
      input[indexToChar] = '9';
      vConsolePrint("NINE\r\n");
      indexToChar++;
    }
  else if (xx > ZERO_X1 && xx < ZERO_X2 && yy > ZERO_Y1 && yy < ZERO_Y2)
    {
      input[indexToChar] = '0';
      vConsolePrint("ZERO\r\n");
      indexToChar++;
    }
  else if (xx > DOT_X1 && xx < DOT_X2 && yy > DOT_Y1 && yy < DOT_Y2)
    {
      input[indexToChar] = '.';
      vConsolePrint("DOT\r\n");
      indexToChar++;
    }
  else if (xx > C_X1 && xx < C_X2 && yy > C_Y1 && yy < C_Y2)
    {
      for (ii = 0; ii < 32; ii++)
                   {
                     input[ii] = '\0';
                   }
                 indexToChar=0;
      vConsolePrint("C\r\n");
    }
  else if (xx > UP_X1 && xx < UP_X2 && yy > UP_Y1 && yy < UP_Y2)
      {
      indexToList++;
      if (UserParametersList[indexToList].text == NULL)
        {
          indexToList = 0;
        }

        vConsolePrint(UserParametersList[indexToList].text );
      }
  else if (xx > DN_X1 && xx < DN_X2 && yy > DN_Y1 && yy < DN_Y2)
       {
      indexToList--;
            if (indexToList <= 0)
              {
                indexToList = 0;
              }
            vConsolePrint(UserParametersList[indexToList].text );

       }
  else if (xx > SUBMIT_X1 && xx < SUBMIT_X2 && yy > SUBMIT_Y1 && yy < SUBMIT_Y2)
         {
      switch (UserParametersList[indexToList].type)
             {
             case INT_TYPE:
             {
                 *(int*)UserParametersList[indexToList].element = atoi(input);

                 break;
             }

             case CHAR_TYPE:
             {
                 printf("char\r\n");
                 break;
             }
             case FLOAT_TYPE:
             {
               *(float*)UserParametersList[indexToList].element = atof(input);





                 break;
             }
             default:
             {

             }
             }
           vConsolePrint("SUBMIT PRESSED \r\n");
           for (ii = 0; ii < 32; ii++)
             {
               input[ii] = '\0';
             }
           indexToChar=0;

         }
  else if (xx > BK_X1 && yy > BK_Y1 && xx < BK_X2 && yy < BK_Y2)
    {
      //try to take the semaphore from the display applet. wait here if we cant take it.
      xSemaphoreTake(xAppletRunningSemaphore, portMAX_DELAY);
      //delete the display applet task if its been created.
      if (xParametersAppletDisplayHandle != NULL)
        {
          vTaskDelete(xParametersAppletDisplayHandle);
          vTaskDelay(100);
          xParametersAppletDisplayHandle = NULL;
        }

      //return the semaphore for taking by another task.
      xSemaphoreGive(xAppletRunningSemaphore);
      indexToList=0;
      indexToChar=0;
      return 1;

    }
//vConsolePrint(input);
  //printf("%d\r\n", indexToChar);
  printf("%s\r\n", input); // SHOULDNT USE... only for debugging

  vTaskDelay(10);
  return 0;

}

