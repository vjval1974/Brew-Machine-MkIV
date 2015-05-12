/*
 * buffer.h
 */

#ifndef BUFFER_H_
#define BUFFER_H_
#include "stm32f10x.h"
#define USARTBUFFSIZE   128
typedef struct{
        uint8_t in;
        uint8_t out;
        uint8_t count;
        uint8_t buff[USARTBUFFSIZE];
}FIFO_TypeDef;

void BufferInit(__IO FIFO_TypeDef *buffer);
ErrorStatus BufferPut(__IO FIFO_TypeDef *buffer, uint8_t ch);
ErrorStatus BufferGet(__IO FIFO_TypeDef *buffer, uint8_t *ch);
ErrorStatus BufferIsEmpty(__IO FIFO_TypeDef buffer);


#endif /* BUFFER_H_ */
