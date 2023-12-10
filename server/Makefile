#################### Makefile #####################
 
################# Compiler Flags ##################
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -pthread -lrt

############## Source & Executable ################
SRCS = aesdsocket.c
EXEC = aesdsocket

##################### Targets #####################
default : $(EXEC)
all : $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o $(EXEC)

###################### Clean ######################
clean:
	-rm -rf *.o $(EXEC)
