CC=clang
BIN=server
CFLAGS=-Wall
LIBS=-lcrypto
OBJECTS=server.o encrypt.o md5.o rc4.o


all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(BIN)
$(OBJECTS): %.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@


clean:
	rm -rf *.o $(BIN)


valgrind: all
	valgrind --leak-check=full ./server

CHK_SOURCES=server.c
check-syntax:
	gcc -Wall -Wextra -pedantic -fsyntax-only $(CHK_SOURCES)
