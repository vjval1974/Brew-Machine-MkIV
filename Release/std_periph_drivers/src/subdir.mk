################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../std_periph_drivers/src/misc.c \
../std_periph_drivers/src/stm32_eth.c \
../std_periph_drivers/src/stm32f10x_adc.c \
../std_periph_drivers/src/stm32f10x_bkp.c \
../std_periph_drivers/src/stm32f10x_can.c \
../std_periph_drivers/src/stm32f10x_cec.c \
../std_periph_drivers/src/stm32f10x_crc.c \
../std_periph_drivers/src/stm32f10x_dac.c \
../std_periph_drivers/src/stm32f10x_dbgmcu.c \
../std_periph_drivers/src/stm32f10x_dma.c \
../std_periph_drivers/src/stm32f10x_exti.c \
../std_periph_drivers/src/stm32f10x_flash.c \
../std_periph_drivers/src/stm32f10x_fsmc.c \
../std_periph_drivers/src/stm32f10x_gpio.c \
../std_periph_drivers/src/stm32f10x_i2c.c \
../std_periph_drivers/src/stm32f10x_iwdg.c \
../std_periph_drivers/src/stm32f10x_pwr.c \
../std_periph_drivers/src/stm32f10x_rcc.c \
../std_periph_drivers/src/stm32f10x_rtc.c \
../std_periph_drivers/src/stm32f10x_sdio.c \
../std_periph_drivers/src/stm32f10x_spi.c \
../std_periph_drivers/src/stm32f10x_tim.c \
../std_periph_drivers/src/stm32f10x_usart.c \
../std_periph_drivers/src/stm32f10x_wwdg.c 

OBJS += \
./std_periph_drivers/src/misc.o \
./std_periph_drivers/src/stm32_eth.o \
./std_periph_drivers/src/stm32f10x_adc.o \
./std_periph_drivers/src/stm32f10x_bkp.o \
./std_periph_drivers/src/stm32f10x_can.o \
./std_periph_drivers/src/stm32f10x_cec.o \
./std_periph_drivers/src/stm32f10x_crc.o \
./std_periph_drivers/src/stm32f10x_dac.o \
./std_periph_drivers/src/stm32f10x_dbgmcu.o \
./std_periph_drivers/src/stm32f10x_dma.o \
./std_periph_drivers/src/stm32f10x_exti.o \
./std_periph_drivers/src/stm32f10x_flash.o \
./std_periph_drivers/src/stm32f10x_fsmc.o \
./std_periph_drivers/src/stm32f10x_gpio.o \
./std_periph_drivers/src/stm32f10x_i2c.o \
./std_periph_drivers/src/stm32f10x_iwdg.o \
./std_periph_drivers/src/stm32f10x_pwr.o \
./std_periph_drivers/src/stm32f10x_rcc.o \
./std_periph_drivers/src/stm32f10x_rtc.o \
./std_periph_drivers/src/stm32f10x_sdio.o \
./std_periph_drivers/src/stm32f10x_spi.o \
./std_periph_drivers/src/stm32f10x_tim.o \
./std_periph_drivers/src/stm32f10x_usart.o \
./std_periph_drivers/src/stm32f10x_wwdg.o 

C_DEPS += \
./std_periph_drivers/src/misc.d \
./std_periph_drivers/src/stm32_eth.d \
./std_periph_drivers/src/stm32f10x_adc.d \
./std_periph_drivers/src/stm32f10x_bkp.d \
./std_periph_drivers/src/stm32f10x_can.d \
./std_periph_drivers/src/stm32f10x_cec.d \
./std_periph_drivers/src/stm32f10x_crc.d \
./std_periph_drivers/src/stm32f10x_dac.d \
./std_periph_drivers/src/stm32f10x_dbgmcu.d \
./std_periph_drivers/src/stm32f10x_dma.d \
./std_periph_drivers/src/stm32f10x_exti.d \
./std_periph_drivers/src/stm32f10x_flash.d \
./std_periph_drivers/src/stm32f10x_fsmc.d \
./std_periph_drivers/src/stm32f10x_gpio.d \
./std_periph_drivers/src/stm32f10x_i2c.d \
./std_periph_drivers/src/stm32f10x_iwdg.d \
./std_periph_drivers/src/stm32f10x_pwr.d \
./std_periph_drivers/src/stm32f10x_rcc.d \
./std_periph_drivers/src/stm32f10x_rtc.d \
./std_periph_drivers/src/stm32f10x_sdio.d \
./std_periph_drivers/src/stm32f10x_spi.d \
./std_periph_drivers/src/stm32f10x_tim.d \
./std_periph_drivers/src/stm32f10x_usart.d \
./std_periph_drivers/src/stm32f10x_wwdg.d 


# Each subdirectory must supply rules for building sources it contributes
std_periph_drivers/src/%.o: ../std_periph_drivers/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


