
CC = gcc
LD = ld

CFLAGS  += -Wall -g -lgcc -lm

all: interpreter.elf

interpreter.elf: interpreter.o interpolator.o
	$(CC) $(CFLAGS) $^ -o $@
	
%.o: %.c %s
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f *.o *.out *.bin *.elf *.hex *.map
	