
Before you start:

Install simulavr and avr-gdb

Simulation options:

-----------------------------------------------------------------
1.) Run the simulation and see the output live in the terminal
-----------------------------------------------------------------

# make sim

-----------------------------------------------------------------
2.) Record a trace of the program execution to the file trace.txt
-----------------------------------------------------------------
Note that the trace file gets HUGE _really fast_. So make sure
you have enough disk space or stop the simulation in time

# make trace

-----------------------------------------------------------------
3.) Run the simulator with a GDB remote backend
-----------------------------------------------------------------

# make gdb

In another terminal start avr-gdb

# avr-gdb

In GDB you can use the following commands to connect to the simulator
and set a breakpoint on the main function

file simu.elf
target remote localhost:1212
load
b main
c



