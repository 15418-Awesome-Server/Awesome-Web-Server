CC = mpicc
CFLAGS = -g -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: tiny cgi auto

tiny: tiny.c csapp.o
	$(CC) $(CFLAGS) -o tiny tiny.c csapp.o $(LIB)

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

cgi:
	(cd cgi-bin; make)

auto:
	$(CC) $(CFLAGS) -o automate/auto automate/auto.c csapp.o

test:
	$(CC) $(CLFAGS) -o testing/test testing/messages.c

clean:
	rm -f *.o tiny *~
	(cd cgi-bin; make clean)

