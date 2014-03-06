# Makefile for colourclock firmware
# Copyright (c) 2013-2014 Matt Evans
#
# This makefile builds two things: firmware for the MCU and a 'sim'
# binary that's an OpenGL app for testing.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

ifeq ($(V), 1)
	VERBOSE=
else
	VERBOSE=@
endif

CROSS_COMPILE ?= /usr/local/gcc-arm-embedded/bin/arm-none-eabi-
STM_LIB ?= /Users/matt/code/STM32F0/STM32F0xx_StdPeriph_Lib_V1.0.0/

################################################################################

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

CMSIS = $(STM_LIB)/Libraries/CMSIS
STMLIB = $(STM_LIB)/Libraries/STM32F0xx_StdPeriph_Driver
INCLUDES = -I$(CMSIS)/ST/STM32F0xx/Include
INCLUDES += -I$(CMSIS)/Include
INCLUDES += -I$(CMSIS)/Device/ST/STM32F0xx/Include
INCLUDES += -I$(STMLIB)/inc
INCLUDES += -I.

FW_CFLAGS = -mthumb -mcpu=cortex-m0 -O2 -std=c99
FW_CFLAGS += -g -ggdb -ffreestanding -msoft-float
FW_CFLAGS += -DUSE_STDPERIPH_DRIVER=1 $(INCLUDES)
FW_CFLAGS += -fdata-sections -ffunction-sections
FW_LIBS = -lgcc
# Note:  The -march/-mthumb items are must-haves, to select v6m crtX.o and
# libgcc.a!
FW_LINKFLAGS = -Wl,--gc-sections -march=armv6-m -mthumb --specs=nano.specs
FW_LINKFLAGS += -Wl,-T stm32f051x6.ld

################################################################################

SIM_CFLAGS=-O3

MACHINE=$(shell uname -s)

ifeq ($(MACHINE),Darwin)
    OPENGL_INC= -FOpenGL
    OPENGL_LIB= -framework OpenGL
    SDL_INC= `sdl2-config --cflags`
    SDL_LIB= `sdl2-config --libs`
else
    OPENGL_INC= -I/usr/X11R6/include
    OPENGL_LIB= -I/usr/lib64 -lGL -lGLU
    SDL_INC= `sdl-config --cflags`
    SDL_LIB= `sdl-config --libs`
endif

SIM_CFLAGS=$(SDL_INC) $(OPENGL_INC)
SIM_LINKFLAGS=$(SDL_LIB) $(OPENGL_LIB)

SIM_BIN_NAME = test

################################################################################

# CLOCK_OBJS are common between sim and FW builds
CLOCK_OBJS=main.o display_effects.o lookuptables.o input.o flashvars.o

# SIM_OBJS are sim-only
SIM_OBJS=sim_disp.o sim_rtc.o sim_time.o

# HW_OBJS are FW-only
HW_OBJS = me_startup_stm32f0xx.o
HW_OBJS += system_stm32f0xx.o stm32f0xx_rcc.o stm32f0xx_tim.o stm32f0xx_rtc.o stm32f0xx_pwr.o stm32f0xx_flash.o
HW_OBJS += hw.o time.o spi.o rtc.o led_disp.o uart.o lightsense.o

# DEFINES gets redefined below.
CFLAGS += $(DEFINES)

FINAL_FW_OBJS = $(addprefix obj_fw/, $(CLOCK_OBJS) $(HW_OBJS))
FINAL_SIM_OBJS = $(addprefix obj_sim/, $(CLOCK_OBJS) $(SIM_OBJS))

################################################################################

.PHONY: all
all:	fw

.PHONY: clean
clean:
	@rm -f *.bin *.elf $(SIM_BIN_NAME) *~ 
	@rm -f $(FINAL_FW_OBJS) $(FINAL_SIM_OBJS)

.PHONY: flash
flash:	main.fl.bin
	st-flash write main.fl.bin 0x08000000

#### Firmware targets
fw:	fw_dir main.fl.bin
# This is buggy; needs a dependency on the fw_dir from the bins!

%.bin:	%.elf
	@$(SIZE) $<
	@echo "[OBJ] $@"
	$(VERBOSE)$(OBJCOPY) $< -O binary $@

fw_dir:
	@mkdir -p obj_fw

main.fl.elf:	CFLAGS += $(FW_CFLAGS)
main.fl.elf:	$(FINAL_FW_OBJS)
	@echo "[LINK]  $@"
	$(VERBOSE)$(CC) $(FW_LINKFLAGS) $^ $(FW_LIBS) -o $@

#### Test target
$(SIM_BIN_NAME):	sim_dir sim_bin

sim_dir:
	@mkdir -p obj_sim

sim_bin:	DEFINES += -DSIM
sim_bin:	CFLAGS += $(SIM_CFLAGS)
sim_bin: 	CROSS_COMPILE =
sim_bin:	$(FINAL_SIM_OBJS)
	$(CC) $(SIM_LINKFLAGS) $^ -o $(SIM_BIN_NAME)


obj_sim/%.o obj_fw/%.o:	%.c
	@echo "[CC]  $<"
	$(VERBOSE)$(CC) $(CFLAGS) -c $< -o $@

obj_sim/%.o obj_fw/%.o:	%.s
	@echo "[AS]  $<"
	$(VERBOSE)$(CC) $(CFLAGS) -c $< -o $@

obj_sim/%.o obj_fw/%.o:	%.S
	@echo "[AS]  $<"
	$(VERBOSE)$(CC) $(CFLAGS) -c $< -o $@


# Temporaries copied in from afar:
%.c:	$(CMSIS)/Device/ST/STM32F0xx/Source/Templates/%.c
	@cp $< $@

%.c:	$(STMLIB)/src/%.c
	@cp $< $@

################################################################################
