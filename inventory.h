#ifndef INVENTORY_H
#define INVENTORY_H

typedef struct {
    int potions;
} Inventory;

void usePotion(int *hp, int maxHp, Inventory *inv);

#endif