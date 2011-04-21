CC=gcc
CFLAGS=-c -g -Wall -I ../include

all: fuseserver.exe fuseclient.exe

clean:
	rm *.exe *.o

fuseserver.exe: fuseserver.o
	$(CC) fuseserver.o -o fuseserver.exe

fuseserver.o: fuseserver.c
	$(CC) $(CFLAGS) fuseserver.c

fuseclient.exe: fuseclient.o
	$(CC) fuseclient.o -o fuseclient.exe

fuseclient.o: fuseclient.c
	$(CC) $(CFLAGS) fuseclient.c
