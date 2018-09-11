################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include std_periph_drivers/src/subdir.mk
-include drivers/subdir.mk
-include FreeRTOS/Source/portable/MemMang/subdir.mk
-include FreeRTOS/Source/portable/GCC/ARM_CM3/subdir.mk
-include FreeRTOS/Source/subdir.mk
-include CM3/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: stm32_freertos_example

# Tool invocations
stm32_freertos_example: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	arm-none-eabi-gcc -nostartfiles -T"/home/ubuntu/workspace/stm32_freertos_example/stm32_flash.ld" -mthumb -mcpu=cortex-m3 -ffunction-sections -fdata-sections -ggdb -Xlinker -M -Xlinker -Map=RTOSBrew.map -Xlinker --no-gc-sections -o "stm32_freertos_example" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) stm32_freertos_example
	-@echo ' '

post-build:
	-@echo 'Create binary output'
	-arm-none-eabi-objcopy stm32_freertos_example -O binary stm32_freertos_example.bin
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
