EXEC = hw4_mm_test
TARGETS = $(EXEC)
CC ?= gcc
CFLAGS += -g -std=gnu99 -I./lib -Wall
OBJS = hw4_mm_test.o ./lib/hw_malloc.o
SUBDIR = ./lib

all: $(TARGETS)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

hw4_mm_test.o: %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

./lib/hw_malloc.o:
	$(MAKE) -C $(SUBDIR)

clean:
	rm -rf *.o $(EXEC)
	$(MAKE) -C $(SUBDIR) clean
