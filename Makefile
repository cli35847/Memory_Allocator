TARGETS = hw_malloc.o
CC ?= gcc
CFLAGS += -g -std=gnu99 -Wall
OBJS = $(TARGETS)

all: $(TARGETS)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o
