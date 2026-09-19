CC = gcc
CFLAGS = -g -Wall -std=gnu99 -mcpu=cortex-a72 -DHAMM_ASM -DASM
# enable Assembler version of Hamming distance
#  -DHAMM_ASM
# enable Assembler interface to low-level functions:
# -DASM

OBJS = cw2.o lcd-fcts.o cw2-aux.o lcd-binary.o hamming.o
LIBS = -lm

all: cw2

# Link the object files to give the program
cw2: $(OBJS)
	$(CC) $(CFLAGS) -o cw2 $(OBJS) $(LIBS)

# Compile a C source file to an object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile an assembler source file to an object file
%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<


# Header file dependencies

cw2.o: cw2-config.h cw2-aux.h lcd-binary.h lcd-fcts.h
cw2-aux.o: cw2-aux.h
lcd-fcts.o: cw2-config.h lcd-fcts.h lcd-binary.h
lcd-binary.o: lcd-binary.h

clean:
	rm -f cw2 $(OBJS)
