////////////////////////////////////////////////////////////////////////////
// Brad Goold 2012 
// DS1820 driver for STM32F103VE using FreeRTOS
////////////////////////////////////////////////////////////////////////////



#include "stm32f10x.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "ds1820.h"
#include "queue.h"
#include "lcd.h"
#include "console.h"


// ROM COMMANDS
#define MATCH_ROM	0x55
#define	SEARCH_ROM	0xF0
#define SKIP_ROM	0xCC
#define READ_ROM        0x33
#define ALARM_SEARCH    0xEC

// FUNCTION COMMANDS
#define CONVERT_TEMP     0x44
#define COPY_SCRATCHPAD  0x48
#define WRITE_SCRATCHPAD 0x4E
#define READ_SCRATCHPAD  0xBE
#define RECALL_EEPROM    0xB8
#define READ_PS          0xB4

#define MAX_SENSORS      10
#define NUM_SENSORS      7
// ERROR DEFINES
#define BUS_ERROR      0xFE
#define PRESENCE_ERROR 0xFD
#define NO_ERROR       0x00

//Default Temp Sensor Addresses
//#define HLT_TEMP_SENSOR "\x10\x99\xd7\x39\x01\x08\x00\xd5"

#define HLT_TEMP_SENSOR  "\x28\xC6\x52\xB6\x04\x00\x00\x23"
//#define MASH_TEMP_SENSOR "\x10\x6c\xe2\x38\x01\x08\x00\x95"
//#define MASH_TEMP_SENSOR "\x10\x99\xd7\x39\x01\x08\x00\xd5"
#define MASH_TEMP_SENSOR "\x28\x3C\xE7\xB5\x04\x00\x00\x8B"
#define CABINET_TEMP_SENSOR "\x10\x83\xc5\x1e\x02\x08\x00\xAC"
//#define CABINET_TEMP_SENSOR "\x28\xe1\xe6\xb5\x04\x00\x00\xc8"
//#define AMBIENT_TEMP_SENSOR "\x10\x9c\xa4\x1e\x02\x08\x00\x0f"
#define AMBIENT_TEMP_SENSOR "\x10\x81\x45\x18\x00\x08\x00\x04"
#define HLT_SSR_TEMP_SENSOR "\x10\xA5\x91\x1E\x02\x08\x00\x22"

#define BOIL_SSR_TEMP_SENSOR "\x10\5Dc\xB0\x1E\x02\x08\x00\xC4"

#define SPARE_2_PIN_SENSOR "\x10\x81\x45\x18\x00\x08\x00\x04"
#define WATERPROOF_SENSOR "\x28\xe1\xe6\xb5\x04\x00\x00\xc8"


// BUS COMMANDS
#define DQ_IN()  {DS1820_PORT->CRH&=0xFFFFF0FF;DS1820_PORT->CRH |= 0x00000400;}
#define DQ_OUT() {DS1820_PORT->CRH&=0xFFFFF0FF;DS1820_PORT->CRH |= 0x00000300;}
#define DQ_SET()   GPIO_SetBits(DS1820_PORT, DS1820_PIN)
#define DQ_RESET() GPIO_ResetBits(DS1820_PORT, DS1820_PIN)
#define DQ_READ()  GPIO_ReadInputDataBit(DS1820_PORT, DS1820_PIN)

xTaskHandle xTaskDS1820DisplayTempsHandle;

// STATIC FUNCTIONS
static void ds1820_convert(void);
static void ds1820_init(void);
static unsigned char ds1820_reset(void);
static void ds1820_write_bit(uint8_t bit);
static uint8_t ds1820_read_bit(void);
static void ds1820_write_byte(uint8_t byte);
static uint8_t ds1820_read_byte(void);
static void ds1820_convert(void);
static uint8_t ds1820_search();
static float ds1820_read_device(uint8_t * rom_code);
static void delay_us(uint16_t count); 
static void ds1820_error(uint8_t code);
static unsigned char ds1820_power_reset(void); // created to see if the bus will reset with a longer time


uint8_t rom[8]; //temporary to store rom codes
float temps[NUM_SENSORS]; // holds the converted temperatures from the devices
char * b[NUM_SENSORS]; // holds the 64 bit addresses of the temp sensors

////////////////////////////////////////////////////////////////////////////
// Interfacing Function
////////////////////////////////////////////////////////////////////////////
void vTaskDS1820Convert( void *pvParameters ){
    char buf[30];
    static char print_buf[80];
    int ii = 0;
    static float fTemp[NUM_SENSORS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, fLastTemp[NUM_SENSORS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};




    // initialise the bus
    ds1820_init();
    if (ds1820_reset() ==PRESENCE_ERROR)
    {
        ds1820_error(PRESENCE_ERROR);
        vConsolePrint("Presence Error, Deleting DS1820 Task\r\n");
        vTaskDelete(NULL); // if this task fails... delete it
    }
  
    // Allocate memory for sensors
    b[HLT] = (char *) malloc (sizeof(rom)+1);
    b[MASH] = (char *) malloc (sizeof(rom)+1);
    b[CABINET] = (char *) malloc (sizeof(rom)+1);
    b[AMBIENT] = (char *) malloc (sizeof(rom)+1);
    b[HLT_SSR] = (char *) malloc (sizeof(rom)+1);
    b[BOIL_SSR] = (char *) malloc (sizeof(rom)+1);
    b[WATERPROOF] = (char *) malloc(sizeof(rom)+1);

    // Copy default values
    memcpy(b[HLT], HLT_TEMP_SENSOR, sizeof(HLT_TEMP_SENSOR)+1);
    memcpy(b[MASH], MASH_TEMP_SENSOR, sizeof(MASH_TEMP_SENSOR)+1);
    memcpy(b[CABINET], CABINET_TEMP_SENSOR, sizeof(CABINET_TEMP_SENSOR)+1);
    memcpy(b[AMBIENT], AMBIENT_TEMP_SENSOR, sizeof(AMBIENT_TEMP_SENSOR)+1);  
    memcpy(b[HLT_SSR], HLT_SSR_TEMP_SENSOR, sizeof(HLT_SSR_TEMP_SENSOR)+1);
    memcpy(b[BOIL_SSR], BOIL_SSR_TEMP_SENSOR, sizeof(BOIL_SSR_TEMP_SENSOR)+1);
    memcpy(b[WATERPROOF], WATERPROOF_SENSOR, sizeof(WATERPROOF_SENSOR)+1);
    ds1820_search();

    //if (rom[0] == 0x10)
      if (rom[0] != 0)
      {
        sprintf(print_buf, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n",rom[0],
            rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
        vConsolePrint(print_buf);
      }
    else
      {
        vConsolePrint("NO SENSOR\r\n");
      }

      ds1820_convert();

      vTaskDelay(750/portTICK_RATE_MS); // wait for conversion

      // save values in array for use by application
      for (ii = 0 ; ii < NUM_SENSORS; ii++)
          temps[ii] = ds1820_read_device(b[ii]);



    for (;;)
    {

        ds1820_convert();
        
        vTaskDelay(750/portTICK_RATE_MS); // wait for conversion

        // save values in array for use by application
        for (ii = 0 ; ii < NUM_SENSORS; ii++){
            fTemp[ii] = ds1820_read_device(b[ii]);
            // Dont want spurious values - crude.
            if (fTemp[ii] < 120.0){
                if (fTemp[ii] < (temps[ii] + 5.0))
                  temps[ii] = fTemp [ii];
                else if (fTemp[ii] > (temps[ii] - 5.0))
                  temps[ii] = fTemp [ii];
                else if (fTemp[ii] != 85.0)
                  temps[ii] = fTemp[ii];
            }
            if (fTemp[ii] == 0.0)
              {
                vConsolePrint("zero values. Temp Bus Resetting\r\n");
                ds1820_power_reset();
                ds1820_reset();
              }
        }



        taskYIELD();

        //printf("DS1820 Convert Water Mark = %u\r\n", uxTaskGetStackHighWaterMark(NULL));
      // Uncomment below to send temps to the console
/*
        printf("HLT Temp = %.2fDeg-C\r\n", temps[HLT]);
        printf("Mash Temp = %.2fDeg-C\r\n", temps[MASH]);
        printf("Cabinet Temp = %.2fDeg-C\r\n", temps[CABINET]);
        printf("Ambient Temp = %.2fDeg-C\r\n", temps[AMBIENT]);
        printf("HLT SSR Temp = %.2fDeg-C\r\n", temps[HLT_SSR]);
        printf("BOIL SSR Temp = %.2fDeg-C\r\n", temps[BOIL_SSR]);
*/
        //vTaskDelay(200); // I put this in here to check that this process
        // wasnt getting switched in if theres a problem with a sensor.

    }
    
}
////////////////////////////////////////////////////////////////////////////

float ds1820_get_temp(unsigned char sensor){
    
    return temps[sensor];
}
////////////////////////////////////////////////////////////////////////////


void ds1820_error(uint8_t code){
  static char print_buf[80];
  switch(code)
  {
  case PRESENCE_ERROR:
    {
      sprintf(print_buf, "DS1820 No sensor present on Bus. Bus OK.\r\n");
      vConsolePrint(print_buf);
      break;
    }
  case BUS_ERROR:
    {
      sprintf(print_buf, "DS1820 Bus Fault\r\n");
      vConsolePrint(print_buf);
      break;
    }
  default:
    {

      sprintf(print_buf, "DS1820 Undefined error\r\n");
      vConsolePrint(print_buf);
      break;
    }
  }
}

void ds1820_search_key(uint16_t x, uint16_t y){
    uint16_t window = 255;
    char code[30];   
    char lcd_string[30];
    char console[100];
    float sensor_temp = 0.00;
    static uint16_t last_window = 0; 
    

/*   
    // window locations
    if (touchIsInWindow(x,y, 0,0, 150,50) == pdTRUE)
        window = 0;
    
    else  if (touchIsInWindow(x,y, 0,50, 150,100) == pdTRUE)
        window = 1;
    
    
    else if (touchIsInWindow(x,y, 0,140, 120,220) == pdTRUE)
        window = 2;
    
    else  if (touchIsInWindow(x,y, 120,140, 239,220) == pdTRUE)
        window = 3;
    
    
    else  if (touchIsInWindow(x,y, 0,220, 120,300) == pdTRUE)
        window = 4;
    
    
    else  if (touchIsInWindow(x,y, 120,220, 239,300) == pdTRUE)
        window = 5;
    
    
    else  if (touchIsInWindow(x,y, 160, 0, 230,100) == pdTRUE)
        window = 6;
*/  
    //Back Button
    if (window == 6)
    {
     
        
    }

#if 0  // needs to be rewritten to use lcd_printf   
    //get code button
    else if (window == 0){
        
        ds1820_search();
        if (rom[0] == 0x10)    
            sprintf(code, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\0",rom[0], 
                    rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
        else sprintf(code, "NO SENSOR\0"); 
        lcd_PutString(2, 101, code, Blue, Black);
        
  
    }
    //get temp button
    else if (window == 1){
        sensor_temp = ds1820_read_device(rom);
        sprintf(code, "Current Sensor = %.2f\0", sensor_temp);
        lcd_PutString(2, 117, code, Blue, Black);
        
        
    }
    //assign current to HLT
    else if (window == 2){
        
        if (rom[0] == 0x10)  {
           
            memcpy(b[HLT], rom, sizeof(rom)+1);
            sprintf(lcd_string, "COPIED\0"); 
            sprintf(console, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n Assigned to HLT\r\n",rom[0], 
                    rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
            xQueueSendToBack(xConsoleQueue, &console, 0);
        }
        else sprintf(lcd_string, "NO SENSOR TO COPY\0"); 
        lcd_PutString(2, 160, lcd_string, Blue, Black);
        
    }
    //assign current to Mash
    else if (window == 3){
       
        if (rom[0] == 0x10)  {  
            memcpy(b[MASH], rom, sizeof(rom)+1);
            
            sprintf(lcd_string, "COPIED\0"); 
            sprintf(console, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n Assigned to MASH\r\n",rom[0], 
                    rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
            xQueueSendToBack(xConsoleQueue, &console, 0);
        }
        else sprintf(lcd_string, "NO SENSOR TO COPY\0"); 
        lcd_PutString(121, 160, lcd_string, Blue, Black);
        
    }
    //assign current to CABINET
    else if (window == 4){
        if (rom[0] == 0x10)  {  
            memcpy(b[CABINET], rom, sizeof(rom)+1);
            sprintf(lcd_string, "COPIED\0"); 
            sprintf(console, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n Assigned to CABINET\r\n",rom[0],rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
             xQueueSendToBack(xConsoleQueue, &console, 0);
        }
        else sprintf(lcd_string, "NO SENSOR TO COPY\0"); 
        lcd_PutString(2, 250, lcd_string, Blue, Black);
    }
    //assign current to AMBIENT
    else if (window == 5){
        if (rom[0] == 0x10)  {
            memcpy(b[AMBIENT], rom, sizeof(rom)+1);  
            sprintf(lcd_string, "COPIED\0"); 
            sprintf(console, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n Assigned to AMBIENT\r\n",rom[0], 
                    rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
            xQueueSendToBack(xConsoleQueue, &console, 0);
        }
        else sprintf(code, "NO SENSOR TO COPY\0"); 
        lcd_PutString(121, 250, lcd_string, Blue, Black);
        
    }
    
#endif
}



////////////////////////////////////////////////////////////////////////////
// Static Functions 
////////////////////////////////////////////////////////////////////////////

static void delay_us(uint16_t count){ 
    
    uint16_t TIMCounter = count;
    TIM_Cmd(TIM2, ENABLE);
    TIM_SetCounter(TIM2, TIMCounter);
    while (TIMCounter)
    {
        TIMCounter = TIM_GetCounter(TIM2);
    }
    TIM_Cmd(TIM2, DISABLE);
}
////////////////////////////////////////////////////////////////////////////

static void ds1820_init(void) {
  int i;
    // Initialise timer 2 for counting
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructure.TIM_Period = 1;                
    TIM_TimeBaseStructure.TIM_Prescaler = 72;       //72MHz->1MHz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;   
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Down;  
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    GPIO_InitTypeDef GPIO_InitStructure;
    
    // PC10-DQ
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);


    GPIO_InitStructure.GPIO_Pin =  DS1820_POWER_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(DS1820_POWER_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(DS1820_POWER_PORT, DS1820_POWER_PIN); //pull low

    //printf("ioMode = %x\r\n", GPIOC->CRH);
    for (i = 0; i < 8; i++){
        rom[i] = 0;
    }
}



////////////////////////////////////////////////////////////////////////////

static unsigned char ds1820_reset(void)
{
    portENTER_CRITICAL();
    DQ_OUT();
    DQ_SET();
    DQ_RESET();
    delay_us(500);
    DQ_IN();
    
    delay_us(60);
    if (DQ_READ()!= 0)
    {
        portEXIT_CRITICAL();
        return PRESENCE_ERROR;
       
    }
    portEXIT_CRITICAL();

    delay_us(240); 
    
    return NO_ERROR;

}

static unsigned char ds1820_power_reset(void) // created to see if the bus will reset with a longer time
{
    GPIO_SetBits(DS1820_POWER_PORT, DS1820_POWER_PIN);
    vTaskDelay(5000);
    GPIO_ResetBits(DS1820_POWER_PORT, DS1820_POWER_PIN);
    return 0;
}


////////////////////////////////////////////////////////////////////////////

static void ds1820_write_bit(uint8_t bit){

    DQ_OUT();
    DQ_SET(); // make sure bus is high
    portENTER_CRITICAL();
    DQ_RESET(); // pull low for 2us
    delay_us(2);
    if (bit){
        DQ_IN();
    }
    else DQ_RESET();
    delay_us(60);
    portEXIT_CRITICAL();
    DQ_IN(); // return bus
}
////////////////////////////////////////////////////////////////////////////

static uint8_t ds1820_read_bit(void){
    uint8_t bit;
    delay_us(1);
    DQ_OUT();
    DQ_SET(); // make sure bus is high
    portENTER_CRITICAL();
    DQ_RESET(); // pull low for 2us
    delay_us(1);
    DQ_IN(); // give bus back to DS1820;
    delay_us(5); 
    bit = DQ_READ();
    delay_us(56);
    //DQ_OUT();
    //DQ_SET();
    portEXIT_CRITICAL();
    if (bit) 
        return 1;
    else return 0; 
}
////////////////////////////////////////////////////////////////////////////

static void ds1820_write_byte(uint8_t byte){

    delay_us(100);
    portENTER_CRITICAL();
    int ii;
    for (ii = 0; ii < 8; ii++) {
        if (byte&0x01){
            ds1820_write_bit(1);
            // printf("1 written\r\n");
        }
        else {
            ds1820_write_bit(0);
            //printf("0 written\r\n");
        }
        byte = byte>>1;
    } 
    //delay_us(100);
   portEXIT_CRITICAL();
}
////////////////////////////////////////////////////////////////////////////

static uint8_t ds1820_read_byte(void){
    int ii;
    uint8_t bit, byte = 0;
    portENTER_CRITICAL();
    delay_us(1);
    for (ii = 0; ii < 8; ii++) {
        byte = byte >> 1;
        bit = ds1820_read_bit();
        if (bit==0) {
            byte = byte & 0x7F;
        }
        else byte = byte | 0x80;
        
        
    }
     portEXIT_CRITICAL();
    return byte;
}
////////////////////////////////////////////////////////////////////////////

static void ds1820_convert(void){
    int code = 0;
    portENTER_CRITICAL();
    code = ds1820_reset();
    if (code!= NO_ERROR)
      ds1820_error(code);

    ds1820_write_byte(SKIP_ROM); 
    ds1820_write_byte(CONVERT_TEMP);
    ds1820_reset();
    portEXIT_CRITICAL();
    DQ_IN();
       
}
////////////////////////////////////////////////////////////////////////////

static uint8_t ds1820_search(){
    uint8_t ii;
    //char console_text[30];
    
    ds1820_reset();
    ds1820_write_byte(READ_ROM); 
    portENTER_CRITICAL();
    for (ii = 0; ii < 8; ii++){
        rom[ii] = ds1820_read_byte();
    }
    portEXIT_CRITICAL();
    ds1820_reset();

    return 0;
   
}
////////////////////////////////////////////////////////////////////////////
#define POW_NEG_1 0.5
#define POW_NEG_2 0.25
#define POW_NEG_3 0.125
#define POW_NEG_4 0.0625


#define CONSOLE_FLOAT_S( dp ,cp, var ) sprintf(cp, "%03d.%03d\r\n", (unsigned int)floor(var), (unsigned int)((var-floor(var))*pow(10, dp)));
static float ds1820_read_device(uint8_t * rom_code){
    float retval;
    uint16_t ds1820_temperature1 = 10000;
    int ii; 
    uint8_t code = 0;
    uint8_t sp1[9]; //temp to hold scratchpad memory
    char c[25];
    int tt = 0;
    float rem = 0.0;
   
    //portENTER_CRITICAL();
    code = ds1820_reset();
    if (code != NO_ERROR)
        ds1820_error(code);
    portENTER_CRITICAL();
    ds1820_reset();
    ds1820_write_byte(MATCH_ROM);

    for (ii = 0; ii < 8; ii++)
    {
        ds1820_write_byte(rom_code[ii]);
    }
    ds1820_write_byte(READ_SCRATCHPAD);
    for (ii = 0; ii < 9; ii++){
        sp1[ii] = ds1820_read_byte();
    }
    
    ds1820_reset();

    ds1820_temperature1 = sp1[1] & 0x0f;
    ds1820_temperature1 <<= 8;
    ds1820_temperature1 |= sp1[0];
    // if the sensor is a 'B' type, we need a different conversion
    if (rom_code[0] == '\x28')
      {
        ds1820_temperature1 >>= 4;

      rem = 0.0;
            if (sp1[0]&(0x01))
              rem += POW_NEG_4;
            if (sp1[0]&(0x02))
              rem += POW_NEG_3;
            if (sp1[0]&(0x04))
              rem += POW_NEG_2;
            if (sp1[0]&(0x08))
              rem += POW_NEG_1;

       retval = (float)ds1820_temperature1 + rem;
      }
    else
      {
        unsigned char remain = sp1[6];
        ds1820_temperature1 >>= 1;
        ds1820_temperature1 = (ds1820_temperature1 * 100) -  25  + (100 * 16 - remain * 100) / (16);
        retval = ((float)ds1820_temperature1/100);
      }
    portEXIT_CRITICAL();

//    if (sp1[1] != '\xff')
//      {
//        sprintf(c, "BLAH %x, %x, %x, %x\r\n", sp1[0]&0x01, sp1[0]&0x02, sp1[0]&0x04, sp1[0]&0x08);
//        vConsolePrint(c);
//      }

//    CONSOLE_FLOAT_S(3,c,retval);
//    vConsolePrint(c);
    return retval;

}
////////////////////////////////////////////////////////////////////////////

void ds1820_search_applet(void)
{
    lcd_clear(Black);
    
    lcd_printf(2, 3,  25, "Get ROM code from");
    lcd_printf(2, 19, 25, "Current DS1820");
    lcd_printf(2, 35, 25, "(One at a time!)");
    lcd_printf(2, 53, 25, "Display the temp");
    lcd_printf(2, 69, 25, "of current sensor");
    char * t1 = "HLT\0";
    char * t2 = "MASH\0";
    char * t3 = "CABINET\0";
    char * t4 = "AMBIENT\0";
    lcd_DrawRect(160, 0, 230, 100, Red);
//    lcd_PutString(179, 46, "BACK"); 
//    lcd_draw_applet_options(t1, t2, t3, t4);
    
}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


// this function was used for diag reasons only.   
float ds1820_one_device_get_temp(void){
    int ii;
    uint8_t sp[9]; //scratchpad
    int16_t ds1820_temperature = 0;

    ds1820_reset();
    ds1820_write_byte(SKIP_ROM); 
    ds1820_write_byte(READ_SCRATCHPAD);
    for (ii = 0; ii < 9; ii++){
        sp[ii] = ds1820_read_byte();
    }
    ds1820_reset();
    ds1820_temperature = sp[1] & 0x0f;
    ds1820_temperature <<= 8;
    ds1820_temperature |= sp[0];
    unsigned char remain = sp[6];
    ds1820_temperature >>= 1;
    ds1820_temperature = (ds1820_temperature * 100) -  25  + (100 * 16 - remain * 100) / (16);

    return ((float)ds1820_temperature/100);
    
}
