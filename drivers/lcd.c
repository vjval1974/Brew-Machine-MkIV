#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "lcd.h"
#include "semphr.h"
#include "stm32f10x.h"


// Doco for our LCD driver is here:
// http://www.displayfuture.com/engineering/specs/controller/LGDP4532.pdf


#ifdef __CC_ARM                			 /* ARM Compiler 	*/
#define lcd_inline   				static __inline
#elif defined (__ICCARM__)        		/* for IAR Compiler */
#define lcd_inline 					inline
#elif defined (__GNUC__)        		/* GNU GCC Compiler */
#define lcd_inline 					static __inline
#else
#define lcd_inline                  static
#endif

#define rw_data_prepare()               write_cmd(34)

// we use this sempaphore to ensure multiple threads do not try to use the LCD at the same time
xSemaphoreHandle xLcdSemaphore;

//
// Note that we call init BEFORE the main OS scheduler is running so we
// cannot use the os delay routines.
//
void Delay(unsigned i)
{
    unsigned n;
    for(;i;i--)
    {
        // was 3100
	for(n=0;n<3100;n++)
	{
	    asm("nop");
	}
    }
}

/* LCD is connected to the FSMC_Bank1_NOR/SRAM2 
   and NE2 is used as ship select signal */
/* RS <==> A2 */
#define LCD_REG (*((volatile unsigned short *) 0x60000000)) /* RS = 0 */
#define LCD_RAM (*((volatile unsigned short *) 0x60020000)) /* RS = 1 */
//---------------------------------------------------------------------/
//               Static Functions and prototypes
//---------------------------------------------------------------------/
static void display_ON(void);
static void display_OFF(void);
static void gamma_SET(void);

static void lcd_DrawCircleFill(unsigned char Xpos, 
                               unsigned int Ypos, 
                               unsigned int Radius, 
                               unsigned int Col);
static void lcd_DrawCircle(unsigned char Xpos, 
                           unsigned int Ypos, 
                           unsigned int Radius);
static void lcd_data_bus_test(void);
static void lcd_gram_test(void);
static void lcd_port_init(void);
static void power_SET(void);
static unsigned short deviceid=0;
void lcd_clear_text(uint16_t col, uint16_t row, uint16_t width);
lcd_inline void write_cmd(unsigned short cmd)
{
    LCD_REG = cmd;
}

lcd_inline unsigned short read_data(void)
{
    return LCD_RAM;
}

lcd_inline void write_data(unsigned short data_code )
{
    LCD_RAM = data_code;
}

lcd_inline void write_reg(unsigned char reg_addr,unsigned short reg_val)
{
    write_cmd(reg_addr);
    write_data(reg_val);
}

lcd_inline unsigned short read_reg(unsigned char reg_addr)
{
    unsigned short val=0;
    write_cmd(reg_addr);
    val = read_data();
    return (val);
}

unsigned int lcd_getdeviceid(void)
{
    return deviceid;
}

unsigned short BGR2RGB(unsigned short c)
{
    u16  r, g, b, rgb;

    b = (c>>0)  & 0x1f;
    g = (c>>5)  & 0x3f;
    r = (c>>11) & 0x1f;

    rgb =  (b<<11) + (g<<5) + (r<<0);

    return( rgb );
}

void lcd_SetCursor(unsigned int x,unsigned int y)
{
    write_reg(32,x);    /* 0-239 */
    write_reg(33,y);    /* 0-319 */
}

unsigned short lcd_read_gram(unsigned int x,unsigned int y)
{
    unsigned short temp;
    lcd_SetCursor(x,y);
    rw_data_prepare();
    /* dummy read */
    temp = read_data();
    temp = read_data();
    return temp;
}

//---------------------------------------------------------------------/
//                       INIT
//---------------------------------------------------------------------/
void lcd_init(void)
{
    xLcdSemaphore = xSemaphoreCreateMutex();

    lcd_port_init(); //initialise IO Registers

    /* deviceid check */    
    deviceid = read_reg(0x00); 
    if(deviceid != 0x4532)
    {
        printf("Invalid LCD ID:%08X\r\n\0",deviceid);
        printf("Please check you hardware and configure.");
        while(1);
    }
    else
    {
       // printf("\r\n\0LCD Device ID : %04X ",deviceid);
    }
    
    //SET UP//
    power_SET(); //Set up the power Registers
    gamma_SET(); //Set up the Gamma Registers

#if 1
    //TEST//
    display_OFF(); //Switch off the display for tests
    lcd_data_bus_test();
    lcd_gram_test();
    display_ON();  //Switch on the display
#endif

#if 1
    write_reg(0x0003,0x1018);//Entry Mode BGR, Horizontal, then vertical

    //Eye Candy//

    Delay(100);      
    //lcd_clear( 0xA0FF );
    //Delay(100);
   // lcd_clear( Black );
   // Delay(100);
   // lcd_clear( 0xA0FF );
   // Delay(100);
    lcd_clear( Red );

    Delay(10);

#endif
    //printf("Lcd_init done\r\n\0");
}

//---------------------------------------------------------------------/
//                       FMSC Setup
//---------------------------------------------------------------------/
static void LCD_FSMCConfig(void)
{
    FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
    FSMC_NORSRAMTimingInitTypeDef  p;

    /*-- FSMC Configuration ----------------------------------------------*/
    p.FSMC_AddressSetupTime = 10; //was 2            /* ��ַ����ʱ��  */
    p.FSMC_AddressHoldTime = 0; //was 1             /* ��ַ����ʱ��  */
    p.FSMC_DataSetupTime = 10; //was 3               /* ��ݽ���ʱ��  */
    p.FSMC_BusTurnAroundDuration = 0;        /* ���߷�תʱ��  */
    p.FSMC_CLKDivision = 0;                  /* ʱ�ӷ�Ƶ      */
    p.FSMC_DataLatency = 0; //was 0                  /* ��ݱ���ʱ��  */
    p.FSMC_AccessMode = FSMC_AccessMode_A;   /* FSMC ����ģʽ */

    /* Color LCD configuration ------------------------------------
       LCD configured as follow:
       - Data/Address MUX = Disable
       - Memory Type = SRAM
       - Data Width = 16bit
       - Write Operation = Enable
       - Extended Mode = Enable
       - Asynchronous Wait = Disable */
    FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
    FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
    FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
    FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
    FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

    FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
}

//---------------------------------------------------------------------/
//                       Hardware (I/O) Setup
//---------------------------------------------------------------------/
static void lcd_port_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
        
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE |
			   RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG |
			   RCC_APB2Periph_AFIO, ENABLE);



    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /*
      FSMC_D0 ~ FSMC_D3
      PD14 FSMC_D0   PD15 FSMC_D1   PD0  FSMC_D2   PD1  FSMC_D3

      FSMC_A16 PD11 - RS
    */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 |
        GPIO_Pin_1 | 
        GPIO_Pin_14 | 
        GPIO_Pin_15 | 
        GPIO_Pin_11;
    GPIO_Init(GPIOD,&GPIO_InitStructure);

    /*
      FSMC_D4 ~ FSMC_D12
      PE7 ~ PE15  FSMC_D4 ~ FSMC_D12
    */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 |
        GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10| 
        GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | 
        GPIO_Pin_14 | GPIO_Pin_15;

    GPIO_Init(GPIOE,&GPIO_InitStructure);

    /* FSMC_D13 ~ FSMC_D15   PD8 ~ PD10 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOD,&GPIO_InitStructure);


    /* RD-PD4 WR-PD5 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOD,&GPIO_InitStructure);
    
    /* NE1/NCE2 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOD,&GPIO_InitStructure);

    GPIO_ResetBits(GPIOE, GPIO_Pin_1);
    Delay(10/portTICK_RATE_MS);					   
    GPIO_SetBits(GPIOE, GPIO_Pin_1 );		 //	 
    Delay(10/portTICK_RATE_MS);					   

    LCD_FSMCConfig();
}

//---------------------------------------------------------------------/
//                       REGISTER SET UP
//---------------------------------------------------------------------/
static void power_SET(void)
{
    //Toggle Reset Pin
    GPIO_ResetBits(GPIOE, GPIO_Pin_1);
    Delay(10/portTICK_RATE_MS);
    GPIO_SetBits(GPIOE, GPIO_Pin_1 );		 //	 
    Delay(10/portTICK_RATE_MS);    
    
   
    write_reg(0x0000,0x0001); //Start Oscillation
    Delay(10);

    write_reg(0x0015,0x0030);// internal voltage reg at 0.65*Vci
    write_reg(0x0011,0x0040);//Power Control Setup Reg 1 
    //Step-Up Circuit 1,2 = Fosc/128,
    // VciOut - 1 * Vci
    write_reg(0x0010,0x1628);//Power Control Setup Reg 1
    //DDVDH activates separately from VGH : Halt step-up circuit 1 (VLOUT1). (Default)
    //Grayscale voltage generating circuit = 0.5
    //
    write_reg(0x0012,0x0000);//Power Control Setup Reg 3

    write_reg(0x0013,0x104d);//Power Control Setup Reg 4
    // VREG1OUT x 0.420


    Delay(10);
    write_reg(0x0012,0x0010);//VREGout = 1.47
    Delay(10);
    write_reg(0x0010,0x2620);//Power Control Setup Reg1
    //DDVDH activates at the same timing as VGH.
    //Grayscale voltage generating circuit = 0.5
    //Constant current (ratio to 3) : 0.8

    write_reg(0x0013,0x344d); //304d
    //VREG1OUT x 0.625
    //VREG1OUT x 0.69

    Delay(10);
    
    write_reg(0x0001,0x0100);//Driver Output Control
    //the source pins output from S720 to S1

    write_reg(0x0002,0x0300);//Driving Range Control
    // Line inversion waveform is selected
    // alternation occured by applying EOR(Exclusive OR) operatin to an odd/even
    // frame selecting signal and n-raster-row inversion signal while a C-pattenr waveform is generated(BC0=1).

    write_reg(0x0003,0x1008);//Entry Mode BGR, Horizontal, then vertical
    //16-bit RAM data is transferred in one transfer
    // 16-bit interface MSB mode(2 transfers/pixel) – 262k colors available
    // Reverses the order of RGB dots to BGR when writing 18-bit pixel data to the internal GRAM
    // horizontal decrement, vertical decrement.

    write_reg(0x0008,0x0604);//Display Control, first 4 and last 6
    // Number of lines for the front/back porches 6 lines front, 4 lines back

    write_reg(0x0009,0x0000);//Display Control
    // interval of scan =  disabled
    // scan mode = normal
    //
    write_reg(0x000A,0x0008);//Output FMARK every 1 Frame
    // When FMARKOE=1, the LGDP4532 starts outputting FMARK signal from the FMARK
    // pin in the output interval set with the FMI[2:0] bits
    // interval = 1 frame

    write_reg(0x000C,0x1003);
    // RAM data write cycle = disabled
    // Interface for RAM access = system/VSYNC interface
    // RGB interface mode = disabled
    
    write_reg(0x0041,0x0002);
    // VcomH level from register R13h

    write_reg(0x0060,0x2700);
    // Sets the number of lines to drive the LCD at an interval of 8lines = 320


    write_reg(0x0061,0x0001);
    // The grayscale level corresponding to the GRAM data can be reversed by setting REV = 1
    // amount of scrolling the base image by the number of lines = 0

    write_reg(0x0090,0x0182);
    // clocks per line = 130
    // Division ratio of the internal clock = 2

    write_reg(0x0093,0x0001);
    // source output timing by the number of internal clock from a reference point = 4
    // Source equalization period = 2

    write_reg(0x00a3,0x0010);
    // Write pulse width test... no more info

    Delay(10);
}

static void gamma_SET(void){
    Delay(10);
    write_reg(0x30,0x0000);		
    write_reg(0x31,0x0502);		
    write_reg(0x32,0x0307);		
    write_reg(0x33,0x0305);		
    write_reg(0x34,0x0004);		
    write_reg(0x35,0x0402);		
    write_reg(0x36,0x0707);		
    write_reg(0x37,0x0503);		
    write_reg(0x38,0x1505);		
    write_reg(0x39,0x1505);
    Delay(10);
}
static void display_ON(void)
{   
    Delay(10);
    write_reg(0x0007,0x0001);
    Delay(10);
    write_reg(0x0007,0x0021);
    write_reg(0x0007,0x0023);
    Delay(10);
    write_reg(0x0007,0x0033);
    Delay(10);
    write_reg(0x0007,0x0133);
}

static void display_OFF(void)
{
    Delay(10);
    write_reg(0x0007,0x0001);
    Delay(10);
}

static void lcd_data_bus_test(void)
{
    unsigned short temp1;
    unsigned short temp2;
    printf("bus test\r\n\0");

    /* [5:4]-ID~ID0 [3]-AM-1��ֱ-0ˮƽ */
    write_reg(0x0003,(1<<12)|(1<<5)|(1<<4) | (0<<3) );

    /* Write Alternating Bit Values */
    lcd_SetCursor(0,0);
    rw_data_prepare();
    write_data(0x5555);
    write_data(0xAAAA);
  

    /* Read it back and check*/
    lcd_SetCursor(0,0);
    temp1 = lcd_read_gram(0,0);
    temp2 = lcd_read_gram(1,0);
    if( (temp1 == 0x5555) && (temp2 == 0xAAAA) )
    {
        printf("Data bus test pass!\r\n\0");
    }
    else
    {
        printf("Data bus test error: %04X %04X\r\n\0",temp1,temp2);
    }
    fflush(stdout);
}

static void lcd_gram_test(void)
{
    unsigned short temp; //Temp value to put in GRAM
    unsigned int test_x;
    unsigned int test_y;
static int i = 1;
    printf("LCD GRAM test....\r\n\0");

    /* write */
    temp=0;
    
    write_reg(0x0003,(1<<12)|(1<<5)|(1<<4) | (0<<3) );
    lcd_SetCursor(0,0);
    rw_data_prepare();
    for(test_y=0; test_y<76800; test_y++) //test 320*240 Memory locations
    {
        write_data(temp); //put temp in GRAM
        temp++;
    }

    /* Read it back from GRAM and Test */
    temp=0;
    for(test_y=0; test_y<320; test_y++)
    {
	for(test_x=0; test_x<240; test_x++)
	{
	    if(  lcd_read_gram(test_x,test_y) != temp++)
	    {
		//if (i == 1) // print once
		  {
	            printf("LCD GRAM ERR %d, %d!!\r\n\0", test_x, test_y);
	            i = 0;
		  }
		// while(1);
	    }
	}
    }
    printf("TEST PASS!\r\n\0");
}

/*******************************************************************************
 * Function Name  : LCD_SetDisplayWindow
 * Description    : Sets a display window
 * Input          : - Xpos: specifies the X buttom left position.
 *                  - Ypos: specifies the Y buttom left position.
 *                  - Height: display window height.
 *                  - Width: display window width.
 * Output         : None
 * Return         : None
 *******************************************************************************/
void LCD_SetDisplayWindow(uint8_t Xpos, uint16_t Ypos, uint8_t Height, uint16_t Width)
{
    /* Horizontal GRAM Start Address */
    if(Xpos >= Height)
    {
	write_reg(0x0080, (Xpos - Height + 1));
	write_reg(0x0080, (Xpos));
    }
    else
    {
	//printf("outside region\r\n\0");
	write_reg(0x0080, 0);
    }
    /* Horizontal GRAM End Address */
    write_reg(0x0082, Ypos);
    /* Vertical GRAM Start Address */
    if(Ypos >= Width)
    {
	write_reg(0x0081, (Ypos));
    }
    else
    {
	//printf("outside region\r\n\0");
	write_reg(0x0081, 0);
    }
    /* Vertical GRAM End Address */
    write_reg(0x0082, Ypos+Height);

    lcd_SetCursor(Xpos, Ypos);
}

#define MAX_X 319
#define MAX_Y 239

static unsigned char const AsciiLib[95][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*" ",0*/
    {0x00,0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},/*"!",1*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*""",2*/
    {0x00,0x00,0x00,0x36,0x36,0x7F,0x36,0x36,0x36,0x7F,0x36,0x36,0x00,0x00,0x00,0x00},/*"#",3*/
    {0x00,0x18,0x18,0x3C,0x66,0x60,0x30,0x18,0x0C,0x06,0x66,0x3C,0x18,0x18,0x00,0x00},/*"$",4*/
    {0x00,0x00,0x70,0xD8,0xDA,0x76,0x0C,0x18,0x30,0x6E,0x5B,0x1B,0x0E,0x00,0x00,0x00},/*"%",5*/
    {0x00,0x00,0x00,0x38,0x6C,0x6C,0x38,0x60,0x6F,0x66,0x66,0x3B,0x00,0x00,0x00,0x00},/*"&",6*/
    {0x00,0x00,0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"'",7*/
    {0x00,0x00,0x00,0x0C,0x18,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x18,0x0C,0x00,0x00},/*"(",8*/
    {0x00,0x00,0x00,0x30,0x18,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x18,0x30,0x00,0x00},/*")",9*/
    {0x00,0x00,0x00,0x00,0x00,0x36,0x1C,0x7F,0x1C,0x36,0x00,0x00,0x00,0x00,0x00,0x00},/*"*",10*/
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},/*"+",11*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x0C,0x18,0x00,0x00},/*",",12*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"-",13*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x00},/*".",14*/
    {0x00,0x00,0x00,0x06,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0x00,0x00,0x00},/*"/",15*/
    {0x00,0x00,0x00,0x1E,0x33,0x37,0x37,0x33,0x3B,0x3B,0x33,0x1E,0x00,0x00,0x00,0x00},/*"0",16*/
    {0x00,0x00,0x00,0x0C,0x1C,0x7C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00,0x00,0x00,0x00},/*"1",17*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00,0x00,0x00,0x00},/*"2",18*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x06,0x1C,0x06,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"3",19*/
    {0x00,0x00,0x00,0x30,0x30,0x36,0x36,0x36,0x66,0x7F,0x06,0x06,0x00,0x00,0x00,0x00},/*"4",20*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},/*"5",21*/
    {0x00,0x00,0x00,0x1C,0x18,0x30,0x7C,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"6",22*/
    {0x00,0x00,0x00,0x7E,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x30,0x00,0x00,0x00,0x00},/*"7",23*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x76,0x3C,0x6E,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"8",24*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3E,0x0C,0x18,0x38,0x00,0x00,0x00,0x00},/*"9",25*/
    {0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x00},/*":",26*/
    {0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x1C,0x1C,0x0C,0x18,0x00,0x00},/*";",27*/
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},/*"<",28*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"=",29*/
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},/*">",30*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x0C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},/*"?",31*/
    {0x00,0x00,0x00,0x7E,0xC3,0xC3,0xCF,0xDB,0xDB,0xCF,0xC0,0x7F,0x00,0x00,0x00,0x00},/*"@",32*/
    {0x00,0x00,0x00,0x18,0x3C,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"A",33*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},/*"B",34*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x60,0x60,0x60,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"C",35*/
    {0x00,0x00,0x00,0x78,0x6C,0x66,0x66,0x66,0x66,0x66,0x6C,0x78,0x00,0x00,0x00,0x00},/*"D",36*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"E",37*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"F",38*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x60,0x60,0x6E,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"G",39*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"H",40*/
    {0x00,0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},/*"I",41*/
    {0x00,0x00,0x00,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"J",42*/
    {0x00,0x00,0x00,0x66,0x66,0x6C,0x6C,0x78,0x6C,0x6C,0x66,0x66,0x00,0x00,0x00,0x00},/*"K",43*/
    {0x00,0x00,0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"L",44*/
    {0x00,0x00,0x00,0x63,0x63,0x77,0x6B,0x6B,0x6B,0x63,0x63,0x63,0x00,0x00,0x00,0x00},/*"M",45*/
    {0x00,0x00,0x00,0x63,0x63,0x73,0x7B,0x6F,0x67,0x63,0x63,0x63,0x00,0x00,0x00,0x00},/*"N",46*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"O",47*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"P",48*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x0C,0x06,0x00,0x00},/*"Q",49*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"R",50*/
    {0x00,0x00,0x00,0x3C,0x66,0x60,0x30,0x18,0x0C,0x06,0x66,0x3C,0x00,0x00,0x00,0x00},/*"S",51*/
    {0x00,0x00,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},/*"T",52*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"U",53*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},/*"V",54*/
    {0x00,0x00,0x00,0x63,0x63,0x63,0x6B,0x6B,0x6B,0x36,0x36,0x36,0x00,0x00,0x00,0x00},/*"W",55*/
    {0x00,0x00,0x00,0x66,0x66,0x34,0x18,0x18,0x2C,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"X",56*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},/*"Y",57*/
    {0x00,0x00,0x00,0x7E,0x06,0x06,0x0C,0x18,0x30,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"Z",58*/
    {0x00,0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},/*"[",59*/
    {0x00,0x00,0x00,0x60,0x60,0x30,0x30,0x18,0x18,0x0C,0x0C,0x06,0x06,0x00,0x00,0x00},/*"\",60*/
    {0x00,0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},/*"]",61*/
    {0x00,0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"^",62*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},/*"_",63*/
    {0x00,0x00,0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"'",64*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x06,0x3E,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"a",65*/
    {0x00,0x00,0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},/*"b",66*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00,0x00,0x00,0x00},/*"c",67*/
    {0x00,0x00,0x00,0x06,0x06,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"d",68*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x60,0x3C,0x00,0x00,0x00,0x00},/*"e",69*/
    {0x00,0x00,0x00,0x1E,0x30,0x30,0x30,0x7E,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},/*"f",70*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x7C,0x00},/*"g",71*/
    {0x00,0x00,0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"h",72*/
    {0x00,0x00,0x18,0x18,0x00,0x78,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},/*"i",73*/
    {0x00,0x00,0x0C,0x0C,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x78,0x00},/*"j",74*/
    {0x00,0x00,0x00,0x60,0x60,0x66,0x66,0x6C,0x78,0x6C,0x66,0x66,0x00,0x00,0x00,0x00},/*"k",75*/
    {0x00,0x00,0x00,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},/*"l",76*/
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x6B,0x6B,0x6B,0x6B,0x6B,0x63,0x00,0x00,0x00,0x00},/*"m",77*/
    {0x00,0x00,0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"n",78*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"o",79*/
    {0x00,0x00,0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},/*"p",80*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x06,0x00},/*"q",81*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x6E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"r",82*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x60,0x60,0x3C,0x06,0x06,0x7C,0x00,0x00,0x00,0x00},/*"s",83*/
    {0x00,0x00,0x00,0x30,0x30,0x7E,0x30,0x30,0x30,0x30,0x30,0x1E,0x00,0x00,0x00,0x00},/*"t",84*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"u",85*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},/*"v",86*/
    {0x00,0x00,0x00,0x00,0x00,0x63,0x6B,0x6B,0x6B,0x6B,0x36,0x36,0x00,0x00,0x00,0x00},/*"w",87*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x00,0x00},/*"x",88*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x0C,0x18,0xF0,0x00},/*"y",89*/
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00,0x00,0x00,0x00},/*"z",90*/
    {0x00,0x00,0x00,0x0C,0x18,0x18,0x18,0x30,0x60,0x30,0x18,0x18,0x18,0x0C,0x00,0x00},/*"{",91*/
    {0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},/*"|",92*/
    {0x00,0x00,0x00,0x30,0x18,0x18,0x18,0x0C,0x06,0x0C,0x18,0x18,0x18,0x30,0x00,0x00},/*"}",93*/
    {0x00,0x00,0x00,0x71,0xDB,0x8E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"~",94*/
};

static __inline void lcd_write_ram_prepare(void)
{
    write_cmd(0x22);
}

static void lcd_set_cursor(uint16_t Xpos,uint16_t Ypos)
{
    write_reg(32, Ypos); /* Row */
    write_reg(33, MAX_X - Xpos); /* Line */ 
}

static void lcd_char_xy(unsigned short Xpos,unsigned short Ypos,unsigned char c,unsigned short charColor,unsigned short bkColor)
{
    unsigned short i=0;
    unsigned short j=0;
    const unsigned char *buffer = AsciiLib[(c - 32)] ;
    unsigned char tmp_char=0;
    for (i=0;i<16;i++)
    {

	tmp_char=buffer[i];
	for (j=0;j<8;j++)
	{
	    if ( (tmp_char >> 7-j) & 0x01)
	    {
		lcd_set_cursor(Xpos + j,Ypos+i);
		lcd_write_ram_prepare();
		write_data(charColor);
		//lcd_write_ram_prepare(); //added to test.
	    }
		

	   // uint16_t col = ( (tmp_char >> 7-j) & 0x01) ? charColor : bkColor;
	   // write_data(col);
	}
    }
}

static void lcd_DrawHLine(int x1, int x2, int col, int y ) 
{
    uint16_t ptr;
    //Set up registers for horizontal scanning
    write_reg(0x0003,(1<<12)|(1<<5)|(1<<4) | (0<<3) );
    
    lcd_SetCursor(x1, y); //start position
    rw_data_prepare(); /* Prepare to write GRAM */
    while (x1 <= x2)
    {
        write_data(col);
        x1 ++;
        
    }
}

static void lcd_DrawVLine(int y1, int y2, int col, int x)
{
    unsigned short p;
    
    //Set up registers for vertical scanning 
    write_reg(0x0003,(1<<12)|(1<<5)|(0<<4) | (1<<3) );
    
    lcd_SetCursor(x, y1); //start position
    rw_data_prepare(); /* Prepare to write GRAM */
    while (y1 <= y2)
    {
        write_data(col);
        y1++;
    }
}

static uint16_t bg_col;

//////////////////////////////////////////////////////////////////////////////////////////////////
// LOCKING code
//////////////////////////////////////////////////////////////////////////////////////////////////
static volatile xTaskHandle lcdUsingTask = NULL;

void lcd_lock()
{
    while( xSemaphoreTake( xLcdSemaphore, ( portTickType ) 100 ) != pdTRUE )
    {
	printf("Waiting a long time for LCD\r\n\0");
    }
    lcdUsingTask = xTaskGetCurrentTaskHandle();
}

void lcd_release()
{
    xSemaphoreGive(xLcdSemaphore);	
    lcdUsingTask = NULL;
}

#define LCD_LOCK char auto_lock = 0;if (lcdUsingTask != xTaskGetCurrentTaskHandle()){ lcd_lock(); auto_lock = 1; }
#define LCD_UNLOCK if (auto_lock) lcd_release()


/////////////////////////////////////////////////////////////////////////////////////////////////
// THREADSAFE INTERFACE - uses semaphores to unsure only one thread is drawing at a time
/////////////////////////////////////////////////////////////////////////////////////////////////

void lcd_text_xy(uint16_t Xpos, uint16_t Ypos, const char *str,uint16_t Color, uint16_t bkColor)
{
    LCD_LOCK;
    uint8_t TempChar;
	
//	printf("lcd text %d,%d %s\r\n\0", Xpos, Ypos, str);
	
    while ((TempChar=*str++))
    {
	lcd_char_xy(Xpos,Ypos,TempChar,Color,bkColor);    
	if (Xpos < MAX_X - 8)
	{
	    Xpos+=8;
	} 
	else if (Ypos < MAX_Y - 16)
	{
	    Xpos=0;
	    Ypos+=16;
	}   
	else
	{
	    Xpos=0;
	    Ypos=0;
	}    
    }
    LCD_UNLOCK;
}

void lcd_text(uint8_t col, uint8_t row, const char *text)
{
    lcd_text_xy(col * 8, row * 16, text, 0xFFFF, bg_col);
}

void lcd_fill(uint16_t xx, uint16_t yy, uint16_t ww, uint16_t hh, uint16_t color)
{
    LCD_LOCK;
    for (int ii = 0; ii < hh; ii++)
    {	  
	lcd_set_cursor(xx, yy + ii);
	lcd_write_ram_prepare();
	for (int jj = 0; jj < ww; jj++)
	{
	    write_data(color);
	}
    }	
    LCD_UNLOCK;
}

#include <stdarg.h>
void lcd_printf(uint8_t col, uint8_t row, uint8_t ww, const char *fmt, ...)
{

    LCD_LOCK;

    char message[31];




    //get message from format
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(message, sizeof(message) - 1, fmt, ap);
    va_end(ap);

    lcd_clear_text(col, row, ww);
//    //clear message space
//    for (i = 0; i < len; i++)
//       	message[i] = ' ';
//
//       lcd_text(col, row, message);
//    // clear until end of line
//    while (len < ww && len < sizeof(message) - 2)
//    {
//    	message[len++] = '';
//    }
//    message[len] = 0;

    // print message
    lcd_text(col, row, message);

    LCD_UNLOCK;

}

void lcd_clear_text(uint16_t col, uint16_t row, uint16_t width)
{
	 lcd_fill(col * 8, row * 16, width * 8, 16, bg_col);
}

void lcd_clear(uint16_t Color)
{
    LCD_LOCK;	
    uint32_t index=0;
    lcd_set_cursor(0,0); 
    lcd_write_ram_prepare(); /* Prepare to write GRAM */
    for(index = 0; index < LCD_W * LCD_H; index++)
    {
	write_data(Color);
    }
    LCD_UNLOCK;
}

uint16_t lcd_background(uint16_t color)
{
	uint16_t old = bg_col;
    bg_col = color;
    return old;
}

void lcd_DrawRect(int x1, int y1, int x2, int y2, int col)
{
    LCD_LOCK;
    lcd_fill(x1, y1, x2-x1+2, y2-y1+2, col);
    lcd_fill(x1+1, y1+1, x2-x1, y2-y1, Black);
    LCD_UNLOCK;
}

void display_on()
{
    write_reg(0x000C,0x1003);
}

void display_off()
{
    write_reg(0x000F,0x0010);
    
    write_reg(0x000C,0x1023);
}
