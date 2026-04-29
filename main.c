#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

#include "player.h"
#include "map.h"
#include "combat.h"
#include "inventory.h"
#include "save.h"

char getch() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int main() {
    srand(time(NULL));

    Player p;
    Inventory inv = {3};

    if (!loadGame(&p)) {
        createPlayer(&p);
    }

    generateMap();

    while (p.hp > 0) {
        system("clear");
        drawMap(p.x, p.y);

        printf("\nHP: %d | Level: %d | XP: %d\n", p.hp, p.level, p.xp);
        printf("Move (WASD), i=inventory, q=quit\n");

        char input = getch();

        int nx = p.x, ny = p.y;

        if (input == 'w') ny--;
        if (input == 's') ny++;
        if (input == 'a') nx--;
        if (input == 'd') nx++;

        char tile = map[ny][nx];

        if (tile != '#') {
            p.x = nx;
            p.y = ny;

            if (tile == 'E') {
                startCombat(&p);
                map[ny][nx] = '.';
            }

            if (tile == 'T') {
                printf("💰 Found treasure!\n");
                inv.potions++;
                map[ny][nx] = '.';
                getchar();
            }
        }

        if (input == 'i') {
            usePotion(&p.hp, p.maxHp, &inv);
            getchar();
        }

        if (input == 'q') {
            saveGame(&p);
            break;
        }

        levelUp(&p);
    }

    return 0;
}