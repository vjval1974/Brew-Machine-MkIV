################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../keg_display.c \
../main.c \
../menu.c \
../startup_stm32f10x.c \
../stf_syscalls_minimal.c 

OBJS += \
./keg_display.o \
./main.o \
./menu.o \
./startup_stm32f10x.o \
./stf_syscalls_minimal.o 

C_DEPS += \
./keg_display.d \
./main.d \
./menu.d \
./startup_stm32f10x.d \
./stf_syscalls_minimal.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


