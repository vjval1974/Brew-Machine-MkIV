################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FreeRTOS/Source/portable/MemMang/heap_2.c 

OBJS += \
./FreeRTOS/Source/portable/MemMang/heap_2.o 

C_DEPS += \
./FreeRTOS/Source/portable/MemMang/heap_2.d 


# Each subdirectory must supply rules for building sources it contributes
FreeRTOS/Source/portable/MemMang/%.o: ../FreeRTOS/Source/portable/MemMang/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-none-eabi-gcc -DSTM32F10X_HD -DUSE_STDPERIPH_DRIVER -DVECT_TAB_FLASH -DGCC_ARMCM3 -Dinline= -DPACK_STRUCT_END=__attribute\(\(packed\)\) -DALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\) -I"/home/ubuntu/workspace/stm32_freertos_example/drivers" -I"/home/ubuntu/workspace/stm32_freertos_example/CM3" -I"/home/ubuntu/workspace/stm32_freertos_example" -I"/home/ubuntu/workspace/stm32_freertos_example/std_periph_drivers/inc" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS/Source/include" -I"/home/ubuntu/workspace/stm32_freertos_example/FreeRTOS" -O1 -g -c -fmessage-length=0 -std=gnu99 -mthumb -mcpu=cortex-m3 -ffunction-sections  -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


