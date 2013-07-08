CC=clang
CFLAGS=-Wall -Wextra -Werror -Wconversion -Wshadow -I../lib/
LDFLAGS=-lcrypto -L../lib/ -lshso
OBJECTS=mp_server.o
BIN=../bin/mp_server

CHK_SOURCES=mp_server.c
#################################################

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
