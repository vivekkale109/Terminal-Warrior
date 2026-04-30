/*
 * save.c - Binary save / load using a magic-number header.
 *
 * Format:
 *   [4 bytes] magic  = 0x54574152  ("TWAR")
 *   [4 bytes] version = 1
 *   [sizeof(Game)] raw game struct
 *
 * Writing/reading the whole Game struct keeps things simple and
 * guarantees all fields (map, inventory, enemy states) are preserved.
 * The downside is saves are tied to the current struct layout; a
 * version mismatch aborts the load and warns the player.
 */

#include "save.h"
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC   0x54574152u   /* "TWAR" */
#define SAVE_VERSION 1u

/* ─────────────────────────────────────────────────────────────────────────── */

int save_game(const Game *g) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) {
        perror("save_game: fopen");
        return 0;
    }

    unsigned int magic   = SAVE_MAGIC;
    unsigned int version = SAVE_VERSION;

    fwrite(&magic,   sizeof(magic),   1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(g,        sizeof(Game),    1, f);

    fclose(f);
    return 1;
}

int load_game(Game *g) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return 0;

    unsigned int magic   = 0;
    unsigned int version = 0;

    fread(&magic,   sizeof(magic),   1, f);
    fread(&version, sizeof(version), 1, f);

    if (magic != SAVE_MAGIC) {
        printf(COLOR_RED "  Save file is corrupted (bad magic). Starting fresh.\n"
               COLOR_RESET);
        fclose(f);
        return 0;
    }
    if (version != SAVE_VERSION) {
        printf(COLOR_YELLOW "  Save file version mismatch (%u vs %u). "
               "Cannot load.\n" COLOR_RESET, version, SAVE_VERSION);
        fclose(f);
        return 0;
    }

    size_t n = fread(g, sizeof(Game), 1, f);
    fclose(f);

    if (n != 1) {
        printf(COLOR_RED "  Save file is incomplete. Starting fresh.\n" COLOR_RESET);
        return 0;
    }

    /* After load, make sure we're in playing state */
    g->state = STATE_PLAYING;
    return 1;
}

int save_exists(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void save_delete(void) {
    remove(SAVE_FILE);
}

int save_menu(Game *g) {
    printf("\033[H\033[2J");
    printf(COLOR_CYAN
           "╔══════════════════════════════╗\n"
           "║       SAVE / LOAD MENU       ║\n"
           "╠══════════════════════════════╣\n"
           "║  [1] Save Game               ║\n"
           "║  [2] Load Game               ║\n"
           "║  [0] Cancel                  ║\n"
           "╚══════════════════════════════╝\n"
           COLOR_RESET);

    if (save_exists())
        printf(COLOR_GREEN "  (A save file exists)\n" COLOR_RESET);
    else
        printf(COLOR_GRAY  "  (No save file found)\n" COLOR_RESET);

    printf("\nChoice: ");
    fflush(stdout);

    char buf[8];
    if (!fgets(buf, sizeof(buf), stdin)) return 0;

    switch (buf[0]) {
        case '1':
            if (save_game(g)) {
                printf(COLOR_GREEN "\n  Game saved successfully!\n" COLOR_RESET);
            } else {
                printf(COLOR_RED "\n  Save FAILED.\n" COLOR_RESET);
            }
            printf("  Press Enter...");
            fflush(stdout);
            fgets(buf, sizeof(buf), stdin);
            return 1;

        case '2':
            if (load_game(g)) {
                printf(COLOR_GREEN "\n  Game loaded!\n" COLOR_RESET);
                printf("  Press Enter...");
                fflush(stdout);
                fgets(buf, sizeof(buf), stdin);
                return 2;
            } else {
                printf(COLOR_RED "\n  Load FAILED.\n" COLOR_RESET);
                printf("  Press Enter...");
                fflush(stdout);
                fgets(buf, sizeof(buf), stdin);
                return 0;
            }

        default:
            return 0;
    }
}