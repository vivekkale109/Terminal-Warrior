#ifndef MAP_H
#define MAP_H

#define WIDTH 20
#define HEIGHT 10

extern char map[HEIGHT][WIDTH];

void generateMap();
void drawMap(int px, int py);

#endif