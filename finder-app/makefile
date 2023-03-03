CC=gcc
CFLAGS=
DIR=.
OBJ = writer.c

rebuild: clean writer

CROSS_COMPILE: CC=aarch64-none-linux-gnu-gcc
CROSS_COMPILE: rebuild

writer: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(DIR)/*.o writer
