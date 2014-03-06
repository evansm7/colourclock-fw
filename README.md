Colourclock firmware
====================

19th March 2018

Here lies firmware source for my Colourclock project.  See <http://www.axio.ms/projects/2018/03/19/Colourclock.html> for information/pictures, and <http://github.com/evansm7/colourclock-hw> for PCB design.

Copyright (c) 2014,2018 Matt Evans.  See below for licence.

This is a bare-metal program for the STM32F051 ARM Cortex-M0 microcontroller.  It needs to:

* Keep time
* Monitor button presses
* Monitor light levels
* Drive 180 LEDs fast
* Draw colourful patterns for the LEDs

This is primarily useful if you want to build the hardware above...  However, it might be useful for other similar projects, a known-working example of driving SPI/DMA/Timers, and so on.

Prerequisites:

* gcc-arm-embedded cross toolchain
* SDL (see below, sim build)

To build firmware:

`
make STM_LIB=/home/foo/STM32F0/STM32F0xx_StdPeriph_Lib_V1.0.0/ CROSS_COMPILE=/opt/local/gcc-arm-embedded/bin/arm-none-eabi-
`

To flash with ```st-util```:

`
make flash
`


Principle of operation
----------------------

The primary task of this firmware is to control 180 LEDs (60x RGB LEDs) arranged in groups of 15 (a quadrant) and in three sub-groups per quadrant.  (Secondary task is to keep time, draw time, watch for button presses etc., but that's easier.)

Each group has one 16-bit shift register (one bit unused), driving the low side of all sub-gropus of 5 RGB LEDs.  Three P-type FETs drive the anodes of each sub-group.  By selecting each sub-group in turn, and changing the shift register contents in-between, all LEDs can be energised separately.

To get shades of brightness, PWM is used (nothing exciting like BCM) and, for a given frame, a sub-group is enabled and a full display cycle performed before disabling that sub-group and moving to the next.  This scanning occurs in ```led_disp.c```, as follows:

* A simple linear framebuffer is maintained, with 60 RGB values.
* ```led_fb_to_pwm_buffer()``` transforms the per-pixel RGB values to a long buffer of bits (```pwm_data```) which represent whether the corresponding LEDs are 'on' given the PWM tick.  An LED starts on, and is turned off when the current PWM tick is greater than the pixel value.
 * This transformation occurs once when a framebuffer bank swap occurs.  It moves complexity/cost to the framebuffer update and removes complexity/cost from the PWM/DMA interrupts.
 * This buffer is 'enormous': at 6bit/channel, double-buffered, it's 3KB (out of 4KB total RAM).
* Timer TIM14 is used to trigger an IRQ on every PWM period.  On each IRQ, data is output to the scan chain by starting a DMA transfer of 8 bytes to SPI.
 * DMA is used because it lets the IRQ handler complete quickly, which bit-banging (or manual loading of SPI TX registers) would not.  This drastically reduces the CPU overhead, which is a consideration because a fast refresh rate costs ~30% even with DMA!
* All the IRQs need to do is stream out the ```pwm_data``` contents using SPI DMA, changing the FET-driving GPIOs as appropriate to scan through the sub-groups in sequence.  This keeps the dynamic CPU usage low and avoids the timer IRQ handler having to re-calculate "Is the LED still on?" over and over.  (Picture an LED refresh rate of 300Hz, but a framebuffer update of 1Hz!)
* Overall display brightness control is achieved not by scaling the RGB output data (how crude!) but by using a fast PWM output (375KHz) from timer TIM1.  This is controlled from a periodic sample of an analog input driven from an LDR, and scaled using user-configurable lo-/hi-brightness thresholds.

The display runs at 300Hz refresh (that's do-full-pass-with-all-PWM-complete, not a single PWM tick) and, at 6bits per channel (18-bit colour), the IRQs to trigger the DMA/scan the LEDs use about 29% of the CPU time.  The STM32F051 runs at 48MHz.

The clock face/style is rendered into the framebuffer in ```display_effects.c```.

Input is gathered from GPIO button inputs and turned into input events, in ```input.c```, which is used to drive a very simple UI state machine in ```main.c```.  This provides a number of modes to set the time, configure brightness-scaling thresholds, and change display effect.

These thresholds are stored in flash, in ```flashvars.c```.  This makes a simple attempt at avoiding erasing the flash for every write, by keeping a trivial journal of configuration variable updates.


Simulation build
----------------

It was annoying trying to tweak display blending with a compile-flash-run cycle, so ```make test``` will build the firmware as a host-native SDL program.  Instead of rendering the framebuffer by pushing it to LEDs through SPI, it's drawn as radial rectangles using OpenGL.  :-)


Ugly parts
----------

There's a tiny amount of (cardinal sin) commented-out code in here, mainly surrounding test/debug code or coefficients/value scaling.

I've also mixed C/C++ comment styles, another hangable offence.


Licence
=======
The following files have other authors and have their own licence as detailed within:

* ```me_startup_stm32f0xx.s```
* ```stm32f051x6.ld```

For the remainder:

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.