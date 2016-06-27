
CC = gcc
LD = ld

CFLAGS  += -Wall -g -lgcc -lm -ansi

all: nikolaus.elf

interpreter.elf: interpreter.o interpolator.o
	$(CC) $(CFLAGS) $^ -o $@

nikolaus.elf:
	$(CC) $(CFLAGS) $^ -o $@
	
%.o: %.c %s
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f *.o *.out *.bin *.elf *.hex *.map
	