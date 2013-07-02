CC=gcc
LD=ld
CFLAGS=-Wall
LDFLAGS=-shared -fPIC
OBJECTS=encrypt.o md5.o rc4.o
TARGET=libshso.so

all: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS)
%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	rm -rf *.o $(TARGET)
