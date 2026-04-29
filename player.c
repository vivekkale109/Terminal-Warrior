#include <stdio.h>
#include <string.h>
#include "player.h"

void createPlayer(Player *p) {
    printf("Enter your name: ");
    scanf("%s", p->name);

    printf("Choose class:\n1. Warrior\n2. Mage\n3. Rogue\n");
    scanf("%d", &p->classType);

    p->level = 1;
    p->xp = 0;

    if (p->classType == 1) {
        p->hp = p->maxHp = 120;
        p->attack = 15;
        p->defense = 10;
        p->mana = 5;
    } else if (p->classType == 2) {
        p->hp = p->maxHp = 80;
        p->attack = 10;
        p->defense = 5;
        p->mana = 20;
    } else {
        p->hp = p->maxHp = 100;
        p->attack = 12;
        p->defense = 8;
        p->mana = 10;
    }

    p->x = 1;
    p->y = 1;
}

void levelUp(Player *p) {
    if (p->xp >= 50) {
        p->level++;
        p->xp = 0;
        p->maxHp += 10;
        p->attack += 2;
        p->defense += 2;
        p->hp = p->maxHp;

        printf("\n🎉 LEVEL UP! Now Level %d\n", p->level);
    }
}