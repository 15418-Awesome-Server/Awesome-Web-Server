CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder superecho

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

superecho: superecho.c
	$(CC) $(CFLAGS) -o superecho superecho.c

clean:
	rm -f adder superecho *~
