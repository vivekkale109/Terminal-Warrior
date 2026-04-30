/*
 * combat.h - Turn-based combat system: attack resolution, AI, special moves
 */

#ifndef COMBAT_H
#define COMBAT_H

#include "types.h"

/* Main combat loop; returns 1 if player wins, 0 if player dies */
int  combat_start(Game *g, Enemy *enemy);

/* Single player attack; returns damage dealt */
int  combat_player_attack(Player *p, Enemy *e, char *log_buf);

/* Special ability based on class; returns 1 if used */
int  combat_player_special(Player *p, Enemy *e, char *log_buf);

/* Enemy takes its turn; fills log_buf */
void combat_enemy_turn(Enemy *e, Player *p, char *log_buf);

/* Display enemy ASCII art */
void combat_enemy_art(EnemyType type);

/* Display combat UI */
void combat_render(const Game *g, const Enemy *e, const char *last_log);

/* Log a combat message (shifts buffer) */
void combat_log(Game *g, const char *msg);

#endif /* COMBAT_H */
