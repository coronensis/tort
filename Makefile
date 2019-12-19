#
# ToRT - Tetris device with toy-grade OSEK/VDX-inspired RTOS
#
# Makefile: MAKE(1) control file to build the software and download
#           it to the microcontroller
#
# Copyright (c) 2019, Helmut Sipos <helmut.sipos@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# MCU clock frequency
F_CPU = 8000000

# Flags for C files
CFLAGS += -std=gnu89
CFLAGS += -ffreestanding -Wall -Wextra -Werror -pedantic -Wundef -Wshadow \
	  -Wunused-parameter -Warray-bounds -Wstrict-prototypes -Wredundant-decls \
	  -Wbad-function-cast -Wunreachable-code
CFLAGS += -gdwarf-2
CFLAGS += -mmcu=atmega328p
CFLAGS += -Os -mcall-prologues
CFLAGS += -I ./lcd5110
CFLAGS += -DF_CPU=$(F_CPU)

# Linker flags
LDFLAGS += -Wl,-Map,tort.map

# Source code files
CSRC = uc.c os.c ap.c

# Default target
all: tort.hex

# Compile the operating system, microcontroller layer, services and
# application into the image to be used for flashing.
# The Adafruit LCD library is compiled and linked as a library to
# comply with the LGPL that governs that code.
tort.hex: $(CSRC) lcd5110/PCD8544.c
	cd lcd5110 && $(MAKE)
	avr-gcc --version
	avr-gcc $(CFLAGS) $(CSRC) lcd5110/PCD8544.a -o tort.elf
	avr-objcopy -j .text -j .data -j .eeprom -j .fuse -O ihex tort.elf tort.hex
	avr-objcopy -j .text -j .data -O binary tort.elf tort.bin
	avr-objdump -h -S -C tort.elf > tort.lst
	avr-nm -n tort.elf > tort.sym
	avr-size -C --mcu=atmega328p tort.elf

# Remove build artefacts
clean:
	cd lcd5110 && $(MAKE) clean
	rm -f *.o *.a *.elf *.hex *.bin *.lst *.sym trace*.txt

# NOTE: The following gpio stuff only applies when running on a Raspberry Pi

# Download the firmware to the microcontroller
install: tort.hex
	gpio -g mode 26 out
	gpio -g write 26 0
	avrdude -V -p m328p -P /dev/spidev0.0 -c linuxspi -b 10000 -U flash:w:tort.hex
	gpio -g write 26 1

# Read the fuse bits of the microcontroller
rdfuse:
	gpio -g mode 26 out
	gpio -g write 26 0
	avrdude -V -p m328p -P /dev/spidev0.0 -c linuxspi -b 10000 -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h
	gpio -g write 26 1

# Write the fuse bits of the microcontroller
wrfuse:
	gpio -g mode 26 out
	gpio -g write 26 0
	avrdude -p m328p -P /dev/spidev0.0 -c linuxspi -b 10000 -U lfuse:w:0xE2:m -U hfuse:w:0xDD:m -U efuse:w:0xFF:m
	gpio -g write 26 1

# HW reset the microcontroller
reset:
	gpio -g write 26 0
	sleep 1
	gpio -g write 26 1

