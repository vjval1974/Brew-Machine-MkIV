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
../drivers/timertest.c \
../drivers/touch.c 

OBJS += \
./drivers/adc.o \
./drivers/ds1820.o \
./drivers/lcd.o \
./drivers/leds.o \
./drivers/serial.o \
./drivers/speaker.o \
./drivers/timer.o \
./drivers/timertest.o \
./drivers/touch.o 

C_DEPS += \
./drivers/adc.d \
./drivers/ds1820.d \
./drivers/lcd.d \
./drivers/leds.d \
./drivers/serial.d \
./drivers/speaker.d \
./drivers/timer.d \
./drivers/timertest.d \
./drivers/touch.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


