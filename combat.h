#ifndef COMBAT_H
#define COMBAT_H

#include "player.h"

typedef struct {
    int hp;
    int attack;
} Enemy;

void startCombat(Player *p);

#endif