################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/adc.c \
../drivers/ds1820.c \
../drivers/lcd.c \
../drivers/leds.c \
../drivers/serial.c \
../drivers/speaker.c \
../drivers/timer.c \
../drivers/touch.c 

OBJS += \
./drivers/adc.o \
./drivers/ds1820.o \
./drivers/lcd.o \
./drivers/leds.o \
./drivers/serial.o \
./drivers/speaker.o \
./drivers/timer.o \
./drivers/touch.o 

C_DEPS += \
./drivers/adc.d \
./drivers/ds1820.d \
./drivers/lcd.d \
./drivers/leds.d \
./drivers/serial.d \
./drivers/speaker.d \
./drivers/timer.d \
./drivers/touch.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER -DVECT_TAB_FLASH -DGCC_ARMCM3 -Dinline= -DPACK_STRUCT_END=__attribute\(\(packed\)\) -DALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\) -I"/home/ubuntu/workspace/stm32_freertos_example/drivers" -I"/home/ubuntu/workspace/stm32_freertos_example/CM3" -I"/home/ubuntu/workspace/stm32_freertos_example" -I"/home/ubuntu/workspace/stm32_freertos_example/std_periph_drivers/inc" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS/Source/include" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS" -O1 -g -c -fmessage-length=0 -std=gnu99 -mthumb -mcpu=cortex-m3 -ffunction-sections  -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


