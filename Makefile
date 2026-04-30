Copy

# ─────────────────────────────────────────────────────────────────────────────
# Makefile — Terminal Warrior
#
# Usage:
#   make          Build the game (output: terminal_warrior)
#   make debug    Build with AddressSanitizer and debug symbols
#   make clean    Remove compiled objects and binary
#   make run      Build and immediately launch the game
# ─────────────────────────────────────────────────────────────────────────────
 
CC      = gcc
TARGET  = terminal_warrior
 
# Source files — add any new .c module here
SRCS    = main.c player.c map.c combat.c inventory.c save.c
 
OBJS    = $(SRCS:.c=.o)
 
# Release flags: optimise, show all warnings, link math library
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lm
 
# ── Default target ────────────────────────────────────────────────────────────
all: $(TARGET)
 
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  Build successful!  Run with:  ./$(TARGET)"
	@echo ""
 
# Compile each .c file to a .o; depend on all headers so changes trigger rebuild
%.o: %.c types.h player.h map.h combat.h inventory.h save.h
	$(CC) $(CFLAGS) -c -o $@ $<
 
# ── Debug build (with sanitizers) ────────────────────────────────────────────
debug: CFLAGS = -Wall -Wextra -g -fsanitize=address,undefined -std=c11
debug: $(TARGET)
 
# ── Run shortcut ─────────────────────────────────────────────────────────────
run: all
	./$(TARGET)
 
# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(TARGET) savegame.dat
 
.PHONY: all debug run clean
 