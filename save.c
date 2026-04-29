#include <stdio.h>
#include "save.h"

void saveGame(Player *p) {
    FILE *f = fopen("save.dat", "wb");
    fwrite(p, sizeof(Player), 1, f);
    fclose(f);
    printf("Game Saved!\n");
}

int loadGame(Player *p) {
    FILE *f = fopen("save.dat", "rb");
    if (!f) return 0;
    fread(p, sizeof(Player), 1, f);
    fclose(f);
    return 1;
}