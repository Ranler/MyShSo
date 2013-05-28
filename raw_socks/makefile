CC=clang
BIN=server
CFLAGS=-Wall

all: server.o
	$(CC) server.o -o $(BIN)
server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -rf *.o $(BIN)


CHK_SOURCES=server.c
check-syntax:
	gcc -Wall -Wextra -pedantic -fsyntax-only $(CHK_SOURCES)
