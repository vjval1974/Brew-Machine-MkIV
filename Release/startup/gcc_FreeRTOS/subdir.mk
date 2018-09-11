################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../startup/gcc_FreeRTOS/startup_stm32f10x_hd.c 

OBJS += \
./startup/gcc_FreeRTOS/startup_stm32f10x_hd.o 

C_DEPS += \
./startup/gcc_FreeRTOS/startup_stm32f10x_hd.d 


# Each subdirectory must supply rules for building sources it contributes
startup/gcc_FreeRTOS/%.o: ../startup/gcc_FreeRTOS/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


