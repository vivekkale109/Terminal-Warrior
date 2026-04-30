/*
 * player.h - Player module: creation, stats, leveling, class abilities
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"

/* Initialize player with chosen class and name */
void player_create(Player *p, const char *name, PlayerClass cls);

/* Apply level-up if XP threshold reached; returns 1 if leveled */
int  player_check_level(Player *p);

/* Add XP; calls check_level internally */
void player_gain_xp(Player *p, int xp, char *msg_buf);

/* Heal player by amount (capped at max_hp) */
void player_heal(Player *p, int amount);

/* Restore mana by amount (capped at max_mp) */
void player_restore_mp(Player *p, int amount);

/* Apply status effect to player */
void player_apply_status(Player *p, StatusEffect eff, int turns);

/* Tick status effects each combat turn; returns damage taken */
int  player_tick_status(Player *p, char *msg_buf);

/* Get class name string */
const char *player_class_name(PlayerClass cls);

/* Get class ASCII art banner */
void player_class_art(PlayerClass cls);

/* Print player stat sheet */
void player_print_stats(const Player *p);

/* XP needed to reach next level */
int player_xp_for_level(int level);

#endif /* PLAYER_H */
