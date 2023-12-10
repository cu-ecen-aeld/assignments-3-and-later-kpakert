TARGET = aesdsocket
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g -O0 
LIB ?= -lpthread

.PHONY : all

all: aesdsocket.c
	$(CC) aesdsocket.c $(CFLAGS) -o aesdsocket $(LIB)


.PHONY : clean
clean:
	-rm -f *.o aesdsocket
