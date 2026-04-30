/*
 * types.h - Shared types, constants, and struct definitions for Terminal Warrior
 * This file is included by all modules to ensure consistent data structures.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>

/* ─────────────────────────────────────────────
   GAME CONSTANTS
───────────────────────────────────────────── */
#define MAP_WIDTH       50
#define MAP_HEIGHT      20
#define MAX_ENEMIES     15
#define MAX_ITEMS       20
#define MAX_INVENTORY   10
#define MAX_NAME_LEN    32
#define MAX_LEVELS      20
#define FOG_RADIUS      5
#define SAVE_FILE       "savegame.dat"
#define VERSION         "1.0.0"

/* Map tile characters */
#define TILE_WALL       '#'
#define TILE_FLOOR      '.'
#define TILE_PLAYER     '@'
#define TILE_ENEMY      'E'
#define TILE_BOSS       'B'
#define TILE_TREASURE   'T'
#define TILE_STAIRS     '>'
#define TILE_EMPTY      ' '

/* ANSI color codes for terminal display */
#define COLOR_RESET     "\033[0m"
#define COLOR_RED       "\033[1;31m"
#define COLOR_GREEN     "\033[1;32m"
#define COLOR_YELLOW    "\033[1;33m"
#define COLOR_BLUE      "\033[1;34m"
#define COLOR_MAGENTA   "\033[1;35m"
#define COLOR_CYAN      "\033[1;36m"
#define COLOR_WHITE     "\033[1;37m"
#define COLOR_GRAY      "\033[0;37m"
#define COLOR_ORANGE    "\033[0;33m"
#define BG_BLACK        "\033[40m"

/* ─────────────────────────────────────────────
   ENUMERATIONS
───────────────────────────────────────────── */

/* Player classes */
typedef enum {
    CLASS_WARRIOR = 1,
    CLASS_MAGE    = 2,
    CLASS_ROGUE   = 3
} PlayerClass;

/* Item types */
typedef enum {
    ITEM_NONE         = 0,
    ITEM_HEALTH_POTION= 1,
    ITEM_MANA_POTION  = 2,
    ITEM_WEAPON       = 3,
    ITEM_ARMOR        = 4,
    ITEM_ANTIDOTE     = 5
} ItemType;

/* Status effects */
typedef enum {
    STATUS_NONE    = 0,
    STATUS_POISON  = 1,
    STATUS_STUN    = 2,
    STATUS_DEFEND  = 4   /* bitmask: can combine */
} StatusEffect;

/* Enemy types */
typedef enum {
    ENEMY_GOBLIN  = 0,
    ENEMY_SKELETON= 1,
    ENEMY_ORC     = 2,
    ENEMY_VAMPIRE = 3,
    ENEMY_DRAGON  = 4   /* Boss */
} EnemyType;

/* Combat actions */
typedef enum {
    ACTION_ATTACK  = 1,
    ACTION_DEFEND  = 2,
    ACTION_ITEM    = 3,
    ACTION_SPECIAL = 4,
    ACTION_FLEE    = 5
} CombatAction;

/* Game states */
typedef enum {
    STATE_MENU     = 0,
    STATE_PLAYING  = 1,
    STATE_COMBAT   = 2,
    STATE_GAMEOVER = 3,
    STATE_WIN      = 4
} GameState;

/* ─────────────────────────────────────────────
   STRUCTS
───────────────────────────────────────────── */

/* Single item definition */
typedef struct {
    char      name[32];
    ItemType  type;
    int       value;       /* HP/MP restored, or attack/defense bonus */
    int       quantity;
} Item;

/* Player inventory */
typedef struct {
    Item  slots[MAX_INVENTORY];
    int   count;
} Inventory;

/* Player character */
typedef struct {
    char        name[MAX_NAME_LEN];
    PlayerClass pclass;

    /* Core stats */
    int         hp;
    int         max_hp;
    int         mp;
    int         max_mp;
    int         attack;
    int         defense;
    int         speed;

    /* Progression */
    int         level;
    int         xp;
    int         xp_to_next;
    int         dungeon_level;   /* which floor we're on */

    /* Position on map */
    int         x;
    int         y;

    /* Status */
    StatusEffect status;
    int          status_turns;   /* how many turns the effect lasts */
    int          defend_bonus;   /* temporary defense when blocking */

    Inventory   inv;
} Player;

/* Enemy character */
typedef struct {
    char        name[32];
    EnemyType   type;
    int         hp;
    int         max_hp;
    int         attack;
    int         defense;
    int         xp_reward;
    int         x;
    int         y;
    int         alive;
    StatusEffect status;
    int          status_turns;
    int          is_boss;
} Enemy;

/* Map cell */
typedef struct {
    char tile;       /* '#', '.', 'T', '>', etc. */
    int  visible;    /* currently in FOV? */
    int  explored;   /* has player seen it before? */
} Cell;

/* Full game map */
typedef struct {
    Cell    grid[MAP_HEIGHT][MAP_WIDTH];
    Enemy   enemies[MAX_ENEMIES];
    int     enemy_count;
    Item    loot[MAX_ITEMS];
    int     loot_count;
    int     stairs_x;
    int     stairs_y;
} GameMap;

/* Top-level game context (passed around modules) */
typedef struct {
    Player    player;
    GameMap   map;
    GameState state;
    int       turn;
    char      message[256];   /* last event message shown to player */
    char      combat_log[5][128];  /* recent combat messages */
    int       log_count;
} Game;

#endif /* TYPES_H */