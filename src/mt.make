CC=clang
CFLAGS=-Wall -I../lib/
LDFLAGS=-lcrypto -pthread -L../lib/ -lshso
OBJECTS=mt_server.o
BIN=../bin/mt_server

CHK_SOURCES=mt_server.c
#####################################

all: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJECTS)
%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	rm -rf $(OBJECTS) $(BIN)

valgrind: all
	valgrind --leak-check=full $(BIN)

check-syntax:
	gcc -Wall -Wextra -pedantic -fsyntax-only $(CHK_SOURCES)