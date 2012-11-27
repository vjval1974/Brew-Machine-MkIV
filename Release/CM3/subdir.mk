################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../CM3/core_cm3.c \
../CM3/stm32f10x_it.c \
../CM3/system_stm32f10x.c 

OBJS += \
./CM3/core_cm3.o \
./CM3/stm32f10x_it.o \
./CM3/system_stm32f10x.o 

C_DEPS += \
./CM3/core_cm3.d \
./CM3/stm32f10x_it.d \
./CM3/system_stm32f10x.d 


# Each subdirectory must supply rules for building sources it contributes
CM3/%.o: ../CM3/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


