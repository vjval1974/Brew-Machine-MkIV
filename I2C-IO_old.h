/*
 * I2C-IO.h
 *
 *  Created on: Jul 24, 2013
 *      Author: brad
 */

#ifndef I2C_IO_H_
#define I2C_IO_H_

void vI2C_Init(void);
void vI2C_Send(char address, char data);

extern xQueueHandle xI2C_SendQueue;
void vI2C_SendTask(void * pvParameters);
void vI2C_TestTask(void *pvParameters);
#define I2C_SLAVE_ADDRESS0 0x70
#define I2C_SLAVE_ADDRESS1 0x72
#define I2C_SLAVE_ADDRESS2 0x74
#define I2C_SLAVE_ADDRESS3 0x76
#define I2C_SLAVE_ADDRESS4 0x78
#define I2C_SLAVE_ADDRESS5 0x7A
#define I2C_SLAVE_ADDRESS6 0x7C
#define I2C_SLAVE_ADDRESS7 0x7E

#endif /* I2C_IO_H_ */
