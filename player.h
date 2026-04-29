#ifndef PLAYER_H
#define PLAYER_H

#define NAME_LEN 50

typedef struct {
    char name[NAME_LEN];
    int classType; // 1=Warrior, 2=Mage, 3=Rogue
    int hp, maxHp;
    int attack;
    int defense;
    int mana;
    int level;
    int xp;
    int x, y; // position
} Player;

void createPlayer(Player *p);
void levelUp(Player *p);

#endif