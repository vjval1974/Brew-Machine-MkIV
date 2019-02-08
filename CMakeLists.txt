#cmake_minimum_required(VERSION 3.13)
#project(Brew_Machine_MkIV C)
#
#
#set(-DCMAKE_TOOLCHAIN_FILE=cmake/arm-toolchain.cmake)
#set(CMAKE_C_STANDARD 99)
#set(USE_STDPERIPH_DRIVER)
#set(STM32F10X_HD)
#set(VECT_TAB_FLASH)
#set(GCC_ARMCM3)
#set(inline=)
#set(PACK_STRUCT_END=__attribute\(\(packed\)\))
#set(ALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\))


INCLUDE(CMakeForceCompiler)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)

# Specify the cross compiler. arm-none-eabi-gcc and arm-none-eabi-g++ are full path required
CMAKE_FORCE_C_COMPILER(arm-none-eabi-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(arm-none-eabi-g++ GNU)

SET(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/stm32_flash.ld)
SET(COMMON_FLAGS "-mcpu=cortex-m3 -O2 -mthumb -ffunction-sections -fdata-sections -g -fno-common -fmessage-length=0")
SET(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -std=c++11")
SET(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS} -std=gnu99")
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-gc-sections -specs=nosys.specs -specs=nano.specs -T ${LINKER_SCRIPT}")
#
#include_directories(.)
#include_directories(CM3)
#include_directories(drivers)
#include_directories(FreeRTOS/Source/include)
#include_directories(FreeRTOS/Source/portable/GCC/ARM_CM3)
#include_directories(std_periph_drivers/inc)
#
#add_executable(Brew_Machine_MkIV
#        CM3/core_cm3.c
#        CM3/core_cm3.h
#        CM3/startup_stm32f10x_hd.c
#        CM3/stm32f10x.h
#        CM3/stm32f10x_conf.h
#        CM3/stm32f10x_it.c
#        CM3/stm32f10x_it.h
#        CM3/system_stm32f10x.c
#        CM3/system_stm32f10x.h
#        drivers/adc.c
#        drivers/adc.h
#        drivers/buffer.c
#        drivers/buffer.h
#        drivers/button.c
#        drivers/button.h
#        drivers/ds1820.c
#        drivers/ds1820.h
#        drivers/lcd.c
#        drivers/lcd.h
#        drivers/leds.c
#        drivers/leds.h
#        drivers/serial.c
#        drivers/serial.h
#        drivers/speaker.c
#        drivers/speaker.h
#        drivers/timer.c
#        drivers/timer.h
#        drivers/touch.c
#        drivers/touch.h
#        FreeRTOS/Source/include/croutine.h
#        FreeRTOS/Source/include/FreeRTOS.h
#        FreeRTOS/Source/include/list.h
#        FreeRTOS/Source/include/mpu_wrappers.h
#        FreeRTOS/Source/include/portable.h
#        FreeRTOS/Source/include/projdefs.h
#        FreeRTOS/Source/include/queue.h
#        FreeRTOS/Source/include/semphr.h
#        FreeRTOS/Source/include/StackMacros.h
#        FreeRTOS/Source/include/task.h
#        FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
#        FreeRTOS/Source/portable/GCC/ARM_CM3/portmacro.h
#        FreeRTOS/Source/portable/MemMang/heap_2.c
#        FreeRTOS/Source/portable/MemMang/heap_3.c
#        FreeRTOS/Source/list.c
#        FreeRTOS/Source/queue.c
#        FreeRTOS/Source/tasks.c
#        std_periph_drivers/inc/misc.h
#        std_periph_drivers/inc/stm32_eth.h
#        std_periph_drivers/inc/stm32f10x_adc.h
#        std_periph_drivers/inc/stm32f10x_bkp.h
#        std_periph_drivers/inc/stm32f10x_can.h
#        std_periph_drivers/inc/stm32f10x_cec.h
#        std_periph_drivers/inc/stm32f10x_crc.h
#        std_periph_drivers/inc/stm32f10x_dac.h
#        std_periph_drivers/inc/stm32f10x_dbgmcu.h
#        std_periph_drivers/inc/stm32f10x_dma.h
#        std_periph_drivers/inc/stm32f10x_exti.h
#        std_periph_drivers/inc/stm32f10x_flash.h
#        std_periph_drivers/inc/stm32f10x_fsmc.h
#        std_periph_drivers/inc/stm32f10x_gpio.h
#        std_periph_drivers/inc/stm32f10x_i2c.h
#        std_periph_drivers/inc/stm32f10x_iwdg.h
#        std_periph_drivers/inc/stm32f10x_pwr.h
#        std_periph_drivers/inc/stm32f10x_rcc.h
#        std_periph_drivers/inc/stm32f10x_rtc.h
#        std_periph_drivers/inc/stm32f10x_sdio.h
#        std_periph_drivers/inc/stm32f10x_spi.h
#        std_periph_drivers/inc/stm32f10x_tim.h
#        std_periph_drivers/inc/stm32f10x_usart.h
#        std_periph_drivers/inc/stm32f10x_wwdg.h
#        std_periph_drivers/src/misc.c
#        std_periph_drivers/src/stm32f10x_adc.c
#        std_periph_drivers/src/stm32f10x_exti.c
#        std_periph_drivers/src/stm32f10x_flash.c
#        std_periph_drivers/src/stm32f10x_fsmc.c
#        std_periph_drivers/src/stm32f10x_gpio.c
#        std_periph_drivers/src/stm32f10x_i2c.c
#        std_periph_drivers/src/stm32f10x_rcc.c
#        std_periph_drivers/src/stm32f10x_spi.c
#        std_periph_drivers/src/stm32f10x_tim.c
#        std_periph_drivers/src/stm32f10x_usart.c
#        boil.c
#        boil.h
#        boil_valve.c
#        boil_valve.h
#        brew.c
#        brew.h
#        chiller_pump.c
#        chiller_pump.h
#        console.c
#        console.h
#        crane.c
#        crane.h
#        diag_pwm.c
#        diag_pwm.h
#        diag_temps.c
#        diag_temps.h
#        Flow1.c
#        Flow1.h
#        FreeRTOSConfig.h
#        hlt.c
#        hlt.h
#        hop_dropper.c
#        hop_dropper.h
#        I2C-IO.c
#        I2C-IO.h
#        io_util.c
#        io_util.h
#        macros.h
#        main.c
#        main.h
#        mash_pump.c
#        mash_pump.h
#        MashWater.c
#        MashWater.h
#        menu.c
#        menu.h
#        MenuExamples.c
#        MenuExamples.h
#        message.h
#        mill.c
#        mill.h
#        newlibstubs.c
#        parameters.c
#        parameters.h
#        printf-stdarg.c
#        stf_syscalls_minimal.c
#        stir.c
#        stir.h
#        stirApplet.c
#        stirApplet.h
#        valves.c
#        valves.h)
