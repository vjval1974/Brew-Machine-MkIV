################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../CM3/core_cm3.c \
../CM3/startup_stm32f10x_hd.c \
../CM3/stm32f10x_it.c \
../CM3/system_stm32f10x.c 

OBJS += \
./CM3/core_cm3.o \
./CM3/startup_stm32f10x_hd.o \
./CM3/stm32f10x_it.o \
./CM3/system_stm32f10x.o 

C_DEPS += \
./CM3/core_cm3.d \
./CM3/startup_stm32f10x_hd.d \
./CM3/stm32f10x_it.d \
./CM3/system_stm32f10x.d 


# Each subdirectory must supply rules for building sources it contributes
CM3/%.o: ../CM3/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER -DVECT_TAB_FLASH -DGCC_ARMCM3 -Dinline= -DPACK_STRUCT_END=__attribute\(\(packed\)\) -DALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\) -I"/home/ubuntu/workspace/stm32_freertos_example/drivers" -I"/home/ubuntu/workspace/stm32_freertos_example/CM3" -I"/home/ubuntu/workspace/stm32_freertos_example" -I"/home/ubuntu/workspace/stm32_freertos_example/std_periph_drivers/inc" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS/Source/include" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS" -O1 -g -c -fmessage-length=0 -std=gnu99 -mthumb -mcpu=cortex-m3 -ffunction-sections  -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


