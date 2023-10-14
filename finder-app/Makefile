BASENAME = writer
TARGET = $(BASENAME).out

# Platform Overrides
PLATFORM = HOST
DEBUG = VERBOSE


CFLAGS = -Wall -Werror -g -O0 -std=c99 -lm
CPPFLAGS =$(INCLUDES) -D$(PLATFORM)

ifeq ($(DEBUG),VERBOSE)
  CPPFLAGS += -DVERBOSE
endif


ifeq ($(PLATFORM),HOST)

  # Compiler Flags and Defines
  CC = gcc
  LD = ld
  LDFLAGS = -Wl,-Map=$(BASENAME).map
  SIZE = size

else ifeq ($(PLATFORM), MSP432)

  #todo

else
  $(error PLATFORM:$(PLATFORM) is not supported)
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
all: $(TARGET)
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
	rm -f $(OBJS) $(PP) $(ASM) $(BASENAME).asm $(TARGET) $(BASENAME).map

