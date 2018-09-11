################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../std_periph_drivers/src/misc.c \
../std_periph_drivers/src/stm32f10x_adc.c \
../std_periph_drivers/src/stm32f10x_flash.c \
../std_periph_drivers/src/stm32f10x_fsmc.c \
../std_periph_drivers/src/stm32f10x_gpio.c \
../std_periph_drivers/src/stm32f10x_rcc.c \
../std_periph_drivers/src/stm32f10x_spi.c \
../std_periph_drivers/src/stm32f10x_tim.c \
../std_periph_drivers/src/stm32f10x_usart.c 

OBJS += \
./std_periph_drivers/src/misc.o \
./std_periph_drivers/src/stm32f10x_adc.o \
./std_periph_drivers/src/stm32f10x_flash.o \
./std_periph_drivers/src/stm32f10x_fsmc.o \
./std_periph_drivers/src/stm32f10x_gpio.o \
./std_periph_drivers/src/stm32f10x_rcc.o \
./std_periph_drivers/src/stm32f10x_spi.o \
./std_periph_drivers/src/stm32f10x_tim.o \
./std_periph_drivers/src/stm32f10x_usart.o 

C_DEPS += \
./std_periph_drivers/src/misc.d \
./std_periph_drivers/src/stm32f10x_adc.d \
./std_periph_drivers/src/stm32f10x_flash.d \
./std_periph_drivers/src/stm32f10x_fsmc.d \
./std_periph_drivers/src/stm32f10x_gpio.d \
./std_periph_drivers/src/stm32f10x_rcc.d \
./std_periph_drivers/src/stm32f10x_spi.d \
./std_periph_drivers/src/stm32f10x_tim.d \
./std_periph_drivers/src/stm32f10x_usart.d 


# Each subdirectory must supply rules for building sources it contributes
std_periph_drivers/src/%.o: ../std_periph_drivers/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER -DVECT_TAB_FLASH -DGCC_ARMCM3 -Dinline= -DPACK_STRUCT_END=__attribute\(\(packed\)\) -DALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\) -I"/home/ubuntu/workspace/stm32_freertos_example/drivers" -I"/home/ubuntu/workspace/stm32_freertos_example/CM3" -I"/home/ubuntu/workspace/stm32_freertos_example" -I"/home/ubuntu/workspace/stm32_freertos_example/std_periph_drivers/inc" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS/Source/include" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS" -O1 -g -c -fmessage-length=0 -std=gnu99 -mthumb -mcpu=cortex-m3 -ffunction-sections  -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


