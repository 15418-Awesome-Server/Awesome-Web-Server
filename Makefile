CC = mpicc
CFLAGS = -g -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: awesome cgi auto

awesome: awesome.c csapp.o cache.o
	$(CC) $(CFLAGS) -o awesome awesome.c csapp.o cache.o $(LIB)

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

cache.o:
	$(CC) $(CFLAGS) -c cache.c csapp.o

cgi:
	(cd cgi-bin; make)

auto:
	$(CC) $(CFLAGS) -o automate/auto automate/auto.c csapp.o

test:
	$(CC) $(CLFAGS) -o testing/test testing/messages.c

clean:
	rm -f *.o awesome *~
	(cd cgi-bin; make clean)

