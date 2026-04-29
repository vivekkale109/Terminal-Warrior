#include <stdio.h>
#include "inventory.h"

void usePotion(int *hp, int maxHp, Inventory *inv) {
    if (inv->potions > 0) {
        *hp += 30;
        if (*hp > maxHp) *hp = maxHp;
        inv->potions--;
        printf("🧪 Potion used! HP restored.\n");
    } else {
        printf("No potions!\n");
    }
}