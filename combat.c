/*
 * combat.c - Turn-based combat system
 *
 * Combat flow per round:
 *   1. Player chooses action (Attack / Defend / Item / Special / Flee)
 *   2. Status effects tick (poison/stun apply)
 *   3. Player acts (unless stunned)
 *   4. Enemy acts (basic AI: attack, or use special if low HP)
 *   5. Check win/lose
 *
 * Critical hits: every attack has a crit_chance% chance of dealing
 * 1.5× damage. Rogue has higher base crit chance.
 *
 * Enemy AI:
 *   - Goblin/Skeleton: always attack
 *   - Orc: 30% chance to defend when HP < 50%
 *   - Vampire: 25% chance to drain (heals self)
 *   - Dragon (Boss): rotates through Bite, Firebreath, Tail Sweep
 */

#define _POSIX_C_SOURCE 200809L
#include "combat.h"
#include "inventory.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ─── ASCII art for enemies ────────────────────────────────────────────── */

void combat_enemy_art(EnemyType type) {
    switch (type) {
        case ENEMY_GOBLIN:
            printf(COLOR_GREEN
                "   .--.\n"
                "  /o  o\\\n"
                " | >--< |\n"
                "  \\    /\n"
                "  /|  |\\\n"
                COLOR_RESET);
            break;
        case ENEMY_SKELETON:
            printf(COLOR_GRAY
                "   _____\n"
                "  |o   o|\n"
                "  | --- |\n"
                "  |_____|\n"
                "    | |\n"
                "   /| |\\\n"
                COLOR_RESET);
            break;
        case ENEMY_ORC:
            printf(COLOR_GREEN
                "  /------\\\n"
                " | OO  OO |\n"
                " |   WW   |\n"
                " |  ||||  |\n"
                "  \\------/\n"
                "   ||  ||\n"
                COLOR_RESET);
            break;
        case ENEMY_VAMPIRE:
            printf(COLOR_MAGENTA
                "  /\\  /\\\n"
                " (  \\/  )\n"
                " | o  o |\n"
                " |  /\\  |\n"
                " ( |  | )\n"
                "  \\|  |/\n"
                COLOR_RESET);
            break;
        case ENEMY_DRAGON:
            printf(COLOR_RED
                "    /\\   /\\\n"
                "   /  \\_/  \\\n"
                "  | * Boss* |\n"
                "  (  O   O  )\n"
                "   \\  ~~~  /\n"
                "  //|     |\\\\\n"
                " // |_____| \\\\\n"
                "    DRAGON!\n"
                COLOR_RESET);
            break;
    }
}

/* ─── Combat log ───────────────────────────────────────────────────────── */

void combat_log(Game *g, const char *msg) {
    /* Shift log entries up */
    for (int i = 4; i > 0; i--)
        strncpy(g->combat_log[i], g->combat_log[i-1], 128);
    strncpy(g->combat_log[0], msg, 128);
    if (g->log_count < 5) g->log_count++;
}

/* ─── Combat UI ────────────────────────────────────────────────────────── */

/* Draw a colored HP bar */
static void draw_bar(int cur, int max, int width, const char *color, const char *label) {
    int filled = (max > 0) ? (cur * width / max) : 0;
    printf("  %s%-6s [", color, label);
    for (int i = 0; i < width; i++)
        printf(i < filled ? "█" : "░");
    printf("] %d/%d" COLOR_RESET "\n", cur, max);
}

void combat_render(const Game *g, const Enemy *e, const char *last_log) {
    const Player *p = &g->player;

    printf("\033[H\033[2J");
    printf(COLOR_RED
        "╔══════════════════════════════════════════════════════╗\n"
        "║                 ⚔  COMBAT  ⚔                        ║\n"
        "╚══════════════════════════════════════════════════════╝\n"
        COLOR_RESET);

    /* Enemy section */
    printf(COLOR_RED "  Enemy: %s\n" COLOR_RESET, e->name);
    if (e->is_boss) printf(COLOR_MAGENTA "  *** BOSS FIGHT! ***\n" COLOR_RESET);
    combat_enemy_art(e->type);
    draw_bar(e->hp, e->max_hp, 20, COLOR_RED, "HP");

    /* Status */
    if (e->status == STATUS_POISON)
        printf(COLOR_GREEN "  Status: POISONED (%d turns)\n" COLOR_RESET, e->status_turns);
    if (e->status == STATUS_STUN)
        printf(COLOR_YELLOW "  Status: STUNNED (%d turns)\n" COLOR_RESET, e->status_turns);

    printf("\n");

    /* Player section */
    printf(COLOR_CYAN "  %s [%s] Lv.%d\n" COLOR_RESET,
           p->name, player_class_name(p->pclass), p->level);
    draw_bar(p->hp, p->max_hp, 20, COLOR_GREEN, "HP");
    draw_bar(p->mp, p->max_mp, 20, COLOR_BLUE,  "MP");

    if (p->status == STATUS_POISON)
        printf(COLOR_GREEN "  Status: POISONED (%d turns)\n" COLOR_RESET, p->status_turns);
    if (p->status == STATUS_STUN)
        printf(COLOR_YELLOW "  Status: STUNNED (%d turns)\n" COLOR_RESET, p->status_turns);
    if (p->status & STATUS_DEFEND)
        printf(COLOR_CYAN "  Status: DEFENDING\n" COLOR_RESET);

    /* Combat log */
    printf(COLOR_YELLOW "\n  ─── Battle Log ───────────────────────────────\n" COLOR_RESET);
    for (int i = g->log_count - 1; i >= 0; i--)
        printf("  %s%s\n" COLOR_RESET, i == 0 ? COLOR_WHITE : COLOR_GRAY, g->combat_log[i]);

    /* Action menu */
    printf(COLOR_CYAN
        "\n  ╔════════════════════════════╗\n"
        "  ║  [1] Attack                ║\n"
        "  ║  [2] Defend                ║\n"
        "  ║  [3] Use Item              ║\n"
        "  ║  [4] Special Ability       ║\n"
        "  ║  [5] Flee                  ║\n"
        "  ╚════════════════════════════╝\n"
        "  Choice: " COLOR_RESET);
    fflush(stdout);
}

/* ─── Attack resolution ────────────────────────────────────────────────── */

/* Crit chance varies by class */
static int crit_chance(const Player *p) {
    switch (p->pclass) {
        case CLASS_ROGUE:   return 25;   /* 25% crit */
        case CLASS_WARRIOR: return 10;
        case CLASS_MAGE:    return 5;
        default:            return 10;
    }
}

int combat_player_attack(Player *p, Enemy *e, char *log_buf) {
    int base_dmg = p->attack - e->defense / 2;
    if (base_dmg < 1) base_dmg = 1;

    /* Add randomness ±20% */
    int variance = base_dmg / 5 + 1;
    base_dmg += (rand() % (variance * 2 + 1)) - variance;

    /* Critical hit check */
    int is_crit = (rand() % 100) < crit_chance(p);
    if (is_crit) base_dmg = base_dmg * 3 / 2;

    /* Poison attack for Rogue: 20% chance to poison */
    int poisoned = 0;
    if (p->pclass == CLASS_ROGUE && rand() % 100 < 20) {
        e->status       = STATUS_POISON;
        e->status_turns = 3;
        poisoned = 1;
    }

    e->hp -= base_dmg;
    if (e->hp < 0) e->hp = 0;

    if (log_buf) {
        if (is_crit)
            snprintf(log_buf, 128,
                COLOR_YELLOW "CRITICAL! " COLOR_WHITE "You dealt %d damage!%s" COLOR_RESET,
                base_dmg, poisoned ? " (Poisoned!)" : "");
        else
            snprintf(log_buf, 128,
                COLOR_WHITE "You attack for %d damage!%s" COLOR_RESET,
                base_dmg, poisoned ? " (Poisoned!)" : "");
    }
    return base_dmg;
}

/* ─── Special abilities ─────────────────────────────────────────────────── */

int combat_player_special(Player *p, Enemy *e, char *log_buf) {
    switch (p->pclass) {
        case CLASS_WARRIOR: {
            /* Shield Bash: 20 MP, deals 2× attack damage + stuns */
            if (p->mp < 20) {
                if (log_buf) snprintf(log_buf, 128, COLOR_RED "Not enough MP! (need 20)" COLOR_RESET);
                return 0;
            }
            p->mp -= 20;
            int dmg = p->attack * 2 - e->defense;
            if (dmg < 1) dmg = 1;
            e->hp -= dmg;
            e->status       = STATUS_STUN;
            e->status_turns = 1;
            if (log_buf)
                snprintf(log_buf, 128,
                    COLOR_CYAN "Shield Bash! %d damage + Enemy STUNNED!" COLOR_RESET, dmg);
            break;
        }
        case CLASS_MAGE: {
            /* Fireball: 30 MP, deals 3× magic damage, ignores defense */
            if (p->mp < 30) {
                if (log_buf) snprintf(log_buf, 128, COLOR_RED "Not enough MP! (need 30)" COLOR_RESET);
                return 0;
            }
            p->mp -= 30;
            int dmg = p->attack * 3;
            if (rand() % 100 < 20) dmg = dmg * 2;  /* 20% chance double-cast */
            e->hp -= dmg;
            if (log_buf)
                snprintf(log_buf, 128,
                    COLOR_RED "FIREBALL! %d magic damage!" COLOR_RESET, dmg);
            break;
        }
        case CLASS_ROGUE: {
            /* Shadow Strike: 15 MP, always crits, poisons */
            if (p->mp < 15) {
                if (log_buf) snprintf(log_buf, 128, COLOR_RED "Not enough MP! (need 15)" COLOR_RESET);
                return 0;
            }
            p->mp -= 15;
            int dmg = p->attack * 2;
            e->hp -= dmg;
            e->status       = STATUS_POISON;
            e->status_turns = 4;
            if (log_buf)
                snprintf(log_buf, 128,
                    COLOR_GREEN "Shadow Strike! %d damage + 4-turn POISON!" COLOR_RESET, dmg);
            break;
        }
    }
    if (e->hp < 0) e->hp = 0;
    return 1;
}

/* ─── Enemy AI ──────────────────────────────────────────────────────────── */

/* Enemy status tick: returns damage dealt to enemy by its own poison */
static int enemy_tick_status(Enemy *e, char *log_buf) {
    if (e->status == STATUS_NONE) return 0;
    int dmg = 0;
    if (e->status == STATUS_POISON) {
        dmg = 3 + rand() % 4;
        e->hp -= dmg;
        if (e->hp < 0) e->hp = 0;
        if (log_buf)
            snprintf(log_buf, 128,
                COLOR_GREEN "Poison deals %d damage to %s!" COLOR_RESET, dmg, e->name);
    }
    e->status_turns--;
    if (e->status_turns <= 0) e->status = STATUS_NONE;
    return dmg;
}

/* Dragon boss move rotation */
static int dragon_move = 0;

void combat_enemy_turn(Enemy *e, Player *p, char *log_buf) {
    /* Stunned? Skip turn */
    if (e->status == STATUS_STUN) {
        e->status_turns--;
        if (e->status_turns <= 0) e->status = STATUS_NONE;
        if (log_buf) snprintf(log_buf, 128,
            COLOR_YELLOW "%s is stunned and cannot act!" COLOR_RESET, e->name);
        return;
    }

    /* Boss special logic */
    if (e->is_boss) {
        dragon_move = (dragon_move + 1) % 3;
        switch (dragon_move) {
            case 0: {
                /* Bite: heavy physical */
                int dmg = e->attack - p->defense / 2;
                if (dmg < 1) dmg = 1;
                if (p->status & STATUS_DEFEND) dmg /= 2;
                p->hp -= dmg;
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_RED "Dragon BITES for %d damage!" COLOR_RESET, dmg);
                break;
            }
            case 1: {
                /* Firebreath: ignores defense, poisons */
                int dmg = e->attack * 2;
                p->hp -= dmg;
                player_apply_status(p, STATUS_POISON, 3);
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_RED "Dragon FIREBREATHS! %d damage + POISON!" COLOR_RESET, dmg);
                break;
            }
            case 2: {
                /* Tail Sweep: lower damage but stuns */
                int dmg = e->attack / 2;
                if (dmg < 1) dmg = 1;
                p->hp -= dmg;
                player_apply_status(p, STATUS_STUN, 1);
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_RED "Dragon TAIL SWEEPS! %d damage + STUN!" COLOR_RESET, dmg);
                break;
            }
        }
        if (p->hp < 0) p->hp = 0;
        return;
    }

    /* Normal enemy AI */
    switch (e->type) {
        case ENEMY_GOBLIN:
        case ENEMY_SKELETON: {
            /* Always attack */
            int dmg = e->attack - p->defense / 2;
            if (p->status & STATUS_DEFEND) dmg = dmg / 2;
            if (dmg < 1) dmg = 1;
            p->hp -= dmg;
            if (log_buf) snprintf(log_buf, 128,
                COLOR_RED "%s attacks for %d damage!" COLOR_RESET, e->name, dmg);
            break;
        }
        case ENEMY_ORC: {
            /* 30% defend if HP < 50%, otherwise heavy attack */
            if (e->hp < e->max_hp / 2 && rand() % 10 < 3) {
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_CYAN "%s braces for impact!" COLOR_RESET, e->name);
                e->defense += 5;  /* temporary */
            } else {
                int dmg = e->attack * 3 / 2 - p->defense / 2;
                if (p->status & STATUS_DEFEND) dmg /= 2;
                if (dmg < 1) dmg = 1;
                p->hp -= dmg;
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_RED "%s smashes for %d damage!" COLOR_RESET, e->name, dmg);
            }
            break;
        }
        case ENEMY_VAMPIRE: {
            /* 25% chance to drain (heals self), else bite */
            if (rand() % 4 == 0) {
                int drain = e->attack / 2;
                p->hp  -= drain;
                e->hp  += drain;
                if (e->hp > e->max_hp) e->hp = e->max_hp;
                if (p->hp < 0) p->hp = 0;
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_MAGENTA "%s DRAINS %d life from you!" COLOR_RESET, e->name, drain);
            } else {
                int dmg = e->attack - p->defense / 2;
                if (p->status & STATUS_DEFEND) dmg /= 2;
                if (dmg < 1) dmg = 1;
                p->hp -= dmg;
                /* Small chance to poison */
                if (rand() % 5 == 0)
                    player_apply_status(p, STATUS_POISON, 2);
                if (log_buf) snprintf(log_buf, 128,
                    COLOR_RED "%s bites for %d damage!" COLOR_RESET, e->name, dmg);
            }
            break;
        }
        default:
            break;
    }
}

/* ─── Main combat loop ──────────────────────────────────────────────────── */

/* Read a single keypress without echo */
static char read_key(void) {
    struct termios old, raw;
    tcgetattr(0, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &raw);
    char c = getchar();
    tcsetattr(0, TCSANOW, &old);
    return c;
}

int combat_start(Game *g, Enemy *enemy) {
    char log_buf[128];
    g->log_count = 0;
    memset(g->combat_log, 0, sizeof(g->combat_log));
    dragon_move = 0;

    combat_log(g, "A battle begins!");

    while (enemy->alive && g->player.hp > 0) {

        combat_render(g, enemy, g->combat_log[0]);

        /* ── Player turn status tick ── */
        if (g->player.status != STATUS_NONE) {
            int stun_active = (g->player.status == STATUS_STUN);
            player_tick_status(&g->player, log_buf);
            combat_log(g, log_buf);

            if (g->player.hp <= 0) break;
            if (stun_active) {
                /* Enemy still gets a turn */
                enemy_tick_status(enemy, log_buf);
                if (strlen(log_buf) > 0) combat_log(g, log_buf);
                combat_enemy_turn(enemy, &g->player, log_buf);
                combat_log(g, log_buf);
                continue;
            }
        }

        /* ── Get player action ── */
        char key = read_key();
        CombatAction action = (CombatAction)(key - '0');

        /* Reset defend bonus each round */
        g->player.status &= ~STATUS_DEFEND;
        g->player.defend_bonus = 0;

        switch (action) {
            case ACTION_ATTACK:
                combat_player_attack(&g->player, enemy, log_buf);
                combat_log(g, log_buf);
                break;

            case ACTION_DEFEND:
                g->player.status |= STATUS_DEFEND;
                g->player.defend_bonus = g->player.defense;
                snprintf(log_buf, 128,
                    COLOR_CYAN "You brace! Defense doubled this turn." COLOR_RESET);
                combat_log(g, log_buf);
                break;

            case ACTION_ITEM: {
                /* Show mini inventory */
                printf("\n  " COLOR_YELLOW "── Items ──────────────────────────\n" COLOR_RESET);
                int shown = 0;
                for (int i = 0; i < MAX_INVENTORY; i++) {
                    Item *it = &g->player.inv.slots[i];
                    if (it->type != ITEM_NONE && it->quantity > 0) {
                        printf("  [%d] %s x%d\n", i + 1, it->name, it->quantity);
                        shown++;
                    }
                }
                if (!shown) {
                    printf("  No usable items!\n");
                    combat_log(g, "No items to use.");
                    break;
                }
                printf("  Choice (1-9, 0=cancel): ");
                fflush(stdout);
                char ic = read_key();
                int idx = ic - '1';
                if (idx >= 0 && idx < MAX_INVENTORY) {
                    inv_use(g, idx, log_buf);
                    combat_log(g, log_buf);
                } else {
                    combat_log(g, "Cancelled.");
                }
                break;
            }

            case ACTION_SPECIAL:
                if (!combat_player_special(&g->player, enemy, log_buf)) {
                    combat_log(g, log_buf);
                    continue;   /* Don't advance turn if special failed */
                }
                combat_log(g, log_buf);
                break;

            case ACTION_FLEE: {
                /* Flee success depends on player speed vs enemy level */
                int flee_chance = 40 + g->player.speed * 3;
                if (rand() % 100 < flee_chance) {
                    combat_log(g, "You flee from battle!");
                    /* Restore some HP on flee */
                    snprintf(g->message, 256, "You fled from %s!", enemy->name);
                    return -1;   /* -1 = fled */
                } else {
                    combat_log(g, "Couldn't escape!");
                }
                break;
            }

            default:
                combat_log(g, "Invalid choice.");
                continue;
        }

        /* Check if enemy is dead */
        if (enemy->hp <= 0) {
            enemy->alive = 0;
            break;
        }

        /* ── Enemy status tick ── */
        enemy_tick_status(enemy, log_buf);
        if (strlen(log_buf) > 0) combat_log(g, log_buf);
        if (enemy->hp <= 0) { enemy->alive = 0; break; }

        /* ── Enemy turn ── */
        combat_enemy_turn(enemy, &g->player, log_buf);
        combat_log(g, log_buf);

        if (g->player.hp <= 0) break;
    }

    /* ── Combat result ── */
    if (g->player.hp <= 0) {
        printf("\033[H\033[2J");
        printf(COLOR_RED
            "\n\n"
            "  ██████╗ ██╗███████╗\n"
            "  ██╔══██╗██║██╔════╝\n"
            "  ██║  ██║██║█████╗  \n"
            "  ██║  ██║██║██╔══╝  \n"
            "  ██████╔╝██║███████╗\n"
            "  ╚═════╝ ╚═╝╚══════╝\n"
            "\n  You have been slain by %s...\n\n" COLOR_RESET,
            enemy->name);
        sleep(2);   /* pause 2s on death screen */
        return 0;
    }

    /* Victory */
    printf("\033[H\033[2J");
    printf(COLOR_YELLOW
        "\n\n"
        "  ██╗   ██╗██╗ ██████╗████████╗ ██████╗ ██████╗ ██╗\n"
        "  ██║   ██║██║██╔════╝╚══██╔══╝██╔═══██╗██╔══██╗██║\n"
        "  ██║   ██║██║██║        ██║   ██║   ██║██████╔╝██║\n"
        "  ╚██╗ ██╔╝██║██║        ██║   ██║   ██║██╔══██╗╚═╝\n"
        "   ╚████╔╝ ██║╚██████╗   ██║   ╚██████╔╝██║  ██║██╗\n"
        "    ╚═══╝  ╚═╝ ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚═╝\n"
        "\n  You defeated: %s!\n\n" COLOR_RESET, enemy->name);

    /* Reward XP */
    char xp_msg[128];
    player_gain_xp(&g->player, enemy->xp_reward, xp_msg);
    printf("  %s\n", xp_msg);

    /* Check level up */
    if (player_check_level(&g->player)) {
        printf(COLOR_YELLOW "\n  ★ LEVEL UP! You are now level %d! ★\n" COLOR_RESET,
               g->player.level);
    }

    /* Random loot drop */
    int loot_roll = rand() % 100;
    if (loot_roll < 40 && g->player.inv.count < MAX_INVENTORY) {
        /* Find an empty inventory slot */
        for (int i = 0; i < MAX_INVENTORY; i++) {
            if (g->player.inv.slots[i].type == ITEM_NONE) {
                Item *drop = &g->player.inv.slots[i];
                if (loot_roll < 20) {
                    strncpy(drop->name, "Health Potion", 32);
                    drop->type = ITEM_HEALTH_POTION;
                    drop->value = 30;
                    drop->quantity = 1;
                    printf("  " COLOR_GREEN "Loot: Health Potion dropped!\n" COLOR_RESET);
                } else {
                    strncpy(drop->name, "Mana Potion", 32);
                    drop->type = ITEM_MANA_POTION;
                    drop->value = 25;
                    drop->quantity = 1;
                    printf("  " COLOR_BLUE "Loot: Mana Potion dropped!\n" COLOR_RESET);
                }
                g->player.inv.count++;
                break;
            }
        }
    }

    printf("\n  Press any key to continue...\n");
    fflush(stdout);
    read_key();
    return 1;
}