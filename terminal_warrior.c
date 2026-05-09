
/*
 * Terminal Warrior - C port (ncurses)
 * Build:  gcc -O2 -o terminal_warrior terminal_warrior.c -lncurses
 * Run:    ./terminal_warrior
 *
 * Save file: ~/.terminal_warrior_save  (binary snapshot of game state)
 *
 * Controls:
 *   Title:  1 New  2 Continue  3 How-to  4 Quit
 *   Map:    WASD / arrows to move, I inventory, Q quit-to-menu
 *   Combat: 1 Attack 2 Defend 3 Item 4 Special 5 Flee
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

/* ── Constants ───────────────────────────────────────────── */
#define MAP_W   60
#define MAP_H   18
#define FOG_R   6
#define MAX_INV 10
#define MAX_ENEMIES 16
#define MAX_LOOT    16
#define MAX_ROOMS   8
#define MAX_LOG     8

/* ── Expanded color pairs ─────────────────────────────────
 *  We now use more pairs to get richer, more readable UI.
 *  Standard ncurses gives us 64 pairs; we use 20-ish.
 */
enum {
    CP_DEFAULT  =  1,   /* white text, default bg             */
    CP_RED      =  2,   /* bright red  – enemy HP, danger     */
    CP_GREEN    =  3,   /* bright green – player HP, grass    */
    CP_YELLOW   =  4,   /* yellow – treasure, highlights      */
    CP_BLUE     =  5,   /* blue – MP bar, cold                */
    CP_MAGENTA  =  6,   /* magenta – boss enemy               */
    CP_CYAN     =  7,   /* cyan – stairs, info text           */
    CP_WHITE    =  8,   /* bright white – walls, player @     */
    CP_DIM      =  9,   /* dark grey – explored-but-dark      */

    /* NEW pairs for richer colour */
    CP_PLAYER   = 10,   /* bold yellow – @ glyph on map       */
    CP_FLOOR    = 11,   /* mid grey – visible floor tiles     */
    CP_WALL     = 12,   /* dark grey – visible wall tiles     */
    CP_WALL_EXP = 13,   /* very dim – explored-not-visible #  */
    CP_FLOOR_EXP= 14,   /* dimmer  – explored-not-visible .   */
    CP_ENEMY    = 15,   /* red on default – regular enemies   */
    CP_BOSS     = 16,   /* bright magenta + bold – boss       */
    CP_TREASURE = 17,   /* bright yellow – T glyph            */
    CP_STAIRS   = 18,   /* bright cyan – > glyph              */
    CP_POISON   = 19,   /* green – poison status text         */
    CP_STUN     = 20,   /* yellow – stun status text          */
    CP_DEFEND   = 21,   /* cyan – defend status text          */
    CP_HEADER   = 22,   /* white bold – section headers       */
    CP_BAR_HP   = 23,   /* green on black – HP bar fill       */
    CP_BAR_MP   = 24,   /* blue on black – MP bar fill        */
    CP_BAR_ENE  = 25,   /* red on black – enemy HP bar fill   */
    CP_GOBLIN   = 26,   /* dark green – goblin art            */
    CP_SKELETON = 27,   /* white  – skeleton art              */
    CP_ORC      = 28,   /* yellow – orc art                   */
    CP_VAMPIRE  = 29,   /* magenta – vampire art              */
    CP_DRAGON   = 30,   /* bright red + bold – dragon art     */
    CP_XP       = 31,   /* bright yellow – XP / level info    */
    CP_FLEE     = 32,   /* bright red – fled / danger msgs    */
    CP_WIN      = 33,   /* bright yellow on default – victory */
    CP_GAMEOVER = 34,   /* red – game over                    */
    CP_MENU_HL  = 35,   /* yellow bold – selected menu item   */
    CP_MENU_DIM = 36,   /* dark – disabled menu item          */
    CP_TITLE    = 37,   /* bright red bold – title banner     */
    CP_SUBHEAD  = 38,   /* cyan – section sub-headings        */
};

/* Player class */
enum { CL_WARRIOR = 1, CL_MAGE = 2, CL_ROGUE = 3 };

/* Enemy types */
enum { E_GOBLIN, E_SKELETON, E_ORC, E_VAMPIRE, E_DRAGON };

/* Item types */
enum { IT_HEALTH, IT_MANA, IT_ANTIDOTE, IT_WEAPON, IT_ARMOR };

/* Status */
enum { ST_NONE, ST_POISON, ST_STUN, ST_DEFEND };

/* ── Types ──────────────────────────────────────────────── */
typedef struct { int x, y, w, h; } Room;

typedef struct {
    char tile;       /* '#', '.', '>', 'E', 'B', 'T' */
    int  visible;
    int  explored;
} Cell;

typedef struct {
    int   alive;
    int   type;       /* E_* */
    char  name[16];
    int   hp, maxHp;
    int   atk, def;
    int   xpReward;
    int   x, y;
    int   status, statusTurns;
    int   isBoss;
    int   dragonMove;
} Enemy;

typedef struct {
    char name[24];
    int  type;        /* IT_* */
    int  value;
    int  quantity;
} Item;

typedef struct {
    int  x, y;
    Item item;
} Loot;

typedef struct {
    Cell  grid[MAP_H][MAP_W];
    Enemy enemies[MAX_ENEMIES];
    int   nEnemies;
    Loot  loot[MAX_LOOT];
    int   nLoot;
    int   stairsX, stairsY;
    Room  rooms[MAX_ROOMS];
    int   nRooms;
} Map;

typedef struct {
    char name[24];
    int  pclass;
    int  hp, maxHp, mp, maxMp;
    int  atk, def, spd;
    int  level, xp, xpNext;
    int  dungeonLevel;
    int  x, y;
    int  status, statusTurns, defendBonus;
    Item inventory[MAX_INV];
    int  nInv;
} Player;

typedef struct {
    Player player;
    Map    map;
    int    state;        /* 0 playing, 1 game-over, 2 win */
    int    turn;
    char   message[128];
} Game;

/* ── Globals ──────────────────────────────────────────────── */
static Game     G;
static int      gameLoaded = 0;
static Enemy   *combatEnemy = NULL;
static char     combatLog[MAX_LOG][96];
static int      combatLogN = 0;
static uint32_t _seed;

/* ── RNG ─────────────────────────────────────────────────── */
static double rngd(void) {
    _seed = _seed * 1664525u + 1013904223u;
    return (double)_seed / (double)0xffffffffu;
}
static int randn(int n) {
    if (n <= 0) return 0;
    return (int)(rngd() * n);
}

/* ── Logging ─────────────────────────────────────────────── */
static void logPush(const char *s) {
    if (combatLogN < MAX_LOG) {
        strncpy(combatLog[combatLogN], s, 95);
        combatLog[combatLogN][95] = 0;
        combatLogN++;
    } else {
        for (int i = 1; i < MAX_LOG; i++)
            strcpy(combatLog[i-1], combatLog[i]);
        strncpy(combatLog[MAX_LOG-1], s, 95);
        combatLog[MAX_LOG-1][95] = 0;
    }
}
static void logClear(void) { combatLogN = 0; }

/* ── Save / Load ─────────────────────────────────────────── */
static const char *savePath(void) {
    static char p[512];
    const char *h = getenv("HOME");
    if (!h) h = ".";
    snprintf(p, sizeof(p), "%s/.terminal_warrior_save", h);
    return p;
}
static int hasSave(void) {
    FILE *f = fopen(savePath(), "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}
static void saveGame(void) {
    FILE *f = fopen(savePath(), "wb");
    if (!f) return;
    fwrite(&G, sizeof(G), 1, f);
    fclose(f);
}
static int loadSave(void) {
    FILE *f = fopen(savePath(), "rb");
    if (!f) return 0;
    size_t n = fread(&G, sizeof(G), 1, f);
    fclose(f);
    return n == 1;
}
static void deleteSave(void) { remove(savePath()); }

/* ── Drawing helpers ─────────────────────────────────────── */
static void drawCenter(int row, int pair, int bold, const char *s) {
    int w = COLS;
    int x = (w - (int)strlen(s)) / 2;
    if (x < 0) x = 0;
    attr_t attrs = COLOR_PAIR(pair) | (bold ? A_BOLD : 0);
    attron(attrs);
    mvprintw(row, x, "%s", s);
    attroff(attrs);
}

/* ── Init colors ─────────────────────────────────────────── */
static void initColors(void) {
    start_color();
    use_default_colors();

    /* Legacy pairs kept for any stray references */
    init_pair(CP_DEFAULT,   COLOR_WHITE,   -1);
    init_pair(CP_RED,       COLOR_RED,     -1);
    init_pair(CP_GREEN,     COLOR_GREEN,   -1);
    init_pair(CP_YELLOW,    COLOR_YELLOW,  -1);
    init_pair(CP_BLUE,      COLOR_BLUE,    -1);
    init_pair(CP_MAGENTA,   COLOR_MAGENTA, -1);
    init_pair(CP_CYAN,      COLOR_CYAN,    -1);
    init_pair(CP_WHITE,     COLOR_WHITE,   -1);
    init_pair(CP_DIM,       COLOR_BLACK,   -1);

    /* New, richer pairs */
    init_pair(CP_PLAYER,    COLOR_YELLOW,  -1);   /* @ on map – bold yellow  */
    init_pair(CP_FLOOR,     COLOR_WHITE,   -1);   /* visible floor – mid     */
    init_pair(CP_WALL,      COLOR_WHITE,   -1);   /* visible wall – bright   */
    init_pair(CP_WALL_EXP,  COLOR_BLACK,   -1);   /* explored wall – dim     */
    init_pair(CP_FLOOR_EXP, COLOR_BLACK,   -1);   /* explored floor – dim    */
    init_pair(CP_ENEMY,     COLOR_RED,     -1);   /* E on map                */
    init_pair(CP_BOSS,      COLOR_MAGENTA, -1);   /* B on map                */
    init_pair(CP_TREASURE,  COLOR_YELLOW,  -1);   /* T on map                */
    init_pair(CP_STAIRS,    COLOR_CYAN,    -1);   /* > on map                */
    init_pair(CP_POISON,    COLOR_GREEN,   -1);   /* [POISONED] badge        */
    init_pair(CP_STUN,      COLOR_YELLOW,  -1);   /* [STUNNED] badge         */
    init_pair(CP_DEFEND,    COLOR_CYAN,    -1);   /* [DEFENDING] badge       */
    init_pair(CP_HEADER,    COLOR_WHITE,   -1);   /* bold headers            */
    init_pair(CP_BAR_HP,    COLOR_GREEN,   COLOR_BLACK); /* HP bar fill      */
    init_pair(CP_BAR_MP,    COLOR_BLUE,    COLOR_BLACK); /* MP bar fill      */
    init_pair(CP_BAR_ENE,   COLOR_RED,     COLOR_BLACK); /* enemy HP fill    */
    init_pair(CP_GOBLIN,    COLOR_GREEN,   -1);
    init_pair(CP_SKELETON,  COLOR_WHITE,   -1);
    init_pair(CP_ORC,       COLOR_YELLOW,  -1);
    init_pair(CP_VAMPIRE,   COLOR_MAGENTA, -1);
    init_pair(CP_DRAGON,    COLOR_RED,     -1);
    init_pair(CP_XP,        COLOR_YELLOW,  -1);
    init_pair(CP_FLEE,      COLOR_RED,     -1);
    init_pair(CP_WIN,       COLOR_YELLOW,  -1);
    init_pair(CP_GAMEOVER,  COLOR_RED,     -1);
    init_pair(CP_MENU_HL,   COLOR_YELLOW,  -1);
    init_pair(CP_MENU_DIM,  COLOR_BLACK,   -1);
    init_pair(CP_TITLE,     COLOR_RED,     -1);
    init_pair(CP_SUBHEAD,   COLOR_CYAN,    -1);
}

/* ── Title Screen ────────────────────────────────────────── */
static void drawTitle(void) {
    erase();
    drawCenter(2,  CP_TITLE,    1, "══  TERMINAL WARRIOR  ══");
    drawCenter(3,  CP_MENU_DIM, 0, "v1.0.0  ·  A Dungeon Crawler (C/ncurses port)");

    drawCenter(5,  CP_CYAN,     0, "+------------------+");
    drawCenter(6,  CP_CYAN,     0, "|  Descend. Fight. |");
    drawCenter(7,  CP_CYAN,     0, "|  Survive the     |");
    drawCenter(8,  CP_CYAN,     0, "|  Dragon's Lair.  |");
    drawCenter(9,  CP_CYAN,     0, "+------------------+");

    int has = hasSave();
    int top = 12;
    drawCenter(top, CP_YELLOW, 1, "── MAIN MENU ──");

    char buf[64];
    snprintf(buf, sizeof(buf), "[1]  New Game");
    drawCenter(top+2, CP_MENU_HL, 1, buf);

    if (has) {
        snprintf(buf, sizeof(buf), "[2]  Continue (Load)");
        drawCenter(top+3, CP_MENU_HL, 1, buf);
    } else {
        snprintf(buf, sizeof(buf), "[2]  Continue (no save)");
        drawCenter(top+3, CP_MENU_DIM, 0, buf);
    }

    snprintf(buf, sizeof(buf), "[3]  How to Play");
    drawCenter(top+4, CP_DEFAULT, 0, buf);

    snprintf(buf, sizeof(buf), "[4]  Quit");
    drawCenter(top+5, CP_FLEE, 0, buf);

    drawCenter(top+8, CP_MENU_DIM, 0, "Press a number key to choose.");
    refresh();
}

/* ── How to Play ─────────────────────────────────────────── */
static void drawHowTo(void) {
    erase();
    int y = 1;
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(y++, 2, "HOW TO PLAY");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    y++;

    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 2, "MOVEMENT");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 4, "WASD / arrow keys to move. Walking into an enemy starts combat.");
    y++;

    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 2, "COMBAT");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    attron(COLOR_PAIR(CP_MENU_HL));
    mvprintw(y++, 4, "[1] Attack");
    attroff(COLOR_PAIR(CP_MENU_HL));
    mvprintw(LINES - 1, 0, "");   /* keep cursor safe */
    mvprintw(y-1, 18, " - Standard attack. Rogues have higher crit.");

    attron(COLOR_PAIR(CP_DEFEND));
    mvprintw(y++, 4, "[2] Defend");
    attroff(COLOR_PAIR(CP_DEFEND));
    mvprintw(y-1, 18, " - Double DEF for one turn.");

    attron(COLOR_PAIR(CP_GREEN));
    mvprintw(y++, 4, "[3] Use Item");
    attroff(COLOR_PAIR(CP_GREEN));
    mvprintw(y-1, 18, " - Use a potion / antidote.");

    attron(COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvprintw(y++, 4, "[4] Special");
    attroff(COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvprintw(y-1, 18, " - Class ability (costs MP).");

    attron(COLOR_PAIR(CP_FLEE));
    mvprintw(y++, 4, "[5] Flee");
    attroff(COLOR_PAIR(CP_FLEE));
    mvprintw(y-1, 18, "   - 50%% chance to escape.");
    y++;

    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 2, "CLASS SPECIALS");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);

    attron(COLOR_PAIR(CP_RED) | A_BOLD);
    mvprintw(y, 4, "Warrior");
    attroff(COLOR_PAIR(CP_RED) | A_BOLD);
    mvprintw(y++, 12, " - Shield Bash  (20 MP): 2x dmg + STUN");

    attron(COLOR_PAIR(CP_BLUE) | A_BOLD);
    mvprintw(y, 4, "Mage   ");
    attroff(COLOR_PAIR(CP_BLUE) | A_BOLD);
    mvprintw(y++, 12, " - Fireball     (30 MP): 3x magic dmg, ignores DEF");

    attron(COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    mvprintw(y, 4, "Rogue  ");
    attroff(COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    mvprintw(y++, 12, " - Shadow Strike(15 MP): 2x dmg + 4-turn POISON");
    y++;

    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 2, "MAP LEGEND");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);

    /* Coloured legend glyphs */
    int lx = 4, ly = y;
    attron(COLOR_PAIR(CP_PLAYER) | A_BOLD); mvprintw(ly, lx, "@"); attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);
    mvprintw(ly++, lx+2, "You");
    attron(COLOR_PAIR(CP_ENEMY) | A_BOLD);  mvprintw(ly, lx, "E"); attroff(COLOR_PAIR(CP_ENEMY) | A_BOLD);
    mvprintw(ly++, lx+2, "Enemy");
    attron(COLOR_PAIR(CP_BOSS) | A_BOLD);   mvprintw(ly, lx, "B"); attroff(COLOR_PAIR(CP_BOSS) | A_BOLD);
    mvprintw(ly++, lx+2, "Boss");
    attron(COLOR_PAIR(CP_TREASURE) | A_BOLD); mvprintw(ly, lx, "T"); attroff(COLOR_PAIR(CP_TREASURE) | A_BOLD);
    mvprintw(ly++, lx+2, "Treasure");
    attron(COLOR_PAIR(CP_STAIRS) | A_BOLD); mvprintw(ly, lx, ">"); attroff(COLOR_PAIR(CP_STAIRS) | A_BOLD);
    mvprintw(ly++, lx+2, "Stairs down");
    attron(COLOR_PAIR(CP_WALL) | A_BOLD);   mvprintw(ly, lx, "#"); attroff(COLOR_PAIR(CP_WALL) | A_BOLD);
    mvprintw(ly++, lx+2, "Wall");
    attron(COLOR_PAIR(CP_FLOOR));           mvprintw(ly, lx, "."); attroff(COLOR_PAIR(CP_FLOOR));
    mvprintw(ly++, lx+2, "Floor");
    y = ly + 1;

    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(y++, 2, "GOAL");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    attron(COLOR_PAIR(CP_YELLOW));
    mvprintw(y++, 4, "Descend through 3 dungeon levels and slay the Dragon Boss!");
    attroff(COLOR_PAIR(CP_YELLOW));
    y += 2;

    attron(COLOR_PAIR(CP_MENU_DIM));
    mvprintw(y, 2, "Press any key to return to the main menu.");
    attroff(COLOR_PAIR(CP_MENU_DIM));
    refresh();
}

/* ── Character creation ──────────────────────────────────── */
static int charScreen(char *outName, int *outClass) {
    int sel = CL_WARRIOR;
    char name[24] = "Hero";

    while (1) {
        erase();
        drawCenter(1, CP_YELLOW, 1, "══  CREATE YOUR HERO  ══");

        attron(COLOR_PAIR(CP_CYAN));
        mvprintw(4, 4, "Hero Name: ");
        attroff(COLOR_PAIR(CP_CYAN));
        attron(COLOR_PAIR(CP_WHITE) | A_BOLD);
        mvprintw(4, 16, "%s_", name);
        attroff(COLOR_PAIR(CP_WHITE) | A_BOLD);

        attron(COLOR_PAIR(CP_SUBHEAD));
        mvprintw(6, 4, "Choose Your Class:");
        attroff(COLOR_PAIR(CP_SUBHEAD));

        const char *titles[] = { "", "[1] WARRIOR", "[2] MAGE", "[3] ROGUE" };
        const char *desc[]   = { "",
            "Tough and strong. High HP & armor. Special: Shield Bash (stuns).",
            "Glass cannon. Low HP, devastating magic. Special: Fireball.",
            "Fast and sneaky. High crit chance. Special: Shadow Strike (poisons)." };
        const char *stats[]  = { "",
            "HP:120  MP:30   ATK:12  DEF:8",
            "HP:70   MP:80   ATK:16  DEF:3",
            "HP:90   MP:50   ATK:14  DEF:5" };
        /* Color per class: warrior=red, mage=blue, rogue=magenta */
        int clrPairs[] = { 0, CP_RED, CP_BLUE, CP_MAGENTA };

        for (int c = 1; c <= 3; c++) {
            int row = 8 + (c-1)*4;
            int hl  = (sel == c);
            int pair = hl ? clrPairs[c] : CP_MENU_DIM;
            attron(COLOR_PAIR(pair) | (hl ? A_BOLD : 0));
            mvprintw(row,   4, "%s %s", hl ? ">" : " ", titles[c]);
            attroff(A_BOLD);
            mvprintw(row+1, 8, "%s", desc[c]);
            if (hl) {
                attron(COLOR_PAIR(CP_CYAN) | A_BOLD);
                mvprintw(row+2, 8, "%s", stats[c]);
                attroff(COLOR_PAIR(CP_CYAN) | A_BOLD);
            }
            attroff(COLOR_PAIR(pair));
        }

        attron(COLOR_PAIR(CP_MENU_DIM));
        mvprintw(22, 4, "Type letters to set name. [1/2/3] pick class. [Enter] begin. [Esc] back.");
        attroff(COLOR_PAIR(CP_MENU_DIM));
        refresh();

        int ch = getch();
        if (ch == 27) return 0;
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (!name[0]) strcpy(name, "Hero");
            strncpy(outName, name, 23); outName[23] = 0;
            *outClass = sel;
            return 1;
        }
        if (ch == '1' || ch == '2' || ch == '3') { sel = ch - '0'; continue; }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            int n = strlen(name);
            if (n > 0) name[n-1] = 0;
            continue;
        }
        if (ch >= 32 && ch < 127) {
            int n = strlen(name);
            if (n < 20) { name[n] = (char)ch; name[n+1] = 0; }
        }
    }
}

/* ── Map generation ──────────────────────────────────────── */
static Enemy spawnEnemy(int level, int isBoss, int x, int y) {
    Enemy e = {0};
    e.alive = 1; e.x = x; e.y = y;
    e.status = ST_NONE; e.statusTurns = 0;
    if (isBoss) {
        e.type = E_DRAGON;
        strcpy(e.name, "Dragon");
        e.hp = e.maxHp = 200;
        e.atk = 25; e.def = 12;
        e.xpReward = 200;
        e.isBoss = 1;
        return e;
    }
    int pool[3]; int np;
    if (level == 1)      { pool[0]=E_GOBLIN; pool[1]=E_SKELETON; np = 2; }
    else if (level == 2) { pool[0]=E_GOBLIN; pool[1]=E_SKELETON; pool[2]=E_ORC; np = 3; }
    else                 { pool[0]=E_ORC;    pool[1]=E_VAMPIRE;  pool[2]=E_SKELETON; np = 3; }
    int t = pool[randn(np)];
    e.type = t;
    switch (t) {
        case E_GOBLIN:   strcpy(e.name,"Goblin");   e.hp=25+randn(10); e.atk=6+level*2;  e.def=2; e.xpReward=10+level*5; break;
        case E_SKELETON: strcpy(e.name,"Skeleton"); e.hp=30+randn(10); e.atk=8+level*2;  e.def=4; e.xpReward=15+level*5; break;
        case E_ORC:      strcpy(e.name,"Orc");      e.hp=50+randn(15); e.atk=12+level*2; e.def=6; e.xpReward=25+level*5; break;
        case E_VAMPIRE:  strcpy(e.name,"Vampire");  e.hp=40+randn(15); e.atk=10+level*2; e.def=5; e.xpReward=30+level*5; break;
    }
    e.maxHp = e.hp;
    return e;
}

static Item randomItem(int level) {
    Item it = {0};
    it.quantity = 1;
    int r = randn(100);
    if (r < 40)      { strcpy(it.name,"Health Potion"); it.type = IT_HEALTH;   it.value = 30 + level*5; }
    else if (r < 60) { strcpy(it.name,"Mana Potion");   it.type = IT_MANA;     it.value = 20 + level*5; }
    else if (r < 75) { strcpy(it.name,"Antidote");      it.type = IT_ANTIDOTE; it.value = 0; }
    else if (r < 88) { snprintf(it.name,sizeof(it.name),"Iron Sword +%d", level); it.type = IT_WEAPON; it.value = 2+level; }
    else             { snprintf(it.name,sizeof(it.name),"Steel Armor +%d", level); it.type = IT_ARMOR; it.value = 2+level; }
    return it;
}

static void generateMap(Map *m, int level) {
    memset(m, 0, sizeof(*m));
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            m->grid[y][x].tile = '#';

    Room rooms[64]; int nr = 0;
    for (int i = 0; i < 30 && nr < MAX_ROOMS; i++) {
        int rw = 4 + randn(7), rh = 3 + randn(5);
        int rx = 1 + randn(MAP_W - rw - 2);
        int ry = 1 + randn(MAP_H - rh - 2);
        int overlap = 0;
        for (int j = 0; j < nr; j++) {
            Room *r = &rooms[j];
            if (rx < r->x + r->w + 1 && rx + rw > r->x - 1 &&
                ry < r->y + r->h + 1 && ry + rh > r->y - 1) { overlap = 1; break; }
        }
        if (!overlap) { rooms[nr].x = rx; rooms[nr].y = ry; rooms[nr].w = rw; rooms[nr].h = rh; nr++; }
    }
    if (nr < 2) {
        rooms[0].x = 1; rooms[0].y = 1; rooms[0].w = MAP_W-2; rooms[0].h = MAP_H-2; nr = 1;
    }
    m->nRooms = nr;
    for (int i = 0; i < nr; i++) m->rooms[i] = rooms[i];

    for (int i = 0; i < nr; i++) {
        Room *r = &rooms[i];
        for (int y = r->y; y < r->y + r->h; y++)
            for (int x = r->x; x < r->x + r->w; x++)
                m->grid[y][x].tile = '.';
    }
    for (int i = 1; i < nr; i++) {
        Room *a = &rooms[i-1], *b = &rooms[i];
        int ax = a->x + a->w/2, ay = a->y + a->h/2;
        int bx = b->x + b->w/2, by = b->y + b->h/2;
        int cx = ax, cy = ay;
        while (cx != bx) { m->grid[cy][cx].tile = '.'; cx += (bx > cx ? 1 : -1); }
        while (cy != by) { m->grid[cy][cx].tile = '.'; cy += (by > cy ? 1 : -1); }
    }

    Room *last = &rooms[nr-1];
    int sx = last->x + last->w/2, sy = last->y + last->h/2;
    m->stairsX = sx; m->stairsY = sy;
    m->grid[sy][sx].tile = '>';

    int isBoss = (level == 3);
    int count = 3 + level * 2; if (count > 12) count = 12;
    for (int i = 0; i < count && m->nEnemies < MAX_ENEMIES - 1; i++) {
        int idx = (nr <= 1) ? 0 : 1 + randn(nr - 1);
        Room *r = &rooms[idx];
        int ex = r->x + 1 + randn(r->w > 2 ? r->w - 2 : 1);
        int ey = r->y + 1 + randn(r->h > 2 ? r->h - 2 : 1);
        if (ex < 0 || ex >= MAP_W || ey < 0 || ey >= MAP_H) continue;
        if (m->grid[ey][ex].tile != '.') continue;
        Enemy e = spawnEnemy(level, 0, ex, ey);
        m->enemies[m->nEnemies++] = e;
        m->grid[ey][ex].tile = 'E';
    }
    if (isBoss) {
        Room *br = &rooms[nr/2];
        int bx2 = br->x + br->w/2, by2 = br->y + br->h/2;
        Enemy boss = spawnEnemy(level, 1, bx2, by2);
        m->enemies[m->nEnemies++] = boss;
        m->grid[by2][bx2].tile = 'B';
    }

    int tcount = 2 + randn(3);
    for (int i = 0; i < tcount && m->nLoot < MAX_LOOT; i++) {
        Room *r = &rooms[randn(nr)];
        int tx = r->x + randn(r->w);
        int ty = r->y + randn(r->h);
        if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) continue;
        if (m->grid[ty][tx].tile != '.') continue;
        m->grid[ty][tx].tile = 'T';
        m->loot[m->nLoot].x = tx;
        m->loot[m->nLoot].y = ty;
        m->loot[m->nLoot].item = randomItem(level);
        m->nLoot++;
    }
}

/* ── FOV ─────────────────────────────────────────────────── */
static void updateFOV(Map *m, int px, int py) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            m->grid[y][x].visible = 0;
    for (int dy = -FOG_R; dy <= FOG_R; dy++) {
        for (int dx = -FOG_R; dx <= FOG_R; dx++) {
            if (dx*dx + dy*dy > FOG_R*FOG_R) continue;
            int tx = px+dx, ty = py+dy;
            if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) continue;
            m->grid[ty][tx].visible  = 1;
            m->grid[ty][tx].explored = 1;
        }
    }
}

static void placePlayer(Map *m, Player *p) {
    Room *r = &m->rooms[0];
    p->x = r->x + 1; p->y = r->y + 1;
}

/* ── Player creation / new game ──────────────────────────── */
static Player createPlayer(const char *name, int pclass) {
    Player p = {0};
    strncpy(p.name, name, 23); p.name[23] = 0;
    p.pclass = pclass;
    int hp=0, mp=0, atk=0, def=0, spd=0;
    switch (pclass) {
        case CL_WARRIOR: hp=120; mp=30; atk=12; def=8; spd=8;  break;
        case CL_MAGE:    hp=70;  mp=80; atk=16; def=3; spd=10; break;
        case CL_ROGUE:   hp=90;  mp=50; atk=14; def=5; spd=12; break;
    }
    p.hp = p.maxHp = hp;
    p.mp = p.maxMp = mp;
    p.atk = atk; p.def = def; p.spd = spd;
    p.level = 1; p.xp = 0; p.xpNext = 30;
    p.dungeonLevel = 1;
    p.x = 1; p.y = 1;
    p.status = ST_NONE; p.statusTurns = 0; p.defendBonus = 0;
    p.nInv = 0;
    return p;
}

static void newGame(const char *name, int pclass) {
    memset(&G, 0, sizeof(G));
    G.player = createPlayer(name, pclass);
    generateMap(&G.map, G.player.dungeonLevel);
    placePlayer(&G.map, &G.player);
    updateFOV(&G.map, G.player.x, G.player.y);
    G.state = 0; G.turn = 0;
    strcpy(G.message, "Welcome! Find the stairs (>) to descend.");
}

/* ── HUD / Map rendering ────────────────────────────────── */
static const char *className(int c) {
    switch (c) { case 1: return "Warrior"; case 2: return "Mage"; case 3: return "Rogue"; }
    return "?";
}
static int classColor(int c) {
    switch (c) { case 1: return CP_RED; case 2: return CP_BLUE; case 3: return CP_MAGENTA; }
    return CP_DEFAULT;
}

/* Draw a bar with coloured fill.  The 'filled' portion uses fillPair, the
   empty portion uses emptyPair (CP_DIM looks like a dim background).       */
static void drawBar(int row, int col, int width, int cur, int max,
                    int fillPair, int emptyPair, const char *label) {
    if (max <= 0) max = 1;
    int filled = (cur * width) / max;
    if (filled < 0) filled = 0; if (filled > width) filled = width;

    attron(COLOR_PAIR(CP_DEFAULT));
    mvprintw(row, col, "%s [", label);
    attroff(COLOR_PAIR(CP_DEFAULT));

    /* filled portion */
    attron(COLOR_PAIR(fillPair) | A_BOLD);
    for (int i = 0; i < filled; i++) addch('|');
    attroff(COLOR_PAIR(fillPair) | A_BOLD);

    /* empty portion */
    attron(COLOR_PAIR(emptyPair));
    for (int i = filled; i < width; i++) addch('-');
    attroff(COLOR_PAIR(emptyPair));

    attron(COLOR_PAIR(CP_DEFAULT));
    printw("] %d/%d", cur, max);
    attroff(COLOR_PAIR(CP_DEFAULT));
}

/* Returns (pair, bold) for a map tile */
static void tileAttr(char c, int visible, int explored, int *pair, int *bold) {
    *bold = 0;
    if (!visible) {
        /* explored-but-dark */
        *pair = (c == '#') ? CP_WALL_EXP : CP_FLOOR_EXP;
        return;
    }
    switch (c) {
        case '#': *pair = CP_WALL;    *bold = 1; break;
        case '.': *pair = CP_FLOOR;   *bold = 0; break;
        case '@': *pair = CP_PLAYER;  *bold = 1; break;
        case 'E': *pair = CP_ENEMY;   *bold = 1; break;
        case 'B': *pair = CP_BOSS;    *bold = 1; break;
        case 'T': *pair = CP_TREASURE;*bold = 1; break;
        case '>': *pair = CP_STAIRS;  *bold = 1; break;
        default:  *pair = CP_DEFAULT; break;
    }
}

static void renderMap(void) {
    erase();
    Player *p = &G.player;

    /* ── Header row 0: name + class ── */
    attron(COLOR_PAIR(CP_WHITE) | A_BOLD);
    mvprintw(0, 0, "%s", p->name);
    attroff(COLOR_PAIR(CP_WHITE) | A_BOLD);

    attron(COLOR_PAIR(classColor(p->pclass)) | A_BOLD);
    mvprintw(0, strlen(p->name) + 1, "[%s]", className(p->pclass));
    attroff(COLOR_PAIR(classColor(p->pclass)) | A_BOLD);

    attron(COLOR_PAIR(CP_MENU_DIM));
    mvprintw(0, 24, "Lv.%d   Floor %d   Turn %d", p->level, p->dungeonLevel, G.turn);
    attroff(COLOR_PAIR(CP_MENU_DIM));

    /* ── Row 1: HP and MP bars ── */
    drawBar(1, 0,  20, p->hp, p->maxHp, CP_BAR_HP,  CP_MENU_DIM, "HP");
    drawBar(1, 36, 20, p->mp, p->maxMp, CP_BAR_MP,  CP_MENU_DIM, "MP");

    /* ── Row 2: stats + XP ── */
    attron(COLOR_PAIR(CP_CYAN));
    mvprintw(2, 0, "ATK:%-4d  DEF:%-4d", p->atk, p->def);
    attroff(COLOR_PAIR(CP_CYAN));
    attron(COLOR_PAIR(CP_XP));
    mvprintw(2, 22, "XP: %d / %d", p->xp, p->xpNext);
    attroff(COLOR_PAIR(CP_XP));

    /* ── Status badges row 2 right ── */
    int sx = 42;
    if (p->status == ST_POISON) {
        attron(COLOR_PAIR(CP_POISON) | A_BOLD);
        mvprintw(2, sx, "[POISONED %d]", p->statusTurns);
        attroff(COLOR_PAIR(CP_POISON) | A_BOLD);
    } else if (p->status == ST_STUN) {
        attron(COLOR_PAIR(CP_STUN) | A_BOLD);
        mvprintw(2, sx, "[STUNNED %d]", p->statusTurns);
        attroff(COLOR_PAIR(CP_STUN) | A_BOLD);
    } else if (p->status == ST_DEFEND) {
        attron(COLOR_PAIR(CP_DEFEND) | A_BOLD);
        mvprintw(2, sx, "[DEFENDING]");
        attroff(COLOR_PAIR(CP_DEFEND) | A_BOLD);
    }

    /* ── Map ── */
    int top = 4;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            Cell *cell = &G.map.grid[y][x];
            char ch; int pair, bold;

            if (p->x == x && p->y == y) {
                ch = '@'; pair = CP_PLAYER; bold = 1;
            } else if (!cell->visible) {
                if (!cell->explored) {
                    mvaddch(top + y, x, ' ');
                    continue;
                }
                tileAttr(cell->tile, 0, 1, &pair, &bold);
                ch = (cell->tile == '#') ? '#' : '.';
            } else {
                ch = cell->tile;
                tileAttr(ch, 1, 1, &pair, &bold);
            }

            attr_t a = COLOR_PAIR(pair) | (bold ? A_BOLD : 0);
            attron(a);
            mvaddch(top + y, x, ch);
            attroff(a);
        }
    }

    /* ── Message + controls ── */
    int msgRow = top + MAP_H + 1;

    /* Colour message based on content keywords */
    int msgPair = CP_YELLOW;
    if (strstr(G.message, "damage") || strstr(G.message, "poison") ||
        strstr(G.message, "slain")  || strstr(G.message, "dead"))
        msgPair = CP_FLEE;
    else if (strstr(G.message, "Level") || strstr(G.message, "XP") ||
             strstr(G.message, "Found") || strstr(G.message, "Picked"))
        msgPair = CP_XP;
    else if (strstr(G.message, "escape") || strstr(G.message, "fled"))
        msgPair = CP_CYAN;

    attron(COLOR_PAIR(msgPair) | A_BOLD);
    mvprintw(msgRow, 0, "%s", G.message);
    attroff(COLOR_PAIR(msgPair) | A_BOLD);

    attron(COLOR_PAIR(CP_MENU_DIM));
    mvprintw(msgRow + 1, 0, "WASD/arrows = move    I = inventory    Q = save & quit");
    attroff(COLOR_PAIR(CP_MENU_DIM));
    refresh();
}

/* ── Inventory display ───────────────────────────────────── */
static void showInventory(void) {
    if (G.player.nInv == 0) {
        strcpy(G.message, "Inventory empty.");
        return;
    }
    char buf[128] = "Inventory: ";
    for (int i = 0; i < G.player.nInv; i++) {
        char tmp[40];
        snprintf(tmp, sizeof(tmp), "%s%sx%d",
                 i == 0 ? "" : ", ",
                 G.player.inventory[i].name,
                 G.player.inventory[i].quantity);
        if (strlen(buf) + strlen(tmp) >= sizeof(buf) - 1) break;
        strcat(buf, tmp);
    }
    strncpy(G.message, buf, sizeof(G.message)-1);
    G.message[sizeof(G.message)-1] = 0;
}

/* ── Pickup / descend ───────────────────────────────────── */
static void pickupItem(int li) {
    Loot *l = &G.map.loot[li];
    Item it = l->item;
    if (it.type == IT_WEAPON) {
        G.player.atk += it.value;
        snprintf(G.message, sizeof(G.message), "Found %s! ATK +%d", it.name, it.value);
    } else if (it.type == IT_ARMOR) {
        G.player.def += it.value;
        snprintf(G.message, sizeof(G.message), "Found %s! DEF +%d", it.name, it.value);
    } else {
        if (G.player.nInv < MAX_INV) {
            G.player.inventory[G.player.nInv++] = it;
            snprintf(G.message, sizeof(G.message), "Picked up %s!", it.name);
        } else {
            strcpy(G.message, "Inventory full!");
            return;
        }
    }
    G.map.grid[l->y][l->x].tile = '.';
    for (int i = li; i < G.map.nLoot - 1; i++) G.map.loot[i] = G.map.loot[i+1];
    G.map.nLoot--;
}

static void descendStairs(void) {
    if (G.player.dungeonLevel >= 3) {
        strcpy(G.message, "The dragon is somewhere on this floor!");
        return;
    }
    G.player.dungeonLevel++;
    generateMap(&G.map, G.player.dungeonLevel);
    placePlayer(&G.map, &G.player);
    updateFOV(&G.map, G.player.x, G.player.y);
    snprintf(G.message, sizeof(G.message),
             "You descend to floor %d. The air grows darker...",
             G.player.dungeonLevel);
}

/* ── Combat ─────────────────────────────────────────────── */
static const char *enemyArt(int t) {
    switch (t) {
        case E_GOBLIN:   return "   .--.\n  /o  o\\\n | >--< |\n  \\    /\n  /|  |\\";
        case E_SKELETON: return "   _____\n  |o   o|\n  | --- |\n  |_____|\n    | |\n   /| |\\";
        case E_ORC:      return "  /------\\\n | OO  OO |\n |   WW   |\n |  ||||  |\n  \\------/\n   ||  ||";
        case E_VAMPIRE:  return "  /\\  /\\\n (  \\/  )\n | o  o |\n |  /\\  |\n ( |  | )\n  \\|  |/";
        case E_DRAGON:   return "    /\\   /\\\n   /  \\_/  \\\n  | * BOSS *|\n  (  O   O )\n   \\  ~~~  /\n  //|     |\\\\\n     DRAGON";
    }
    return "  ???";
}

static int enemyArtColor(int t) {
    switch (t) {
        case E_GOBLIN:   return CP_GOBLIN;
        case E_SKELETON: return CP_SKELETON;
        case E_ORC:      return CP_ORC;
        case E_VAMPIRE:  return CP_VAMPIRE;
        case E_DRAGON:   return CP_DRAGON;
    }
    return CP_DEFAULT;
}

static void renderCombat(void) {
    erase();
    Player *p = &G.player;
    Enemy  *e = combatEnemy;

    /* ── Title bar ── */
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    drawCenter(0, CP_TITLE, 1, "══  COMBAT  ══");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    if (e->isBoss) drawCenter(1, CP_MAGENTA, 1, "★  BOSS FIGHT  ★");

    /* ── Enemy panel (left) ── */
    int enemyPair = e->isBoss ? CP_BOSS : CP_ENEMY;
    attron(COLOR_PAIR(enemyPair) | A_BOLD);
    mvprintw(3, 2, "%s", e->name);
    attroff(COLOR_PAIR(enemyPair) | A_BOLD);
    drawBar(4, 2, 20, e->hp, e->maxHp, CP_BAR_ENE, CP_MENU_DIM, "HP");

    /* Enemy status badges */
    if (e->status == ST_POISON) {
        attron(COLOR_PAIR(CP_POISON) | A_BOLD);
        mvprintw(5, 2, "[POISONED %d]", e->statusTurns);
        attroff(COLOR_PAIR(CP_POISON) | A_BOLD);
    }
    if (e->status == ST_STUN) {
        attron(COLOR_PAIR(CP_STUN) | A_BOLD);
        mvprintw(5, 2, "[STUNNED %d]", e->statusTurns);
        attroff(COLOR_PAIR(CP_STUN) | A_BOLD);
    }

    /* Enemy art (type-specific colour, bold for boss) */
    int artPair = enemyArtColor(e->type);
    attron(COLOR_PAIR(artPair) | (e->isBoss ? A_BOLD : 0));
    int row = 7, col0 = 2, c = 0;
    const char *art = enemyArt(e->type);
    for (const char *q = art; *q; q++) {
        if (*q == '\n') { row++; c = 0; }
        else { mvaddch(row, col0 + c, *q); c++; }
    }
    attroff(COLOR_PAIR(artPair) | (e->isBoss ? A_BOLD : 0));

    /* ── Player panel (right) ── */
    int px0 = 42;
    int clrP = classColor(p->pclass);
    attron(COLOR_PAIR(clrP) | A_BOLD);
    mvprintw(3, px0, "%s [%s] Lv.%d", p->name, className(p->pclass), p->level);
    attroff(COLOR_PAIR(clrP) | A_BOLD);

    drawBar(4, px0, 20, p->hp, p->maxHp, CP_BAR_HP, CP_MENU_DIM, "HP");
    drawBar(5, px0, 20, p->mp, p->maxMp, CP_BAR_MP, CP_MENU_DIM, "MP");

    attron(COLOR_PAIR(CP_CYAN));
    mvprintw(6, px0, "ATK:%-4d  DEF:%d", p->atk, p->def);
    attroff(COLOR_PAIR(CP_CYAN));

    /* Player status badges */
    if (p->status == ST_POISON) {
        attron(COLOR_PAIR(CP_POISON) | A_BOLD);
        mvprintw(7, px0, "[POISONED %d]", p->statusTurns);
        attroff(COLOR_PAIR(CP_POISON) | A_BOLD);
    }
    if (p->status == ST_STUN) {
        attron(COLOR_PAIR(CP_STUN) | A_BOLD);
        mvprintw(7, px0, "[STUNNED %d]", p->statusTurns);
        attroff(COLOR_PAIR(CP_STUN) | A_BOLD);
    }
    if (p->status == ST_DEFEND) {
        attron(COLOR_PAIR(CP_DEFEND) | A_BOLD);
        mvprintw(8, px0, "[DEFENDING]");
        attroff(COLOR_PAIR(CP_DEFEND) | A_BOLD);
    }

    /* ── Battle log ── */
    int logRow = 16;
    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(logRow, 0, "── Battle Log ──────────────────────────────");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);

    int start = combatLogN > 6 ? combatLogN - 6 : 0;
    for (int i = start; i < combatLogN; i++) {
        int isLatest = (i == combatLogN - 1);
        int logPair  = isLatest ? CP_WHITE : CP_MENU_DIM;
        attron(COLOR_PAIR(logPair) | (isLatest ? A_BOLD : 0));
        mvprintw(logRow + 1 + (i - start), 2, "%s", combatLog[i]);
        attroff(COLOR_PAIR(logPair) | (isLatest ? A_BOLD : 0));
    }

    /* ── Action panel ── */
    int aRow = 24;
    const char *specials[] = { "", "Shield Bash (20mp)", "Fireball (30mp)", "Shadow Strike (15mp)" };
    attron(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);
    mvprintw(aRow, 0, "── Choose Action ───────────────────────────");
    attroff(COLOR_PAIR(CP_SUBHEAD) | A_BOLD);

    /* Colour each option */
    attron(COLOR_PAIR(CP_MENU_HL) | A_BOLD);  mvprintw(aRow+1, 0,  "[1] Attack");   attroff(COLOR_PAIR(CP_MENU_HL) | A_BOLD);
    mvprintw(aRow+1, 10, "   ");
    attron(COLOR_PAIR(CP_DEFEND)  | A_BOLD);  mvprintw(aRow+1, 13, "[2] Defend");   attroff(COLOR_PAIR(CP_DEFEND) | A_BOLD);
    mvprintw(aRow+1, 23, "   ");
    attron(COLOR_PAIR(CP_GREEN)   | A_BOLD);  mvprintw(aRow+1, 26, "[3] Use Item"); attroff(COLOR_PAIR(CP_GREEN) | A_BOLD);
    mvprintw(aRow+1, 38, "   ");
    attron(COLOR_PAIR(CP_YELLOW)  | A_BOLD);  mvprintw(aRow+1, 41, "[4] %s", specials[p->pclass]); attroff(COLOR_PAIR(CP_YELLOW) | A_BOLD);
    attron(COLOR_PAIR(CP_FLEE)    | A_BOLD);  mvprintw(aRow+2, 0,  "[5] Flee");     attroff(COLOR_PAIR(CP_FLEE) | A_BOLD);

    refresh();
}

/* Item menu in combat */
static int itemMenu(void) {
    Player *p = &G.player;
    while (1) {
        renderCombat();
        int ar = 26;
        attron(COLOR_PAIR(CP_YELLOW) | A_BOLD);
        mvprintw(ar, 0, "── Select Item ─────────────────────  [0] Cancel");
        attroff(COLOR_PAIR(CP_YELLOW) | A_BOLD);
        if (p->nInv == 0) {
            attron(COLOR_PAIR(CP_MENU_DIM));
            mvprintw(ar + 1, 2, "No items!");
            attroff(COLOR_PAIR(CP_MENU_DIM));
        } else {
            for (int i = 0; i < p->nInv && i < 9; i++) {
                /* Colour by item type */
                int itPair = CP_DEFAULT;
                switch (p->inventory[i].type) {
                    case IT_HEALTH:   itPair = CP_GREEN;   break;
                    case IT_MANA:     itPair = CP_BLUE;    break;
                    case IT_ANTIDOTE: itPair = CP_CYAN;    break;
                    case IT_WEAPON:   itPair = CP_RED;     break;
                    case IT_ARMOR:    itPair = CP_YELLOW;  break;
                }
                attron(COLOR_PAIR(itPair));
                mvprintw(ar + 1 + i, 2, "[%d] %s x%d",
                         i + 1, p->inventory[i].name, p->inventory[i].quantity);
                attroff(COLOR_PAIR(itPair));
            }
        }
        refresh();
        int ch = getch();
        if (ch == '0' || ch == 27) return 0;
        if (ch >= '1' && ch <= '9') {
            int idx = ch - '1';
            if (idx >= p->nInv) continue;
            Item *it = &p->inventory[idx];
            char msg[96];
            if (it->type == IT_HEALTH) {
                int nw = p->hp + it->value; if (nw > p->maxHp) nw = p->maxHp;
                snprintf(msg, sizeof(msg), "Used %s: +%d HP", it->name, nw - p->hp);
                p->hp = nw;
            } else if (it->type == IT_MANA) {
                int nw = p->mp + it->value; if (nw > p->maxMp) nw = p->maxMp;
                snprintf(msg, sizeof(msg), "Used %s: +%d MP", it->name, nw - p->mp);
                p->mp = nw;
            } else if (it->type == IT_ANTIDOTE) {
                if (p->status == ST_POISON) { p->status = ST_NONE; p->statusTurns = 0; strcpy(msg, "Antidote cures poison!"); }
                else strcpy(msg, "No poison to cure.");
            } else {
                snprintf(msg, sizeof(msg), "Cannot use %s here.", it->name);
            }
            logPush(msg);
            it->quantity--;
            if (it->quantity <= 0) {
                for (int k = idx; k < p->nInv - 1; k++) p->inventory[k] = p->inventory[k+1];
                p->nInv--;
            }
            return 1;
        }
    }
}

static char *playerAttack(Player *p, Enemy *e, char *out, size_t n) {
    int critChance = (p->pclass == CL_ROGUE) ? 25 : (p->pclass == CL_WARRIOR) ? 10 : 5;
    int isCrit = randn(100) < critChance;
    int dmg = p->atk - e->def + randn(4);
    if (dmg < 1) dmg = 1;
    if (isCrit) dmg = (dmg * 3) / 2;
    e->hp -= dmg; if (e->hp < 0) e->hp = 0;
    if (e->hp <= 0) e->alive = 0;
    if (p->pclass == CL_ROGUE && randn(100) < 20 && e->status == ST_NONE) {
        e->status = ST_POISON; e->statusTurns = 3;
        snprintf(out, n, "You strike for %d damage%s! Enemy poisoned!", dmg, isCrit?" (CRIT!)":"");
        return out;
    }
    snprintf(out, n, "You attack for %d damage%s!", dmg, isCrit?" (CRIT!)":"");
    return out;
}

static int playerSpecial(Player *p, Enemy *e, char *out, size_t n) {
    switch (p->pclass) {
        case CL_WARRIOR: {
            if (p->mp < 20) { logPush("Not enough MP! (need 20)"); return 0; }
            p->mp -= 20;
            int dmg = p->atk * 2 - e->def; if (dmg < 1) dmg = 1;
            e->hp -= dmg; if (e->hp < 0) e->hp = 0;
            if (e->hp <= 0) e->alive = 0;
            e->status = ST_STUN; e->statusTurns = 1;
            snprintf(out, n, "Shield Bash! %d damage + Enemy STUNNED!", dmg);
            return 1;
        }
        case CL_MAGE: {
            if (p->mp < 30) { logPush("Not enough MP! (need 30)"); return 0; }
            p->mp -= 30;
            int dmg = p->atk * 3;
            if (randn(100) < 20) dmg *= 2;
            e->hp -= dmg; if (e->hp < 0) e->hp = 0;
            if (e->hp <= 0) e->alive = 0;
            snprintf(out, n, "FIREBALL! %d magic damage!", dmg);
            return 1;
        }
        case CL_ROGUE: {
            if (p->mp < 15) { logPush("Not enough MP! (need 15)"); return 0; }
            p->mp -= 15;
            int dmg = p->atk * 2;
            e->hp -= dmg; if (e->hp < 0) e->hp = 0;
            if (e->hp <= 0) e->alive = 0;
            e->status = ST_POISON; e->statusTurns = 4;
            snprintf(out, n, "Shadow Strike! %d damage + 4-turn POISON!", dmg);
            return 1;
        }
    }
    return 0;
}

static void enemyTurn(Enemy *e, Player *p) {
    if (e->status == ST_STUN) {
        e->statusTurns--;
        char m[64]; snprintf(m, sizeof(m), "%s is stunned and cannot act!", e->name);
        logPush(m);
        if (e->statusTurns <= 0) e->status = ST_NONE;
        return;
    }
    if (e->status == ST_POISON) {
        int dmg = 3;
        e->hp -= dmg; if (e->hp < 0) e->hp = 0;
        e->statusTurns--;
        if (e->hp <= 0) { e->alive = 0; return; }
        if (e->statusTurns <= 0) e->status = ST_NONE;
    }

    int dmg = 0;
    char msg[96] = "";
    int defTotal = p->def + p->defendBonus;

    if (e->isBoss) {
        int move = e->dragonMove % 3;
        e->dragonMove++;
        if (move == 0) {
            dmg = e->atk - defTotal + randn(5); if (dmg < 1) dmg = 1;
            p->hp -= dmg; if (p->hp < 0) p->hp = 0;
            snprintf(msg, sizeof(msg), "Dragon bites for %d damage!", dmg);
        } else if (move == 1) {
            dmg = (e->atk * 3) / 2 + randn(5); if (dmg < 1) dmg = 1;
            p->hp -= dmg; if (p->hp < 0) p->hp = 0;
            int pois = 0;
            if (randn(100) < 25 && p->status == ST_NONE) { p->status = ST_POISON; p->statusTurns = 3; pois = 1; }
            snprintf(msg, sizeof(msg), "Dragon breathes fire! %d damage%s!", dmg, pois?" + POISON":"");
        } else {
            dmg = (e->atk * 7) / 10 + randn(3); if (dmg < 1) dmg = 1;
            p->hp -= dmg; if (p->hp < 0) p->hp = 0;
            int stun = 0;
            if (randn(100) < 40 && p->status == ST_NONE) { p->status = ST_STUN; p->statusTurns = 1; stun = 1; }
            snprintf(msg, sizeof(msg), "Dragon tail sweeps! %d damage%s!", dmg, stun?" + STUN":"");
        }
    } else {
        switch (e->type) {
            case E_GOBLIN:
            case E_SKELETON:
                dmg = e->atk - defTotal + randn(4); if (dmg < 1) dmg = 1;
                p->hp -= dmg; if (p->hp < 0) p->hp = 0;
                snprintf(msg, sizeof(msg), "%s attacks for %d damage!", e->name, dmg);
                break;
            case E_ORC:
                if (e->hp < e->maxHp / 2 && randn(100) < 30) {
                    snprintf(msg, sizeof(msg), "%s braces for impact! (defending)", e->name);
                } else {
                    dmg = e->atk - defTotal + randn(4); if (dmg < 1) dmg = 1;
                    p->hp -= dmg; if (p->hp < 0) p->hp = 0;
                    snprintf(msg, sizeof(msg), "%s attacks for %d damage!", e->name, dmg);
                }
                break;
            case E_VAMPIRE:
                if (randn(100) < 25) {
                    dmg = (e->atk * 7) / 10; if (dmg < 1) dmg = 1;
                    p->hp -= dmg; if (p->hp < 0) p->hp = 0;
                    e->hp = e->hp + dmg; if (e->hp > e->maxHp) e->hp = e->maxHp;
                    snprintf(msg, sizeof(msg), "%s drains %d HP! (healed)", e->name, dmg);
                } else {
                    dmg = e->atk - defTotal + randn(4); if (dmg < 1) dmg = 1;
                    p->hp -= dmg; if (p->hp < 0) p->hp = 0;
                    snprintf(msg, sizeof(msg), "%s attacks for %d damage!", e->name, dmg);
                }
                break;
        }
    }
    logPush(msg);
}

static void doGameOver(const char *msg) {
    G.state = 1;
    deleteSave();
    erase();
    drawCenter(LINES/2 - 4, CP_GAMEOVER, 1, "╔══════════════════╗");
    drawCenter(LINES/2 - 3, CP_GAMEOVER, 1, "║   GAME  OVER     ║");
    drawCenter(LINES/2 - 2, CP_GAMEOVER, 1, "╚══════════════════╝");
    attron(COLOR_PAIR(CP_FLEE));
    drawCenter(LINES/2 - 0, CP_FLEE, 0, msg);
    attroff(COLOR_PAIR(CP_FLEE));
    char buf[128];
    snprintf(buf, sizeof(buf), "%s · %s", G.player.name, className(G.player.pclass));
    drawCenter(LINES/2 + 2, CP_CYAN, 1, buf);
    snprintf(buf, sizeof(buf), "Level %d  ·  Floor %d  ·  Turn %d",
             G.player.level, G.player.dungeonLevel, G.turn);
    drawCenter(LINES/2 + 3, CP_CYAN, 0, buf);
    drawCenter(LINES/2 + 6, CP_MENU_DIM, 0, "Press any key to return to the menu.");
    refresh();
    getch();
}

static int doWin(void) {
    G.state = 2;
    deleteSave();
    erase();
    drawCenter(LINES/2 - 5, CP_WIN,  1, "╔════════════════════╗");
    drawCenter(LINES/2 - 4, CP_WIN,  1, "║     VICTORY!       ║");
    drawCenter(LINES/2 - 3, CP_WIN,  1, "╚════════════════════╝");
    drawCenter(LINES/2 - 1, CP_CYAN, 0, "The Dragon falls. The dungeon is yours.");
    char buf[128];
    snprintf(buf, sizeof(buf), "%s · %s", G.player.name, className(G.player.pclass));
    drawCenter(LINES/2 + 1, CP_WIN, 1, buf);
    snprintf(buf, sizeof(buf), "Level %d  ·  Turn %d", G.player.level, G.turn);
    drawCenter(LINES/2 + 2, CP_XP, 0, buf);
    drawCenter(LINES/2 + 5, CP_MENU_DIM, 0, "Press any key to return to the menu.");
    refresh();
    getch();
    return 1;
}

static int combatEnd(int won) {
    Enemy *e = combatEnemy;
    if (won) {
        G.player.xp += e->xpReward;
        char gain[128];
        snprintf(gain, sizeof(gain), "%s defeated! +%d XP", e->name, e->xpReward);
        while (G.player.xp >= G.player.xpNext) {
            G.player.xp -= G.player.xpNext;
            G.player.level++;
            G.player.xpNext = (G.player.xpNext * 3) / 2;
            G.player.maxHp += 10; G.player.hp = G.player.maxHp;
            G.player.maxMp += 5;  G.player.mp = G.player.maxMp;
            G.player.atk += 2; G.player.def += 1;
            char tmp[64]; snprintf(tmp, sizeof(tmp), " | LEVEL UP -> Lv.%d!", G.player.level);
            strncat(gain, tmp, sizeof(gain) - strlen(gain) - 1);
        }
        logPush(gain);

        if (e->y >= 0 && e->y < MAP_H && e->x >= 0 && e->x < MAP_W)
            G.map.grid[e->y][e->x].tile = '.';

        if (e->isBoss) { combatEnemy = NULL; return doWin(); }

        combatEnemy = NULL;
        strncpy(G.message, gain, sizeof(G.message)-1); G.message[sizeof(G.message)-1] = 0;
        updateFOV(&G.map, G.player.x, G.player.y);
        renderMap();
        saveGame();
        return 0;
    } else {
        char m[96]; snprintf(m, sizeof(m), "%s was slain by %s...", G.player.name, e->name);
        doGameOver(m);
        return 1;
    }
}

static int enterCombat(Enemy *e);
static int combatLoop(void);

static int enterCombat(Enemy *e) {
    combatEnemy = e;
    logClear();
    char intro[64];
    snprintf(intro, sizeof(intro), "A %s appears! Prepare for battle!", e->name);
    logPush(intro);
    return combatLoop();
}

static int combatLoop(void) {
    Player *p = &G.player;
    Enemy  *e = combatEnemy;

    while (1) {
        renderCombat();
        if (!e->alive || e->hp <= 0) { e->alive = 0; return combatEnd(1); }
        if (p->hp <= 0) return combatEnd(0);

        int ch = getch();
        int action = 0;
        if      (ch == '1') action = 1;
        else if (ch == '2') action = 2;
        else if (ch == '3') action = 3;
        else if (ch == '4') action = 4;
        else if (ch == '5') action = 5;
        else continue;

        if (p->status == ST_DEFEND) { p->status = ST_NONE; p->defendBonus = 0; }

        if (p->status == ST_POISON) {
            p->statusTurns--;
            int dmg = 3; p->hp -= dmg; if (p->hp < 0) p->hp = 0;
            char m[64]; snprintf(m, sizeof(m), "Poison deals %d damage! (%d turns left)", dmg, p->statusTurns);
            logPush(m);
            if (p->statusTurns <= 0) { p->status = ST_NONE; logPush("Poison has faded."); }
            if (p->hp <= 0) return combatEnd(0);
        }
        if (p->status == ST_STUN) {
            p->statusTurns--;
            logPush("You are stunned and cannot act!");
            if (p->statusTurns <= 0) p->status = ST_NONE;
            enemyTurn(e, p);
            if (p->hp <= 0) return combatEnd(0);
            continue;
        }

        char buf[128];
        switch (action) {
            case 1:
                playerAttack(p, e, buf, sizeof(buf));
                logPush(buf);
                break;
            case 2:
                p->status = ST_DEFEND;
                p->defendBonus = p->def;
                logPush("You brace yourself! Defense doubled this turn.");
                break;
            case 3: {
                int used = itemMenu();
                if (!used) continue;
                if (!e->alive || e->hp <= 0) { e->alive = 0; return combatEnd(1); }
                enemyTurn(e, p);
                if (p->hp <= 0) return combatEnd(0);
                continue;
            }
            case 4:
                if (!playerSpecial(p, e, buf, sizeof(buf))) continue;
                logPush(buf);
                break;
            case 5:
                if (randn(100) < 50) {
                    logPush("You fled from battle!");
                    combatEnemy = NULL;
                    strcpy(G.message, "You escaped!");
                    renderMap();
                    return 0;
                }
                logPush("Escape failed!");
                break;
        }

        if (!e->alive || e->hp <= 0) { e->alive = 0; return combatEnd(1); }
        enemyTurn(e, p);
        if (p->hp <= 0) return combatEnd(0);
        if (!e->alive || e->hp <= 0) { e->alive = 0; return combatEnd(1); }
    }
}

/* ── Movement ────────────────────────────────────────────── */
static int tryMove(int dx, int dy) {
    if (G.state != 0) return 0;
    int nx = G.player.x + dx, ny = G.player.y + dy;
    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return 0;
    char tile = G.map.grid[ny][nx].tile;
    if (tile == '#') return 0;

    for (int i = 0; i < G.map.nEnemies; i++) {
        Enemy *e = &G.map.enemies[i];
        if (e->alive && e->x == nx && e->y == ny) {
            if (enterCombat(e)) return 2;
            return 0;
        }
    }

    for (int i = 0; i < G.map.nLoot; i++) {
        if (G.map.loot[i].x == nx && G.map.loot[i].y == ny) {
            pickupItem(i);
            break;
        }
    }

    if (tile == '>') { descendStairs(); renderMap(); saveGame(); return 0; }

    G.player.x = nx; G.player.y = ny;
    G.turn++;

    if (G.player.status == ST_POISON) {
        G.player.statusTurns--;
        int dmg = 3;
        G.player.hp -= dmg; if (G.player.hp < 0) G.player.hp = 0;
        snprintf(G.message, sizeof(G.message),
                 "Poison deals %d damage! (%d turns left)", dmg, G.player.statusTurns);
        if (G.player.statusTurns <= 0) {
            G.player.status = ST_NONE;
            strcpy(G.message, "Poison has faded.");
        }
        if (G.player.hp <= 0) { doGameOver("You succumbed to poison..."); return 2; }
    }

    updateFOV(&G.map, G.player.x, G.player.y);
    renderMap();
    saveGame();
    return 0;
}

/* ── Game loop ───────────────────────────────────────────── */
static void runGame(void) {
    renderMap();
    while (1) {
        int ch = getch();
        int dx = 0, dy = 0;
        switch (ch) {
            case 'w': case 'W': case KEY_UP:    dy = -1; break;
            case 's': case 'S': case KEY_DOWN:  dy =  1; break;
            case 'a': case 'A': case KEY_LEFT:  dx = -1; break;
            case 'd': case 'D': case KEY_RIGHT: dx =  1; break;
            case 'i': case 'I':
                showInventory(); renderMap(); continue;
            case 'q': case 'Q':
                saveGame(); return;
            default: continue;
        }
        int r = tryMove(dx, dy);
        if (r == 2) return;
    }
}

/* ── main ────────────────────────────────────────────────── */
int main(void) {
    _seed = (uint32_t)time(NULL) ^ (uint32_t)getpid();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) initColors();

    if (LINES < 28 || COLS < 70) {
        endwin();
        fprintf(stderr, "Terminal too small. Need at least 70x28 (have %dx%d)\n", COLS, LINES);
        return 1;
    }

    int running = 1;
    while (running) {
        drawTitle();
        int ch = getch();
        switch (ch) {
            case '1': {
                char name[24]; int cls;
                if (charScreen(name, &cls)) {
                    newGame(name, cls);
                    deleteSave();
                    runGame();
                }
            } break;
            case '2':
                if (hasSave() && loadSave()) { runGame(); }
                break;
            case '3':
                drawHowTo();
                getch();
                break;
            case '4':
            case 'q': case 'Q':
                running = 0;
                break;
            default: break;
        }
    }

    endwin();
    printf("Thanks for playing Terminal Warrior! Farewell, adventurer.\n");
    return 0;
}