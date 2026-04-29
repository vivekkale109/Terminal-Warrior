#include <stdio.h>
#include <stdlib.h>
#include "combat.h"

void startCombat(Player *p) {
    Enemy e;
    e.hp = 50;
    e.attack = 8;

    printf("\n⚔️ Enemy Appeared!\n");

    while (e.hp > 0 && p->hp > 0) {
        int choice;
        printf("\n1. Attack\n2. Defend\nChoice: ");
        scanf("%d", &choice);

        if (choice == 1) {
            int dmg = p->attack - (rand() % 5);
            if (rand() % 5 == 0) {
                dmg *= 2;
                printf("💥 Critical Hit!\n");
            }
            e.hp -= dmg;
            printf("You dealt %d damage\n", dmg);
        }

        if (e.hp > 0) {
            int edmg = e.attack - p->defense / 2;
            if (edmg < 1) edmg = 1;
            p->hp -= edmg;
            printf("Enemy hits you for %d\n", edmg);
        }
    }

    if (p->hp > 0) {
        printf("🏆 You won!\n");
        p->xp += 25;
    } else {
        printf("💀 You died...\n");
    }
}