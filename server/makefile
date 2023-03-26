CC=gcc
CFLAGS=-Wall -std=gnu1x
SRC=aesdsocket.c
OUTPUT=aesdsocket

all: clean default

default:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)

clean:
	rm -f $(OUTPUT)