/*
 * main.c - Terminal Warrior: Entry point and main game loop
 *
 * Architecture overview:
 *   main()            - program entry, title screen, new/load choice
 *   game_new()        - initialise Game struct, create player, generate map
 *   game_loop()       - blocking input loop: movement, events, combat
 *   game_title()      - ASCII art title screen
 *   game_win_screen() - victory screen after killing the dragon boss
 *   game_over()       - death screen
 *
 * Input uses raw terminal mode (termios) so WASD work without Enter.
 * We restore the terminal to cooked mode before exiting in all paths.
 *
 * Compile: see Makefile  (gcc -o terminal_warrior *.c -lm)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>

#include "types.h"
#include "player.h"
#include "map.h"
#include "combat.h"
#include "inventory.h"
#include "save.h"

/* ─────────────────────────────────────────────────────────────────────────── */
/* Terminal raw-mode helpers                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

static struct termios g_orig_termios;

/* Switch terminal to raw mode: no line-buffering, no echo */
static void enable_raw_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* Restore terminal to its original cooked mode */
static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

/* Read a single character in raw mode */
static char read_raw_key(void) {
    enable_raw_mode();
    char c = (char)getchar();
    disable_raw_mode();
    return c;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Helper: press-any-key prompt in cooked mode                                 */
/* ─────────────────────────────────────────────────────────────────────────── */
static void press_any_key(void) {
    printf("\n  Press any key to continue...");
    fflush(stdout);
    read_raw_key();
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* ASCII art title screen                                                       */
/* ─────────────────────────────────────────────────────────────────────────── */
static void game_title(void) {
    printf("\033[H\033[2J");  /* clear screen */
    printf(COLOR_RED
        "\n"
        "  ████████╗███████╗██████╗ ███╗   ███╗██╗███╗   ██╗ █████╗ ██╗     \n"
        "     ██╔══╝██╔════╝██╔══██╗████╗ ████║██║████╗  ██║██╔══██╗██║     \n"
        "     ██║   █████╗  ██████╔╝██╔████╔██║██║██╔██╗ ██║███████║██║     \n"
        "     ██║   ██╔══╝  ██╔══██╗██║╚██╔╝██║██║██║╚██╗██║██╔══██║██║     \n"
        "     ██║   ███████╗██║  ██║██║ ╚═╝ ██║██║██║ ╚████║██║  ██║███████╗\n"
        "     ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝\n"
        COLOR_RESET
        COLOR_YELLOW
        "\n"
        "  ██╗    ██╗ █████╗ ██████╗ ██████╗ ██╗ ██████╗ ██████╗ \n"
        "  ██║    ██║██╔══██╗██╔══██╗██╔══██╗██║██╔═══██╗██╔══██╗\n"
        "  ██║ █╗ ██║███████║██████╔╝██████╔╝██║██║   ██║██████╔╝\n"
        "  ██║███╗██║██╔══██║██╔══██╗██╔══██╗██║██║   ██║██╔══██╗\n"
        "  ╚███╔███╔╝██║  ██║██║  ██║██║  ██║██║╚██████╔╝██║  ██║\n"
        "   ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═╝\n"
        COLOR_RESET);

    printf(COLOR_CYAN
        "\n  ═══════════════════════════════════════════════════════════\n"
        "   A Terminal-Based ASCII Dungeon RPG  |  Version " VERSION "\n"
        "  ═══════════════════════════════════════════════════════════\n\n"
        COLOR_RESET);

    printf(COLOR_GRAY
        "  Legend:  " COLOR_WHITE "@ " COLOR_GRAY "= You  "
        COLOR_RED    "E " COLOR_GRAY "= Enemy  "
        COLOR_YELLOW "T " COLOR_GRAY "= Treasure  "
        COLOR_GREEN  "> " COLOR_GRAY "= Stairs  "
        COLOR_WHITE  "# " COLOR_GRAY "= Wall  "
        COLOR_CYAN   ". " COLOR_GRAY "= Floor\n\n"
        COLOR_RESET);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Character creation screen                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */
static void create_character(Player *p) {
    char name[MAX_NAME_LEN];
    int  cls_choice;

    printf("\033[H\033[2J");
    printf(COLOR_YELLOW
           "╔══════════════════════════════════════════╗\n"
           "║         CHARACTER CREATION               ║\n"
           "╚══════════════════════════════════════════╝\n\n"
           COLOR_RESET);

    /* Name */
    printf("  Enter your hero's name: ");
    fflush(stdout);
    if (!fgets(name, sizeof(name), stdin)) strcpy(name, "Hero");
    name[strcspn(name, "\n")] = '\0';
    if (name[0] == '\0') strcpy(name, "Hero");

    /* Class selection */
    printf("\n");
    printf(COLOR_CYAN  "  ┌────────────────────────────────────────┐\n" COLOR_RESET);
    printf(COLOR_CYAN  "  │  Choose your class:                    │\n" COLOR_RESET);
    printf(COLOR_CYAN  "  ├────────────────────────────────────────┤\n" COLOR_RESET);

    printf(COLOR_CYAN  "  │  " COLOR_WHITE "[1] Warrior" COLOR_GRAY
           "  HP:120  ATK:18  DEF:12  MP:30 " COLOR_CYAN "│\n" COLOR_RESET);
    printf(COLOR_CYAN  "  │  " COLOR_MAGENTA "[2] Mage   " COLOR_GRAY
           "  HP: 60  ATK:22  DEF: 6  MP:100" COLOR_CYAN "│\n" COLOR_RESET);
    printf(COLOR_CYAN  "  │  " COLOR_GREEN "[3] Rogue  " COLOR_GRAY
           "  HP: 85  ATK:20  DEF: 8  MP:50 " COLOR_CYAN "│\n" COLOR_RESET);
    printf(COLOR_CYAN  "  └────────────────────────────────────────┘\n\n" COLOR_RESET);

    printf(COLOR_WHITE "  Class tips:\n" COLOR_RESET);
    printf(COLOR_GRAY
        "  Warrior  - Tankiest. Best for beginners.\n"
        "  Mage     - Glass cannon. Fireball wrecks bosses.\n"
        "  Rogue    - Balanced. High speed = more crits & dodge.\n\n"
        COLOR_RESET);

    printf("  Your choice (1-3): ");
    fflush(stdout);

    char buf[8];
    cls_choice = 1;
    if (fgets(buf, sizeof(buf), stdin))
        cls_choice = (buf[0] >= '1' && buf[0] <= '3') ? buf[0] - '0' : 1;

    PlayerClass cls = (PlayerClass)cls_choice;
    player_create(p, name, cls);

    /* Show chosen class art */
    printf("\n");
    player_class_art(cls);
    printf(COLOR_YELLOW "\n  Welcome, %s the %s!\n\n" COLOR_RESET,
           p->name, player_class_name(cls));

    /* Stat sheet */
    player_print_stats(p);

    printf(COLOR_GREEN "\n  ★ Tip: Find the Dragon Boss on Floor 5 to win! ★\n\n" COLOR_RESET);
    press_any_key();
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Win and game-over screens                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */
static void game_win_screen(const Player *p) {
    printf("\033[H\033[2J");
    printf(COLOR_YELLOW
        "\n\n"
        "  ██╗   ██╗ ██████╗ ██╗   ██╗    ██╗    ██╗██╗███╗   ██╗██╗\n"
        "  ╚██╗ ██╔╝██╔═══██╗██║   ██║    ██║    ██║██║████╗  ██║██║\n"
        "   ╚████╔╝ ██║   ██║██║   ██║    ██║ █╗ ██║██║██╔██╗ ██║██║\n"
        "    ╚██╔╝  ██║   ██║██║   ██║    ██║███╗██║██║██║╚██╗██║╚═╝\n"
        "     ██║   ╚██████╔╝╚██████╔╝    ╚███╔███╔╝██║██║ ╚████║██╗\n"
        "     ╚═╝    ╚═════╝  ╚═════╝      ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝╚═╝\n"
        COLOR_RESET);

    printf(COLOR_CYAN
        "\n\n  ★ ═══════════════════════════════════════════ ★\n"
        "    The Dragon has fallen! The dungeon is free!\n"
        "    %s the %s, Level %d — a legend is born!\n"
        "  ★ ═══════════════════════════════════════════ ★\n\n"
        COLOR_RESET,
        p->name, player_class_name(p->pclass), p->level);

    printf(COLOR_WHITE "  Final Stats:\n" COLOR_RESET);
    player_print_stats(p);
    printf("\n");
}

static void game_over_screen(const Player *p) {
    printf("\033[H\033[2J");
    printf(COLOR_RED
        "\n\n"
        "  ██████╗ ███████╗ █████╗ ████████╗██╗  ██╗\n"
        "  ██╔══██╗██╔════╝██╔══██╗╚══██╔══╝██║  ██║\n"
        "  ██║  ██║█████╗  ███████║   ██║   ███████║\n"
        "  ██║  ██║██╔══╝  ██╔══██║   ██║   ██╔══██║\n"
        "  ██████╔╝███████╗██║  ██║   ██║   ██║  ██║\n"
        "  ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝\n"
        COLOR_RESET);

    printf(COLOR_GRAY
        "\n  %s the %s fell on floor %d at level %d.\n"
        "  The dungeon claims another soul...\n\n"
        COLOR_RESET,
        p->name, player_class_name(p->pclass),
        p->dungeon_level, p->level);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Main menu                                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */
static int main_menu(void) {
    game_title();

    int has_save = save_exists();

    printf(COLOR_WHITE
           "  ╔═══════════════════════════╗\n"
           "  ║         MAIN MENU         ║\n"
           "  ╠═══════════════════════════╣\n"
           "  ║  [1] New Game             ║\n");
    if (has_save)
        printf("  ║  " COLOR_GREEN "[2] Continue (Load)" COLOR_WHITE "       ║\n");
    else
        printf("  ║  " COLOR_GRAY "[2] Continue (no save)" COLOR_WHITE "   ║\n");
    printf(
           "  ║  [3] How to Play          ║\n"
           "  ║  [4] Quit                 ║\n"
           "  ╚═══════════════════════════╝\n\n"
           COLOR_RESET);

    printf("  Choice: ");
    fflush(stdout);

    char buf[8];
    if (!fgets(buf, sizeof(buf), stdin)) return 4;
    return (buf[0] >= '1' && buf[0] <= '4') ? buf[0] - '0' : 1;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* How-to-play screen                                                           */
/* ─────────────────────────────────────────────────────────────────────────── */
static void how_to_play(void) {
    printf("\033[H\033[2J");
    printf(COLOR_YELLOW "  HOW TO PLAY — Terminal Warrior\n\n" COLOR_RESET);
    printf(COLOR_WHITE
        "  MOVEMENT\n" COLOR_RESET COLOR_GRAY
        "  W/A/S/D  - Move up/left/down/right\n"
        "  Walking into an enemy starts combat automatically.\n\n"
        COLOR_WHITE "  IN GAME\n" COLOR_RESET COLOR_GRAY
        "  i - Open inventory (use potions, equip weapons)\n"
        "  p - View player stats\n"
        "  S - Save / Load menu\n"
        "  Q - Quit to main menu (unsaved progress lost)\n\n"
        COLOR_WHITE "  COMBAT\n" COLOR_RESET COLOR_GRAY
        "  1 - Attack         (always available)\n"
        "  2 - Defend         (reduce damage next turn)\n"
        "  3 - Use Item       (opens inventory mid-combat)\n"
        "  4 - Special Ability (costs MP, class-specific)\n"
        "  5 - Flee           (50%% chance to escape)\n\n"
        COLOR_WHITE "  GOAL\n" COLOR_RESET COLOR_GRAY
        "  Descend to Floor 5, defeat the Dragon Boss.\n"
        "  Stairs (>) take you deeper into the dungeon.\n\n"
        COLOR_WHITE "  TIPS\n" COLOR_RESET COLOR_GRAY
        "  - Pick up Treasure (T) for random loot\n"
        "  - Fog of war: unexplored tiles are dark\n"
        "  - Enemies hit harder on deeper floors\n"
        "  - Save often!\n"
        COLOR_RESET);

    press_any_key();
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Initialise a brand-new game                                                  */
/* ─────────────────────────────────────────────────────────────────────────── */
static void game_new(Game *g) {
    memset(g, 0, sizeof(Game));
    g->state = STATE_PLAYING;
    snprintf(g->message, 256, "Welcome! Find the stairs (>) to descend.");

    create_character(&g->player);

    map_generate(&g->map, g->player.dungeon_level);
    map_place_player(g);
    map_update_fov(&g->map, g->player.x, g->player.y);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Check if the player walked onto a cell with a live enemy and start combat    */
/* ─────────────────────────────────────────────────────────────────────────── */
static void check_enemy_encounter(Game *g) {
    Enemy *e = map_enemy_at(&g->map, g->player.x, g->player.y);
    if (!e || !e->alive) return;

    /* Kick off combat */
    int result = combat_start(g, e);

    if (result == 0) {
        /* Player died */
        g->state = STATE_GAMEOVER;
        return;
    }

    /* Enemy dead; check for boss kill → win condition */
    if (e->is_boss && !e->alive) {
        g->state = STATE_WIN;
    }

    /* Remove dead enemy from map tile */
    if (!e->alive && g->map.grid[e->y][e->x].tile == TILE_ENEMY) {
        g->map.grid[e->y][e->x].tile = TILE_FLOOR;
    }
    if (!e->alive && g->map.grid[e->y][e->x].tile == TILE_BOSS) {
        g->map.grid[e->y][e->x].tile = TILE_FLOOR;
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Main game loop                                                               */
/* ─────────────────────────────────────────────────────────────────────────── */
static void game_loop(Game *g) {
    while (g->state == STATE_PLAYING) {

        /* Render current map */
        map_render(g);

        /* Read a single raw keypress (no Enter needed) */
        char key = read_raw_key();

        switch (key) {
            /* ── Movement ── */
            case 'w': case 'W':
                if (map_move_player(g,  0, -1))
                    check_enemy_encounter(g);
                break;
            case 's': case 'S':
                if (key == 's') {
                    /* lowercase s = move down */
                    if (map_move_player(g,  0,  1))
                        check_enemy_encounter(g);
                } else {
                    /* uppercase S = Save menu */
                    disable_raw_mode();
                    save_menu(g);
                    enable_raw_mode();
                }
                break;
            case 'a': case 'A':
                if (map_move_player(g, -1,  0))
                    check_enemy_encounter(g);
                break;
            case 'd': case 'D':
                if (map_move_player(g,  1,  0))
                    check_enemy_encounter(g);
                break;

            /* ── Inventory ── */
            case 'i': case 'I':
                disable_raw_mode();
                inv_show(g);
                enable_raw_mode();
                break;

            /* ── Stats ── */
            case 'p': case 'P':
                printf("\033[H\033[2J");
                player_print_stats(&g->player);
                printf("\n");
                inv_print_short(&g->player.inv);
                press_any_key();
                break;

            /* ── Save / Load ── */
            case 'l': case 'L':
                disable_raw_mode();
                save_menu(g);
                enable_raw_mode();
                break;

            /* ── Quit ── */
            case 'q': case 'Q':
                disable_raw_mode();
                printf("\n  Quit to main menu? Progress may be lost. (y/n): ");
                fflush(stdout);
                char qbuf[4];
                if (fgets(qbuf, sizeof(qbuf), stdin) && (qbuf[0] == 'y' || qbuf[0] == 'Y')) {
                    g->state = STATE_MENU;
                }
                enable_raw_mode();
                break;

            /* ── Unknown key ── */
            default:
                snprintf(g->message, 256, "Unknown key '%c'. WASD=move i=inv p=stats S=save Q=quit", key);
                break;
        }

        /* ── Check death from poison between moves ── */
        if (g->player.hp <= 0 && g->state == STATE_PLAYING) {
            g->state = STATE_GAMEOVER;
        }

        g->turn++;
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Program entry point                                                          */
/* ─────────────────────────────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned int)time(NULL));

    /* Hide cursor for cleaner map rendering */
    printf("\033[?25l");
    fflush(stdout);

    int running = 1;
    while (running) {

        disable_raw_mode();   /* cooked mode for menu text input */
        int choice = main_menu();

        switch (choice) {
            case 1: {   /* New Game */
                /* Warn if a save exists */
                if (save_exists()) {
                    printf(COLOR_YELLOW
                           "\n  A save file exists. Starting a new game will\n"
                           "  overwrite it. Continue? (y/n): " COLOR_RESET);
                    fflush(stdout);
                    char buf[4];
                    if (fgets(buf, sizeof(buf), stdin) && buf[0] != 'y' && buf[0] != 'Y')
                        break;
                    save_delete();
                }

                Game g;
                game_new(&g);
                game_loop(&g);

                if (g.state == STATE_WIN) {
                    save_delete();
                    game_win_screen(&g.player);
                    press_any_key();
                } else if (g.state == STATE_GAMEOVER) {
                    save_delete();
                    game_over_screen(&g.player);
                    press_any_key();
                }
                break;
            }

            case 2: {   /* Load / Continue */
                if (!save_exists()) {
                    printf(COLOR_RED "\n  No save file found.\n" COLOR_RESET);
                    press_any_key();
                    break;
                }

                Game g;
                memset(&g, 0, sizeof(Game));
                if (!load_game(&g)) {
                    press_any_key();
                    break;
                }

                printf(COLOR_GREEN "\n  Save loaded! Welcome back, %s!\n" COLOR_RESET,
                       g.player.name);
                press_any_key();

                g.state = STATE_PLAYING;
                game_loop(&g);

                if (g.state == STATE_WIN) {
                    save_delete();
                    game_win_screen(&g.player);
                    press_any_key();
                } else if (g.state == STATE_GAMEOVER) {
                    save_delete();
                    game_over_screen(&g.player);
                    press_any_key();
                }
                break;
            }

            case 3:   /* How to Play */
                how_to_play();
                break;

            case 4:   /* Quit */
            default:
                running = 0;
                break;
        }
    }

    /* Restore terminal: show cursor, reset colours */
    printf(COLOR_RESET "\033[?25h\n");
    disable_raw_mode();
    printf("  Thanks for playing Terminal Warrior! Farewell, adventurer.\n\n");
    return 0;
}
