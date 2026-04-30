/*
 * save.h - Save / load game progress to binary file
 *
 * The save file stores the entire Game struct as a raw binary blob
 * preceded by a magic number and version tag so we can detect stale
 * or corrupted files gracefully.
 */

#ifndef SAVE_H
#define SAVE_H

#include "types.h"

/* Write current game state to SAVE_FILE.
   Returns 1 on success, 0 on failure. */
int save_game(const Game *g);

/* Load game state from SAVE_FILE into g.
   Returns 1 on success, 0 if file missing/corrupt. */
int load_game(Game *g);

/* Return 1 if a save file exists, 0 otherwise */
int save_exists(void);

/* Delete the save file (called on game over or new game) */
void save_delete(void);

/* Display the save/load menu; returns user's choice (1=save, 2=load, 0=cancel) */
int save_menu(Game *g);

#endif /* SAVE_H */