/*
 * inventory.c - Inventory management system
 *
 * Items are stored in a flat array with quantity stacking.
 * Using an item during combat (via inv_use) applies its effect
 * immediately and removes one unit from the stack.
 */

#include "inventory.h"
#include "player.h"
#include <stdio.h>
#include <string.h>

/* ─── Internal helper: item type colour ─────────────────────────────────── */
static const char *item_colour(ItemType t) {
    switch (t) {
        case ITEM_HEALTH_POTION: return COLOR_RED;
        case ITEM_MANA_POTION:   return COLOR_BLUE;
        case ITEM_WEAPON:        return COLOR_YELLOW;
        case ITEM_ARMOR:         return COLOR_CYAN;
        case ITEM_ANTIDOTE:      return COLOR_GREEN;
        default:                 return COLOR_WHITE;
    }
}

/* ─── Internal helper: item type label ──────────────────────────────────── */
static const char *item_type_label(ItemType t) {
    switch (t) {
        case ITEM_HEALTH_POTION: return "[Potion]";
        case ITEM_MANA_POTION:   return "[Mana]  ";
        case ITEM_WEAPON:        return "[Weapon]";
        case ITEM_ARMOR:         return "[Armor] ";
        case ITEM_ANTIDOTE:      return "[Cure]  ";
        default:                 return "[???]   ";
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */

void inv_show(Game *g) {
    Inventory *inv = &g->player.inv;

    printf("\033[H\033[2J");   /* clear screen */
    printf(COLOR_YELLOW
           "╔══════════════════════════════════════════╗\n"
           "║           INVENTORY                      ║\n"
           "╠══════════════════════════════════════════╣\n"
           COLOR_RESET);

    if (inv->count == 0) {
        printf(COLOR_GRAY "║  (empty)                                 ║\n" COLOR_RESET);
    } else {
        for (int i = 0; i < MAX_INVENTORY; i++) {
            Item *it = &inv->slots[i];
            if (it->type == ITEM_NONE) continue;
            printf(COLOR_YELLOW "║" COLOR_RESET);
            printf(" %s%d%s. %s%-14s%s %s x%-3d  Effect: +%d  ",
                   COLOR_WHITE, i + 1, COLOR_RESET,
                   item_colour(it->type), it->name, COLOR_RESET,
                   item_type_label(it->type),
                   it->quantity,
                   it->value);
            printf(COLOR_YELLOW "║\n" COLOR_RESET);
        }
    }

    printf(COLOR_YELLOW
           "╠══════════════════════════════════════════╣\n"
           "║  [U] Use item   [ESC/Q] Close            ║\n"
           "╚══════════════════════════════════════════╝\n"
           COLOR_RESET);

    printf("\nEnter slot number to use (or Q to close): ");
    fflush(stdout);

    char buf[8];
    if (fgets(buf, sizeof(buf), stdin)) {
        if (buf[0] >= '1' && buf[0] <= '9') {
            int slot = buf[0] - '1';
            char msg[128] = {0};
            if (inv_use(g, slot, msg)) {
                printf(COLOR_GREEN "\n  %s\n" COLOR_RESET, msg);
            } else {
                printf(COLOR_RED "\n  %s\n" COLOR_RESET,
                       msg[0] ? msg : "Nothing happened.");
            }
            printf("  Press Enter...");
            fflush(stdout);
            fgets(buf, sizeof(buf), stdin);
        }
    }
}

int inv_use(Game *g, int slot, char *msg_buf) {
    if (slot < 0 || slot >= MAX_INVENTORY) {
        if (msg_buf) strcpy(msg_buf, "Invalid slot.");
        return 0;
    }

    Item *it = &g->player.inv.slots[slot];
    if (it->type == ITEM_NONE || it->quantity <= 0) {
        if (msg_buf) strcpy(msg_buf, "That slot is empty.");
        return 0;
    }

    Player *p = &g->player;

    switch (it->type) {
        case ITEM_HEALTH_POTION:
            if (p->hp >= p->max_hp) {
                if (msg_buf) strcpy(msg_buf, "HP is already full!");
                return 0;
            }
            player_heal(p, it->value);
            if (msg_buf)
                sprintf(msg_buf, "Used %s: restored %d HP. HP: %d/%d",
                        it->name, it->value, p->hp, p->max_hp);
            break;

        case ITEM_MANA_POTION:
            if (p->mp >= p->max_mp) {
                if (msg_buf) strcpy(msg_buf, "MP is already full!");
                return 0;
            }
            player_restore_mp(p, it->value);
            if (msg_buf)
                sprintf(msg_buf, "Used %s: restored %d MP. MP: %d/%d",
                        it->name, it->value, p->mp, p->max_mp);
            break;

        case ITEM_WEAPON:
            /* Equipping a weapon permanently boosts attack */
            p->attack += it->value;
            if (msg_buf)
                sprintf(msg_buf, "Equipped %s: +%d ATK permanently!",
                        it->name, it->value);
            break;

        case ITEM_ARMOR:
            /* Equipping armor permanently boosts defense */
            p->defense += it->value;
            if (msg_buf)
                sprintf(msg_buf, "Equipped %s: +%d DEF permanently!",
                        it->name, it->value);
            break;

        case ITEM_ANTIDOTE:
            if (p->status == STATUS_NONE) {
                if (msg_buf) strcpy(msg_buf, "You are not afflicted by any status!");
                return 0;
            }
            p->status       = STATUS_NONE;
            p->status_turns = 0;
            if (msg_buf) sprintf(msg_buf, "Used %s: status cleared!", it->name);
            break;

        default:
            if (msg_buf) strcpy(msg_buf, "Cannot use that item.");
            return 0;
    }

    /* Consume one unit */
    inv_remove(&p->inv, slot);
    return 1;
}

int inv_add(Inventory *inv, const Item *item) {
    /* First: try to stack on an identical item */
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv->slots[i].type == item->type &&
            strcmp(inv->slots[i].name, item->name) == 0) {
            inv->slots[i].quantity += item->quantity;
            return 1;
        }
    }
    /* Second: find an empty slot */
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv->slots[i].type == ITEM_NONE) {
            inv->slots[i] = *item;
            inv->count++;
            return 1;
        }
    }
    return 0;   /* full */
}

int inv_remove(Inventory *inv, int slot) {
    if (slot < 0 || slot >= MAX_INVENTORY) return 0;
    Item *it = &inv->slots[slot];
    if (it->type == ITEM_NONE) return 0;

    it->quantity--;
    if (it->quantity <= 0) {
        memset(it, 0, sizeof(Item));
        inv->count = 0;
        /* Recount */
        for (int i = 0; i < MAX_INVENTORY; i++)
            if (inv->slots[i].type != ITEM_NONE) inv->count++;
    }
    return 1;
}

void inv_print_short(const Inventory *inv) {
    printf(COLOR_YELLOW "  Inventory: " COLOR_RESET);
    int printed = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        const Item *it = &inv->slots[i];
        if (it->type == ITEM_NONE) continue;
        printf("%s%s x%d  " COLOR_RESET,
               item_colour(it->type), it->name, it->quantity);
        printed++;
        if (printed >= 4) { printf("..."); break; }
    }
    if (printed == 0) printf(COLOR_GRAY "(empty)" COLOR_RESET);
    printf("\n");
}

int inv_count_usable(const Inventory *inv) {
    int c = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        const Item *it = &inv->slots[i];
        if (it->type == ITEM_HEALTH_POTION ||
            it->type == ITEM_MANA_POTION   ||
            it->type == ITEM_ANTIDOTE)
            c += it->quantity;
    }
    return c;
}