/*
 * map.c - Dungeon map generation using BSP-style room placement,
 *          fog-of-war (circular FOV), rendering with ANSI colors,
 *          enemy/loot placement.
 *
 * Algorithm overview:
 *   1. Fill entire grid with walls.
 *   2. Carve random rectangular rooms ensuring no overlap.
 *   3. Connect rooms with L-shaped corridors.
 *   4. Scatter enemies and treasure.
 *   5. Place stairs down ('>') in last room.
 *   6. Place boss (type ENEMY_DRAGON) on deepest floor (level 5).
 */

#include "map.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ─── Room helper ──────────────────────────────────────────────────────── */
typedef struct { int x, y, w, h; } Room;

#define MAX_ROOMS 12

static Room rooms[MAX_ROOMS];
static int  room_count;

/* Fill a rectangle of the grid with a tile */
static void fill_rect(GameMap *map, int x, int y, int w, int h, char tile) {
    for (int ry = y; ry < y + h && ry < MAP_HEIGHT; ry++)
        for (int rx = x; rx < x + w && rx < MAP_WIDTH; rx++)
            map->grid[ry][rx].tile = tile;
}

/* Carve horizontal corridor */
static void carve_h(GameMap *map, int x1, int x2, int y) {
    int start = x1 < x2 ? x1 : x2;
    int end   = x1 < x2 ? x2 : x1;
    for (int x = start; x <= end; x++)
        if (x > 0 && x < MAP_WIDTH && y > 0 && y < MAP_HEIGHT)
            map->grid[y][x].tile = TILE_FLOOR;
}

/* Carve vertical corridor */
static void carve_v(GameMap *map, int y1, int y2, int x) {
    int start = y1 < y2 ? y1 : y2;
    int end   = y1 < y2 ? y2 : y1;
    for (int y = start; y <= end; y++)
        if (x > 0 && x < MAP_WIDTH && y > 0 && y < MAP_HEIGHT)
            map->grid[y][x].tile = TILE_FLOOR;
}

/* Check if a new room overlaps any existing room (with 1-cell margin) */
static int rooms_overlap(Room *a, Room *b) {
    return !(a->x + a->w + 1 < b->x ||
             b->x + b->w + 1 < a->x ||
             a->y + a->h + 1 < b->y ||
             b->y + b->h + 1 < a->y);
}

/* Room center */
static void room_center(Room *r, int *cx, int *cy) {
    *cx = r->x + r->w / 2;
    *cy = r->y + r->h / 2;
}

/* ─── Enemy name table ─────────────────────────────────────────────────── */
static const char *ENEMY_NAMES[] = {
    "Goblin", "Skeleton", "Orc", "Vampire", "Dragon (BOSS)"
};

/* Enemy base stats scaled by dungeon level */
static void init_enemy(Enemy *e, EnemyType type, int dlevel) {
    memset(e, 0, sizeof(Enemy));
    strncpy(e->name, ENEMY_NAMES[type], 32);
    e->type    = type;
    e->alive   = 1;
    e->is_boss = (type == ENEMY_DRAGON);

    /* Scale stats by dungeon floor */
    int scale = dlevel;
    switch (type) {
        case ENEMY_GOBLIN:
            e->max_hp  = 20 + scale * 4;
            e->attack  = 6  + scale * 2;
            e->defense = 2;
            e->xp_reward = 15 + scale * 5;
            break;
        case ENEMY_SKELETON:
            e->max_hp  = 30 + scale * 5;
            e->attack  = 8  + scale * 2;
            e->defense = 4;
            e->xp_reward = 25 + scale * 6;
            break;
        case ENEMY_ORC:
            e->max_hp  = 50 + scale * 8;
            e->attack  = 12 + scale * 3;
            e->defense = 6;
            e->xp_reward = 40 + scale * 8;
            break;
        case ENEMY_VAMPIRE:
            e->max_hp  = 65 + scale * 10;
            e->attack  = 15 + scale * 3;
            e->defense = 8;
            e->xp_reward = 60 + scale * 10;
            break;
        case ENEMY_DRAGON:
            e->max_hp  = 200 + scale * 20;
            e->attack  = 25  + scale * 5;
            e->defense = 12;
            e->xp_reward = 300;
            e->is_boss = 1;
            break;
    }
    e->hp = e->max_hp;
}

/* ─── Map generation ────────────────────────────────────────────────────── */

void map_generate(GameMap *map, int dungeon_level) {
    /* Clear map and state */
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->grid[y][x].tile     = TILE_WALL;
            map->grid[y][x].visible  = 0;
            map->grid[y][x].explored = 0;
        }
    map->enemy_count = 0;
    map->loot_count  = 0;
    room_count       = 0;

    /* ── Step 1: place random rooms ─────────────────────── */
    int attempts = 0;
    while (room_count < MAX_ROOMS && attempts < 200) {
        attempts++;
        Room r;
        r.w = 4 + rand() % 7;   /* width 4-10 */
        r.h = 3 + rand() % 5;   /* height 3-7 */
        r.x = 1 + rand() % (MAP_WIDTH  - r.w - 2);
        r.y = 1 + rand() % (MAP_HEIGHT - r.h - 2);

        /* Check overlap */
        int overlap = 0;
        for (int i = 0; i < room_count; i++)
            if (rooms_overlap(&r, &rooms[i])) { overlap = 1; break; }
        if (overlap) continue;

        /* Carve room */
        fill_rect(map, r.x, r.y, r.w, r.h, TILE_FLOOR);
        rooms[room_count++] = r;
    }

    /* ── Step 2: connect rooms with corridors ───────────── */
    for (int i = 1; i < room_count; i++) {
        int cx1, cy1, cx2, cy2;
        room_center(&rooms[i - 1], &cx1, &cy1);
        room_center(&rooms[i],     &cx2, &cy2);

        /* Randomly choose H-then-V or V-then-H */
        if (rand() % 2) {
            carve_h(map, cx1, cx2, cy1);
            carve_v(map, cy1, cy2, cx2);
        } else {
            carve_v(map, cy1, cy2, cx1);
            carve_h(map, cx1, cx2, cy2);
        }
    }

    /* ── Step 3: place stairs in last room ──────────────── */
    if (room_count > 0) {
        int cx, cy;
        room_center(&rooms[room_count - 1], &cx, &cy);
        map->grid[cy][cx].tile = TILE_STAIRS;
        map->stairs_x = cx;
        map->stairs_y = cy;
    }

    /* ── Step 4: scatter enemies ────────────────────────── */
    int num_enemies = 3 + dungeon_level * 2;
    if (num_enemies > MAX_ENEMIES) num_enemies = MAX_ENEMIES;

    for (int i = 0; i < num_enemies && map->enemy_count < MAX_ENEMIES; i++) {
        /* Pick a random room (not the first one — player spawns there) */
        int ri = 1 + rand() % (room_count - 1 < 1 ? 1 : room_count - 1);
        Room *r = &rooms[ri];
        int ex = r->x + rand() % r->w;
        int ey = r->y + rand() % r->h;
        if (map->grid[ey][ex].tile != TILE_FLOOR) continue;

        /* Enemy type weighted by dungeon level */
        EnemyType etype;
        int roll = rand() % 10;
        if (dungeon_level <= 1)       etype = (roll < 7) ? ENEMY_GOBLIN : ENEMY_SKELETON;
        else if (dungeon_level == 2)  etype = (roll < 5) ? ENEMY_SKELETON : ENEMY_ORC;
        else if (dungeon_level == 3)  etype = (roll < 5) ? ENEMY_ORC : ENEMY_VAMPIRE;
        else                          etype = (roll < 4) ? ENEMY_ORC : ENEMY_VAMPIRE;

        Enemy *e = &map->enemies[map->enemy_count++];
        init_enemy(e, etype, dungeon_level);
        e->x = ex;
        e->y = ey;
        map->grid[ey][ex].tile = TILE_ENEMY;
    }

    /* ── Step 5: place boss on floor 5 ─────────────────── */
    if (dungeon_level >= 5 && room_count >= 2) {
        int ci = room_count / 2;
        int cx, cy;
        room_center(&rooms[ci], &cx, &cy);
        /* Make sure tile is free */
        if (map->grid[cy][cx].tile == TILE_FLOOR) {
            Enemy *boss = &map->enemies[map->enemy_count++];
            init_enemy(boss, ENEMY_DRAGON, dungeon_level);
            boss->x = cx;
            boss->y = cy;
            map->grid[cy][cx].tile = TILE_BOSS;
        }
    }

    /* ── Step 6: scatter treasure ───────────────────────── */
    int num_loot = 2 + rand() % 4;
    for (int i = 0; i < num_loot && map->loot_count < MAX_ITEMS; i++) {
        int ri = rand() % room_count;
        Room *r = &rooms[ri];
        int lx = r->x + rand() % r->w;
        int ly = r->y + rand() % r->h;
        if (map->grid[ly][lx].tile != TILE_FLOOR) continue;

        Item *it = &map->loot[map->loot_count++];
        int lroll = rand() % 4;
        if (lroll == 0) {
            strncpy(it->name, "Health Potion", 32);
            it->type = ITEM_HEALTH_POTION;
            it->value = 30 + rand() % 20;
        } else if (lroll == 1) {
            strncpy(it->name, "Mana Potion", 32);
            it->type = ITEM_MANA_POTION;
            it->value = 20 + rand() % 15;
        } else if (lroll == 2) {
            strncpy(it->name, "Antidote", 32);
            it->type = ITEM_ANTIDOTE;
            it->value = 0;
        } else {
            strncpy(it->name, "Old Weapon", 32);
            it->type = ITEM_WEAPON;
            it->value = 3 + rand() % 5;
        }
        it->quantity = 1;
        map->grid[ly][lx].tile = TILE_TREASURE;
        /* Store position in value fields (hack: reuse struct padding) */
        /* We'll use a parallel loot_pos array trick via index */
        /* Instead, embed position inside Item.name suffix - we handle
           pickup by scanning the map grid instead */
    }
}

/* ─── Field of View ────────────────────────────────────────────────────── */

/* Simple circular FOV: mark tiles within FOG_RADIUS as visible,
   cast rays to detect walls (very lightweight Bresenham FOV) */
void map_update_fov(GameMap *map, int px, int py) {
    /* First reset visibility */
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            map->grid[y][x].visible = 0;

    /* Sweep 360 degrees in small angle increments */
    for (float angle = 0; angle < 6.2832f; angle += 0.05f) {
        float rx = (float)px;
        float ry = (float)py;
        float dx = cosf(angle);
        float dy = sinf(angle);

        for (int r = 0; r < FOG_RADIUS; r++) {
            int ix = (int)rx;
            int iy = (int)ry;
            if (ix < 0 || ix >= MAP_WIDTH || iy < 0 || iy >= MAP_HEIGHT)
                break;
            map->grid[iy][ix].visible  = 1;
            map->grid[iy][ix].explored = 1;
            if (map->grid[iy][ix].tile == TILE_WALL) break;  /* wall blocks ray */
            rx += dx;
            ry += dy;
        }
    }
}

/* ─── Rendering ─────────────────────────────────────────────────────────── */

/* Color each tile type */
static void print_tile(char tile, int visible, int explored) {
    if (!explored) {
        printf(" ");
        return;
    }
    if (!visible) {
        /* Dimmed explored tile */
        printf(COLOR_GRAY);
        printf("%c", tile == TILE_PLAYER ? '.' : tile);
        printf(COLOR_RESET);
        return;
    }

    switch (tile) {
        case TILE_WALL:     printf(COLOR_WHITE "#" COLOR_RESET); break;
        case TILE_FLOOR:    printf(COLOR_GRAY  "." COLOR_RESET); break;
        case TILE_PLAYER:   printf(COLOR_YELLOW "@" COLOR_RESET); break;
        case TILE_ENEMY:    printf(COLOR_RED   "E" COLOR_RESET); break;
        case TILE_BOSS:     printf(COLOR_MAGENTA "B" COLOR_RESET); break;
        case TILE_TREASURE: printf(COLOR_YELLOW "T" COLOR_RESET); break;
        case TILE_STAIRS:   printf(COLOR_CYAN  ">" COLOR_RESET); break;
        default:            printf("%c", tile);                   break;
    }
}

/* Print a bar (HP/MP) as colored blocks */
static void print_bar(int cur, int max, int width, const char *color) {
    int filled = (max > 0) ? (cur * width / max) : 0;
    printf("%s[", color);
    for (int i = 0; i < width; i++)
        printf(i < filled ? "█" : "░");
    printf("]" COLOR_RESET);
}

void map_render(const Game *g) {
    const GameMap *map  = &g->map;
    const Player  *p    = &g->player;

    /* Clear screen */
    printf("\033[H\033[2J");

    /* ── Title bar ─────────────────────────────────── */
    printf(COLOR_CYAN
        "╔════════════════════════════════════════════════════╗\n"
        "║        TERMINAL WARRIOR  v1.0  — Floor %-2d          ║\n"
        "╚════════════════════════════════════════════════════╝\n"
        COLOR_RESET, p->dungeon_level);

    /* ── Map grid ───────────────────────────────────── */
    for (int y = 0; y < MAP_HEIGHT; y++) {
        printf(COLOR_WHITE "│" COLOR_RESET);
        for (int x = 0; x < MAP_WIDTH; x++) {
            /* Player tile overrides */
            if (x == p->x && y == p->y) {
                printf(COLOR_YELLOW "@" COLOR_RESET);
            } else {
                print_tile(map->grid[y][x].tile,
                           map->grid[y][x].visible,
                           map->grid[y][x].explored);
            }
        }
        printf(COLOR_WHITE "│\n" COLOR_RESET);
    }

    /* ── HUD ────────────────────────────────────────── */
    map_print_hud(g);
}

void map_print_hud(const Game *g) {
    const Player *p = &g->player;

    printf(COLOR_CYAN
        "╔════════════════════════════════════════════════════╗\n"
        COLOR_RESET);

    /* HP bar */
    printf(COLOR_CYAN "║ " COLOR_RED "HP ");
    print_bar(p->hp, p->max_hp, 15, COLOR_RED);
    printf(COLOR_RED " %3d/%-3d ", p->hp, p->max_hp);

    /* MP bar */
    printf(COLOR_BLUE "MP ");
    print_bar(p->mp, p->max_mp, 10, COLOR_BLUE);
    printf(COLOR_BLUE " %3d/%-3d", p->mp, p->max_mp);
    printf(COLOR_CYAN " ║\n" COLOR_RESET);

    /* XP bar */
    printf(COLOR_CYAN "║ " COLOR_YELLOW "XP ");
    print_bar(p->xp, p->xp_to_next, 20, COLOR_YELLOW);
    printf(COLOR_YELLOW " Lv%-2d  %s  ", p->level, player_class_name(p->pclass));
    printf(COLOR_CYAN "║\n" COLOR_RESET);

    /* Status and message */
    char status_str[32] = "Normal";
    if (p->status == STATUS_POISON) snprintf(status_str, 32, COLOR_GREEN "POISONED" COLOR_RESET);
    if (p->status == STATUS_STUN)   snprintf(status_str, 32, COLOR_YELLOW "STUNNED" COLOR_RESET);

    printf(COLOR_CYAN "║ " COLOR_WHITE "Status: %-20s" COLOR_CYAN, status_str);
    printf("                    ║\n" COLOR_RESET);

    /* Last message */
    printf(COLOR_CYAN "║ " COLOR_WHITE "%-50.50s" COLOR_CYAN "║\n" COLOR_RESET, g->message);

    printf(COLOR_CYAN
        "║ " COLOR_GRAY "[WASD] Move  [i] Inventory  [s] Stats  [Q] Quit  [S] Save" COLOR_CYAN "  ║\n"
        "╚════════════════════════════════════════════════════╝\n"
        COLOR_RESET);
}

/* ─── Movement ──────────────────────────────────────────────────────────── */

int map_is_walkable(const GameMap *map, int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return 0;
    char t = map->grid[y][x].tile;
    return (t == TILE_FLOOR || t == TILE_TREASURE ||
            t == TILE_STAIRS || t == TILE_ENEMY || t == TILE_BOSS);
}

Enemy *map_enemy_at(GameMap *map, int x, int y) {
    for (int i = 0; i < map->enemy_count; i++)
        if (map->enemies[i].alive && map->enemies[i].x == x && map->enemies[i].y == y)
            return &map->enemies[i];
    return NULL;
}

/* Loot positions: we track by scanning TILE_TREASURE cells against loot array.
   This simple approach stores loot items in sequential order and matches them
   by index when the player steps on a TILE_TREASURE cell. */
static int loot_x[MAX_ITEMS];
static int loot_y[MAX_ITEMS];

/* Called after map_generate to record loot positions */
void map_record_loot_positions(GameMap *map) {
    int idx = 0;
    for (int y = 0; y < MAP_HEIGHT && idx < map->loot_count; y++)
        for (int x = 0; x < MAP_WIDTH && idx < map->loot_count; x++)
            if (map->grid[y][x].tile == TILE_TREASURE) {
                loot_x[idx] = x;
                loot_y[idx] = y;
                idx++;
            }
}

int map_pickup_loot(Game *g, int x, int y, char *msg_buf) {
    GameMap  *map = &g->map;
    Player   *p   = &g->player;

    /* Find loot at this position */
    for (int i = 0; i < map->loot_count; i++) {
        if (loot_x[i] == x && loot_y[i] == y) {
            Item *it = &map->loot[i];
            /* Try to add to inventory */
            int added = 0;
            for (int j = 0; j < MAX_INVENTORY; j++) {
                if (p->inv.slots[j].type == ITEM_NONE ||
                    (p->inv.slots[j].type == it->type &&
                     p->inv.count <= MAX_INVENTORY)) {
                    /* Stack if same type */
                    if (p->inv.slots[j].type == it->type) {
                        p->inv.slots[j].quantity++;
                    } else {
                        p->inv.slots[j] = *it;
                        p->inv.count++;
                    }
                    added = 1;
                    break;
                }
            }
            if (added) {
                if (msg_buf)
                    snprintf(msg_buf, 128, "Picked up: %s", it->name);
                /* Remove from map */
                map->grid[y][x].tile = TILE_FLOOR;
                /* Mark as taken */
                loot_x[i] = -1;
                loot_y[i] = -1;
                return 1;
            } else {
                if (msg_buf)
                    snprintf(msg_buf, 128, "Inventory full!");
                return 0;
            }
        }
    }
    return 0;
}

void map_place_player(Game *g) {
    /* Place player in center of first room */
    if (room_count > 0) {
        int cx, cy;
        room_center(&rooms[0], &cx, &cy);
        g->player.x = cx;
        g->player.y = cy;
    } else {
        g->player.x = 2;
        g->player.y = 2;
    }
    /* Record loot positions after generation */
    map_record_loot_positions(&g->map);
    map_update_fov(&g->map, g->player.x, g->player.y);
}

int map_move_player(Game *g, int dx, int dy) {
    int nx = g->player.x + dx;
    int ny = g->player.y + dy;

    if (!map_is_walkable(&g->map, nx, ny)) return 0;

    /* Check for stairs */
    if (g->map.grid[ny][nx].tile == TILE_STAIRS) {
        snprintf(g->message, 256, "You descend to floor %d!", g->player.dungeon_level + 1);
        g->player.dungeon_level++;
        map_generate(&g->map, g->player.dungeon_level);
        map_place_player(g);
        return 1;
    }

    /* Check for treasure */
    if (g->map.grid[ny][nx].tile == TILE_TREASURE) {
        char pick_msg[128];
        map_pickup_loot(g, nx, ny, pick_msg);
        snprintf(g->message, 256, "%s", pick_msg);
    }

    g->player.x = nx;
    g->player.y = ny;
    map_update_fov(&g->map, nx, ny);
    return 1;
}