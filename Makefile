# Makefile — Terminal Warrior
CC      = gcc
TARGET  = terminal_warrior
SRCS    = main.c player.c map.c combat.c inventory.c save.c
OBJS    = $(SRCS:.c=.o)
CFLAGS  = -Wall -Wextra -O2 -std=gnu11 -D_DEFAULT_SOURCE
LDFLAGS = -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  Build successful!  Run with:  ./$(TARGET)"
	@echo ""

%.o: %.c types.h player.h map.h combat.h inventory.h save.h
	$(CC) $(CFLAGS) -c -o $@ $<

debug: CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -std=gnu11 -D_DEFAULT_SOURCE
debug: $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) savegame.dat

.PHONY: all debug run clean
