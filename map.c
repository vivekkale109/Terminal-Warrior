#include <stdio.h>
#include <stdlib.h>
#include "map.h"

char map[HEIGHT][WIDTH];

void generateMap() {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (i == 0 || j == 0 || i == HEIGHT - 1 || j == WIDTH - 1)
                map[i][j] = '#';
            else {
                int r = rand() % 10;
                if (r < 7) map[i][j] = '.';
                else if (r == 8) map[i][j] = 'E';
                else map[i][j] = 'T';
            }
        }
    }
}

void drawMap(int px, int py) {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (i == py && j == px)
                printf("@");
            else
                printf("%c", map[i][j]);
        }
        printf("\n");
    }
}