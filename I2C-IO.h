/*
 * I2C-IO.h
 *
 *  Created on: Jul 24, 2013
 *      Author: brad
 */

#ifndef I2C_IO_H_
#define I2C_IO_H_
#define TRUE 1
#define FALSE 0
#define ERROR 255;

void vI2C_Init(void);
//void vI2C_Send(char address, char data);
void vPCF_SetBits(uint8_t bitnum, uint8_t add);
void vPCF_ResetBits(uint8_t bitnum, uint8_t add);
//extern xQueueHandle xI2C_SendQueue;
void vI2C_SendTask(void * pvParameters);
void vI2C_TestTask(void *pvParameters);
char cI2cGetInput(char port, char pin);
#define I2C_SLAVE_ADDRESS0 0x70
#define I2C_SLAVE_ADDRESS1 0x72
#define I2C_SLAVE_ADDRESS2 0x74
#define I2C_SLAVE_ADDRESS3 0x76
#define I2C_SLAVE_ADDRESS4 0x78
#define I2C_SLAVE_ADDRESS5 0x7A
#define I2C_SLAVE_ADDRESS6 0x7C
#define I2C_SLAVE_ADDRESS7 0x7E

#define PCF_PIN0  0
#define PCF_PIN1  1
#define PCF_PIN2  2
#define PCF_PIN3  3
#define PCF_PIN4  4
#define PCF_PIN5  5
#define PCF_PIN6  6
#define PCF_PIN7  7

#define PORTU I2C_SLAVE_ADDRESS0
#define PORTV I2C_SLAVE_ADDRESS1
#define PORTW I2C_SLAVE_ADDRESS2
#define PORTX I2C_SLAVE_ADDRESS3
#define PORTY I2C_SLAVE_ADDRESS4

#endif /* I2C_IO_H_ */
