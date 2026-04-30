/*
 * player.c - Player management: creation, leveling, status effects
 *
 * Each class has unique base stats that define its playstyle:
 *   Warrior  - High HP/Defense, lower mana; physical powerhouse
 *   Mage     - Low HP, high mana and magic attack; glass cannon
 *   Rogue    - Balanced, high speed and crit; hit-and-run fighter
 */

#include "player.h"
#include <stdio.h>
#include <string.h>

/* ─── Class base stats table ─────────────────────────────────────────────── */
typedef struct { int hp, mp, atk, def, spd; } ClassStats;

static const ClassStats CLASS_TABLE[4] = {
    {0,  0,  0,  0,  0},   /* index 0 unused */
    {120,30, 18,  12, 8},   /* Warrior */
    {60, 100,22,  6,  10},  /* Mage    */
    {85, 50, 20,  8,  14},  /* Rogue   */
};

/* ─────────────────────────────────────────────────────────────────────────── */

void player_create(Player *p, const char *name, PlayerClass cls) {
    memset(p, 0, sizeof(Player));
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->pclass  = cls;
    p->level   = 1;
    p->xp      = 0;
    p->dungeon_level = 1;
    p->status  = STATUS_NONE;

    /* Apply class stats */
    const ClassStats *s = &CLASS_TABLE[(int)cls];
    p->max_hp  = s->hp;
    p->hp      = s->hp;
    p->max_mp  = s->mp;
    p->mp      = s->mp;
    p->attack  = s->atk;
    p->defense = s->def;
    p->speed   = s->spd;

    p->xp_to_next = player_xp_for_level(1);

    /* Starting inventory: one health potion */
    p->inv.count = 1;
    strncpy(p->inv.slots[0].name, "Health Potion", 32);
    p->inv.slots[0].type     = ITEM_HEALTH_POTION;
    p->inv.slots[0].value    = 30;
    p->inv.slots[0].quantity = 2;
}

/* XP thresholds grow exponentially per level */
int player_xp_for_level(int level) {
    return 50 * level * level;
}

void player_gain_xp(Player *p, int xp, char *msg_buf) {
    p->xp += xp;
    if (msg_buf)
        sprintf(msg_buf, "Gained %d XP!", xp);
    player_check_level(p);
}

int player_check_level(Player *p) {
    if (p->xp < p->xp_to_next) return 0;

    p->level++;
    p->xp -= p->xp_to_next;
    p->xp_to_next = player_xp_for_level(p->level);

    /* Stat increases differ by class */
    switch (p->pclass) {
        case CLASS_WARRIOR:
            p->max_hp  += 15;
            p->max_mp  += 3;
            p->attack  += 3;
            p->defense += 3;
            break;
        case CLASS_MAGE:
            p->max_hp  += 6;
            p->max_mp  += 15;
            p->attack  += 5;
            p->defense += 1;
            break;
        case CLASS_ROGUE:
            p->max_hp  += 10;
            p->max_mp  += 5;
            p->attack  += 4;
            p->defense += 2;
            p->speed   += 1;
            break;
    }

    /* Full restore on level up */
    p->hp = p->max_hp;
    p->mp = p->max_mp;
    return 1;
}

void player_heal(Player *p, int amount) {
    p->hp += amount;
    if (p->hp > p->max_hp) p->hp = p->max_hp;
}

void player_restore_mp(Player *p, int amount) {
    p->mp += amount;
    if (p->mp > p->max_mp) p->mp = p->max_mp;
}

void player_apply_status(Player *p, StatusEffect eff, int turns) {
    /* Don't overwrite a longer existing effect */
    if (p->status == eff && p->status_turns > turns) return;
    p->status       = eff;
    p->status_turns = turns;
}

/* Called each combat turn; returns damage taken from poison/stun */
int player_tick_status(Player *p, char *msg_buf) {
    if (p->status == STATUS_NONE) return 0;

    int dmg = 0;
    if (p->status == STATUS_POISON) {
        dmg = 4 + rand() % 4;   /* 4-7 poison damage */
        p->hp -= dmg;
        if (p->hp < 0) p->hp = 0;
        if (msg_buf)
            sprintf(msg_buf, "Poison deals %d damage to you!", dmg);
    } else if (p->status == STATUS_STUN) {
        if (msg_buf)
            sprintf(msg_buf, "You are stunned and cannot act!");
    }

    p->status_turns--;
    if (p->status_turns <= 0) {
        p->status       = STATUS_NONE;
        p->status_turns = 0;
    }
    return dmg;
}

const char *player_class_name(PlayerClass cls) {
    switch (cls) {
        case CLASS_WARRIOR: return "Warrior";
        case CLASS_MAGE:    return "Mage";
        case CLASS_ROGUE:   return "Rogue";
        default:            return "Unknown";
    }
}

void player_class_art(PlayerClass cls) {
    switch (cls) {
        case CLASS_WARRIOR:
            printf(COLOR_CYAN
                "  __|__\n"
                " / o o \\\n"
                "|  ---  |\n"
                " \\ ___ /\n"
                "  |   |\n"
                " /|   |\\\n"
                COLOR_RESET);
            break;
        case CLASS_MAGE:
            printf(COLOR_MAGENTA
                "   /\\\n"
                "  /^^\\\n"
                " / ** \\\n"
                "|  **  |\n"
                " \\    /\n"
                "  |  |\n"
                "  |__|\n"
                COLOR_RESET);
            break;
        case CLASS_ROGUE:
            printf(COLOR_GREEN
                "  _/\\_\n"
                " (o_o)\n"
                "  \\|/\n"
                "  / \\\n"
                " /   \\\n"
                COLOR_RESET);
            break;
    }
}

void player_print_stats(const Player *p) {
    printf(COLOR_CYAN "╔══════════════════════════════╗\n" COLOR_RESET);
    printf(COLOR_CYAN "║" COLOR_WHITE "  %-28s" COLOR_CYAN "║\n" COLOR_RESET, p->name);
    printf(COLOR_CYAN "║" COLOR_YELLOW " Class: %-22s" COLOR_CYAN "║\n" COLOR_RESET,
           player_class_name(p->pclass));
    printf(COLOR_CYAN "║" COLOR_GREEN  " Level: %-22d" COLOR_CYAN "║\n" COLOR_RESET, p->level);
    printf(COLOR_CYAN "║" COLOR_RED    " HP:    %3d / %-16d" COLOR_CYAN "║\n" COLOR_RESET,
           p->hp, p->max_hp);
    printf(COLOR_CYAN "║" COLOR_BLUE   " MP:    %3d / %-16d" COLOR_CYAN "║\n" COLOR_RESET,
           p->mp, p->max_mp);
    printf(COLOR_CYAN "║" COLOR_WHITE  " ATK:   %-22d" COLOR_CYAN "║\n" COLOR_RESET, p->attack);
    printf(COLOR_CYAN "║" COLOR_WHITE  " DEF:   %-22d" COLOR_CYAN "║\n" COLOR_RESET, p->defense);
    printf(COLOR_CYAN "║" COLOR_WHITE  " SPD:   %-22d" COLOR_CYAN "║\n" COLOR_RESET, p->speed);
    printf(COLOR_CYAN "║" COLOR_YELLOW " XP:    %d / %-17d" COLOR_CYAN "║\n" COLOR_RESET,
           p->xp, p->xp_to_next);
    printf(COLOR_CYAN "║" COLOR_ORANGE " Floor: %-22d" COLOR_CYAN "║\n" COLOR_RESET,
           p->dungeon_level);
    printf(COLOR_CYAN "╚══════════════════════════════╝\n" COLOR_RESET);
}