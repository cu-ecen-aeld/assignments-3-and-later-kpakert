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

EXTRA_CFLAGS = -DUSE_AESD_CHAR_DEVICE=1
EXTRA_CFLAGS += -USE_AESD_CHAR_DEVICE=1

$(EXEC): $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o $(EXEC)

###################### Clean ######################
clean:
	-rm -rf *.o $(EXEC)
