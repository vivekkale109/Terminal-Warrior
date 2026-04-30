/*
 * map.h - Dungeon map generation, rendering, FOV, movement
 */

#ifndef MAP_H
#define MAP_H

#include "types.h"

/* Generate a new random dungeon map */
void map_generate(GameMap *map, int dungeon_level);

/* Render the map to terminal with fog of war around player */
void map_render(const Game *g);

/* Update field of view from player position */
void map_update_fov(GameMap *map, int px, int py);

/* Try to move player; returns 1 on success, 0 if blocked */
int  map_move_player(Game *g, int dx, int dy);

/* Place the player at a valid starting position */
void map_place_player(Game *g);

/* Check if a tile is walkable */
int  map_is_walkable(const GameMap *map, int x, int y);

/* Get enemy at position, or NULL */
Enemy *map_enemy_at(GameMap *map, int x, int y);

/* Get loot at position; removes it from map; returns 1 if found */
int  map_pickup_loot(Game *g, int x, int y, char *msg_buf);

/* Print the HUD (HP/MP/level bar below map) */
void map_print_hud(const Game *g);

#endif /* MAP_H */
