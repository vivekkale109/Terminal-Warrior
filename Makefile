CC=gcc
CFLAGS=-Wall

all:
	$(CC) main.c player.c map.c combat.c inventory.c save.c -o game

run:
	./game