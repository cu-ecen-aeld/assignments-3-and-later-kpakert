.PHONY: default
default: aesdsocket ;

BASENAME =aesdsocket
TARGET = $(BASENAME).out

# Platform Overrides
DEBUG = VERBOSE


CFLAGS = -Wall -Werror -g -O0 -std=c99 -lm -pthread -lrt
CPPFLAGS =$(INCLUDES) 

ifeq ($(DEBUG),VERBOSE)
  CPPFLAGS += -DVERBOSE
endif


ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)

  # Compiler Flags and Defines
  CC = aarch64-none-linux-gnu-gcc
  LD = aarch64-none-linux-gnu-gcc
  LDFLAGS = -Wl,-Map=$(BASENAME).map
  SIZE = size


else
  
  # Compiler Flags and Defines
  CC = gcc
  LD = ld
  LDFLAGS = -Wl,-Map=$(BASENAME).map
  SIZE = size

endif


OBJS = $(SOURCES:.c=.o)
PP = $(SOURCES:.c=.i)
ASM = $(SOURCES:.c=.asm)


%.o : %.c
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@
%.i : %.c
	$(CC) -E $< $(CFLAGS) $(CPPFLAGS) -o $@
%.asm : %.c
	$(CC) -S $< $(CFLAGS) $(CPPFLAGS) -o $@

.PHONY: build
build: all
.PHONY: all
all: aesdsocket
.PHONY: compile-all
compile-all: $(SOURCES)
	$(CC) -c $(SOURCES) $(CFLAGS) $(CPPFLAGS)
$(BASENAME).asm : $(TARGET)
	objdump --source $< > $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) -o $@
	$(SIZE) $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(PP) $(ASM) $(BASENAME).asm $(TARGET) $(BASENAME) $(BASENAME).map

