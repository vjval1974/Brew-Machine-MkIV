################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FreeRTOS/Source/portable/GCC/ARM_CM3_MPU/port.c 

OBJS += \
./FreeRTOS/Source/portable/GCC/ARM_CM3_MPU/port.o 

C_DEPS += \
./FreeRTOS/Source/portable/GCC/ARM_CM3_MPU/port.d 


# Each subdirectory must supply rules for building sources it contributes
FreeRTOS/Source/portable/GCC/ARM_CM3_MPU/%.o: ../FreeRTOS/Source/portable/GCC/ARM_CM3_MPU/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


