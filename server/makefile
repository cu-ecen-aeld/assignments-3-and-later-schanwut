CC?=gcc
CFLAGS?=-Wall -std=gnu1x
SRC?=aesdsocket.c
OUTPUT?=aesdsocket
LDFLAGS?=-pthread -lrt

all: clean default

default:
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUTPUT)