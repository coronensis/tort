#
# TORT - Toy-grade RTOS with Tetris on top
#
# Makefile: MAKE(1) control file to build the Adafruit LCD library
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

CFLAGS += -ffreestanding -Wall -Wextra -Werror -pedantic -Wundef -Wshadow -Wcast-qual \
	  -Wcast-align -Wstrict-prototypes -Wredundant-decls -Wnested-externs \
	  -Wbad-function-cast -Wunreachable-code
CFLAGS += -gdwarf-2
CFLAGS += -mmcu=atmega328p
CFLAGS += -Os -mcall-prologues
CFLAGS += -DF_CPU=$(F_CPU)

# External library
LIBSRC = PCD8544.c

# The Adafruit LCD library is compiled and linked as a library to
# comply with the LGPL that governs that code.
all:
	avr-gcc --version
	avr-gcc $(CFLAGS) -c $(LIBSRC) -o PCD8544.o
	avr-ar rcs PCD8544.a PCD8544.o

# Remove build artefacts
clean:
	rm -f *.o *.a

