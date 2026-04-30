/*
 * inventory.h - Inventory management: display, use, pickup, drop
 *
 * The inventory holds up to MAX_INVENTORY item stacks. Each slot is an
 * Item struct with a quantity field so duplicate items stack neatly.
 */

#ifndef INVENTORY_H
#define INVENTORY_H

#include "types.h"

/* Show the full inventory screen (blocking, waits for key) */
void inv_show(Game *g);

/* Use item at slot index during combat; fills msg_buf.
   Returns 1 if item was used, 0 if invalid/empty. */
int  inv_use(Game *g, int slot, char *msg_buf);

/* Add an item to inventory; returns 1 on success, 0 if full.
   If an identical item already exists, increments quantity. */
int  inv_add(Inventory *inv, const Item *item);

/* Remove one unit from slot; removes slot if quantity hits 0.
   Returns 1 on success, 0 if slot empty or invalid. */
int  inv_remove(Inventory *inv, int slot);

/* Print a short inventory summary (used in HUD) */
void inv_print_short(const Inventory *inv);

/* Count usable items (health/mana potions) for combat UI hint */
int  inv_count_usable(const Inventory *inv);

#endif /* INVENTORY_H */
