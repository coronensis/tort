#
# playground - Toy-grade RTOS with Tetris on top
#
# Makefile: MAKE(1) control file to build the scheduling playground application
#           to be run in the AVR simulator
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
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR playground
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
CFLAGS += -mmcu=atmega328
CFLAGS += -Os -mcall-prologues
CFLAGS += -DF_CPU=$(F_CPU)

# Linker flags
LDFLAGS += -Wl,-Map,playground.map

# Source code files
CSRC = playground.c

# Default target
all: playground.elf

# Compile the source code
playground.elf: $(CSRC)
	avr-gcc --version
	avr-gcc $(CFLAGS) $(CSRC) -o playground.elf
	avr-objcopy -j .text -j .data -j .eeprom -j .fuse -O ihex playground.elf playground.hex
	avr-objcopy -j .text -j .data -O binary playground.elf playground.bin
	avr-objdump -h -S -C playground.elf > playground.lst
	avr-nm -n playground.elf > playground.sym
	avr-size -C --mcu=atmega328 playground.elf

# Address of the memory mapped UART data register
PORT_UART = 0xC6

# Run on the avr simulator
sim: playground.elf
	simulavr -d atmega328 -s -F $(F_CPU) -f playground.elf -W $(PORT_UART),- -T exit

# Run on the avr simulator and produce an execution trace file
trace: playground.elf
	simulavr -d atmega328 -s -F $(F_CPU) -f playground.elf -t trace.txt -W $(PORT_UART),- -T exit

# Run on the avr simulator and wait for avr-gdb to connect
gdb: playground.elf
	simulavr -d atmega328 -g -F $(F_CPU) -s -f playground.elf

# Remove build artefacts
clean:
	rm -f  *.elf *.hex *.bin *.lst *.sym trace*.txt

