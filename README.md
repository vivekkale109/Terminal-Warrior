# ⚔️ Terminal Warrior

> A terminal-based ASCII dungeon RPG written in C — built as a final-year engineering project.

```
  ████████╗███████╗██████╗ ███╗   ███╗██╗███╗   ██╗ █████╗ ██╗
     ██╔══╝██╔════╝██╔══██╗████╗ ████║██║████╗  ██║██╔══██╗██║
     ██║   █████╗  ██████╔╝██╔████╔██║██║██╔██╗ ██║███████║██║
     ██║   ██╔══╝  ██╔══██╗██║╚██╔╝██║██║██║╚██╗██║██╔══██║██║
     ██║   ███████╗██║  ██║██║ ╚═╝ ██║██║██║ ╚████║██║  ██║███████╗
     ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝

  ██╗    ██╗ █████╗ ██████╗ ██████╗ ██╗ ██████╗ ██████╗
  ██║    ██║██╔══██╗██╔══██╗██╔══██╗██║██╔═══██╗██╔══██╗
  ██║ █╗ ██║███████║██████╔╝██████╔╝██║██║   ██║██████╔╝
  ██║███╗██║██╔══██║██╔══██╗██╔══██╗██║██║   ██║██╔══██╗
  ╚███╔███╔╝██║  ██║██║  ██║██║  ██║██║╚██████╔╝██║  ██║
   ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═╝
```

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Installation & Compilation](#installation--compilation)
- [How to Play](#how-to-play)
- [Controls](#controls)
- [Classes](#classes)
- [Combat System](#combat-system)
- [Map & Dungeon](#map--dungeon)
- [Items & Inventory](#items--inventory)
- [Save & Load](#save--load)
- [Example Gameplay](#example-gameplay)
- [Technical Design](#technical-design)
- [Known Warnings](#known-warnings)
- [Future Improvements](#future-improvements)

---

## Overview

**Terminal Warrior** is a fully playable, terminal-based roguelike RPG written in pure C. You explore a procedurally generated dungeon, fight monsters in turn-based combat, collect loot, level up, and face a Dragon Boss on Floor 5.

The project demonstrates:
- Modular C programming across multiple `.c`/`.h` files
- Real-time keyboard input using `termios` (no Enter key required)
- Procedural dungeon generation (BSP-style room placement)
- Fog of war with circular field-of-view
- Struct-based game state, binary save/load, and dynamic enemy AI

---

## Features

| Feature | Details |
|---|---|
| 🧙 Character Creation | Choose name + class (Warrior / Mage / Rogue) with unique stats |
| 🗺️ Procedural Dungeon | Random rooms + L-shaped corridors, regenerated each floor |
| 🌫️ Fog of War | Circular FOV radius 5; explored tiles shown dimly |
| ⚔️ Turn-Based Combat | Attack, Defend, Use Item, Special Ability, Flee |
| 🧠 Enemy AI | Scales difficulty per floor; boss uses multi-phase strategy |
| 💀 5 Enemy Types | Goblin, Skeleton, Orc, Vampire, Dragon (Boss) |
| 🎒 Inventory System | Stacking items, use mid-combat, equip weapons/armor |
| ✨ Status Effects | Poison (DoT), Stun (skip turn), cleared by Antidote |
| 💥 Critical Hits | Class-based crit chance; Rogue has highest |
| 📈 Leveling System | XP → Level Up → stat increases (class-specific scaling) |
| 💾 Save / Load | Binary save file with magic-number header + version check |
| 🐉 Boss Fight | Dragon on Floor 5; defeat it to win the game |
| 🎨 ANSI Colors | Full color-coded UI, health/mana/XP bars, ASCII art |

---

## Project Structure

```
terminal_warrior/
├── main.c          — Entry point: title screen, menu, game loop, win/lose
├── types.h         — All shared structs, enums, constants (included by all modules)
├── player.h/.c     — Character creation, leveling, status effects, class art
├── map.h/.c        — Dungeon generation, FOV, rendering, HUD, movement
├── combat.h/.c     — Turn-based combat loop, enemy AI, special abilities
├── inventory.h/.c  — Item management: add, use, stack, display
├── save.h/.c       — Binary save/load with magic-number validation
├── Makefile        — Build system (release, debug, run, clean)
└── README.md       — This file
```

### Module Dependency Graph

```
main.c
 ├── types.h       (shared by all)
 ├── player.h/c
 ├── map.h/c
 │    └── player.h
 ├── combat.h/c
 │    ├── player.h
 │    └── inventory.h
 ├── inventory.h/c
 │    └── player.h
 └── save.h/c
```

---

## Requirements

- **OS:** Linux or any Unix-like system (macOS also works)
- **Compiler:** GCC (any version supporting C11)
- **Libraries:** Standard C library + `libm` (math) — no external dependencies
- **Terminal:** Any terminal emulator that supports ANSI escape codes (virtually all modern ones do)

> **Arch Linux users:** The provided Makefile already includes `-D_POSIX_C_SOURCE=200809L` which is required on stricter gcc configurations.

---

## Installation & Compilation

### 1. Clone or download the source files

Place all `.c`, `.h`, and `Makefile` files in the same directory.

### 2. Compile

```bash
make
```

This produces the `./terminal_warrior` binary.

### 3. Run

```bash
./terminal_warrior
```

### One-step build and run

```bash
make run
```

### Debug build (with AddressSanitizer)

```bash
make debug
./terminal_warrior
```

### Clean build artifacts

```bash
make clean
```

---

## How to Play

### Objective
Descend through 5 floors of the dungeon and defeat the **Dragon Boss** on Floor 5.

### The Map

```
##################################################
#..........#.....E.......#........................#
#..........#.............#........T...............#
#..........#######.#######........................#
#..@.......#.............#........................#
#..........#.............######################..#
#..........#..........>..#........................#
##################################################
```

| Symbol | Meaning |
|--------|---------|
| `@` | Your character |
| `#` | Wall (impassable) |
| `.` | Floor (walkable) |
| `E` | Enemy |
| `T` | Treasure chest (auto-picked up on contact) |
| `>` | Stairs to next floor |
| `B` | Boss enemy |

Tiles you haven't explored yet appear as **empty/dark**. Tiles you've seen before but aren't currently in your FOV appear **dimly** (fog of war).

### Goal
1. Explore each floor using WASD
2. Fight or avoid enemies
3. Pick up treasure for random loot
4. Find the stairs `>` to go deeper
5. Reach Floor 5 and defeat the Dragon

---

## Controls

### Exploration (Map)

| Key | Action |
|-----|--------|
| `W` | Move up |
| `A` | Move left |
| `s` | Move down |
| `D` | Move right |
| `i` | Open inventory |
| `p` | View full stats |
| `S` | Save / Load menu |
| `Q` | Quit to main menu |

> Input is **real-time** — no Enter key needed. Uses raw terminal mode via `termios`.

### Combat Menu

| Key | Action |
|-----|--------|
| `1` | Attack (physical damage) |
| `2` | Defend (halves incoming damage next turn) |
| `3` | Use Item (opens inventory) |
| `4` | Special Ability (costs MP) |
| `5` | Flee (50% chance to escape) |

---

## Classes

### ⚔️ Warrior
```
  __|__
 / o o \
|  ---  |
 \ ___ /
  |   |
```
| Stat | Value |
|------|-------|
| HP | 120 |
| MP | 30 |
| ATK | 18 |
| DEF | 12 |
| SPD | 8 |

**Special:** *Shield Bash* — deals 1.5× damage, no MP cost.  
**Best for:** Beginners. Highest survivability.

---

### 🔮 Mage
```
   /\
  /^^\
 / ** \
|  **  |
 \    /
```
| Stat | Value |
|------|-------|
| HP | 60 |
| MP | 100 |
| ATK | 22 |
| DEF | 6 |
| SPD | 10 |

**Special:** *Fireball* — deals 2.5× damage, costs 20 MP.  
**Best for:** High-risk/reward. Melts bosses fast.

---

### 🗡️ Rogue
```
  _/\_
 (o_o)
  \|/
  / \
```
| Stat | Value |
|------|-------|
| HP | 85 |
| MP | 50 |
| ATK | 20 |
| DEF | 8 |
| SPD | 14 |

**Special:** *Backstab* — deals 2× damage, costs 15 MP.  
**Best for:** Experienced players. Highest crit rate + speed.

---

## Combat System

### Turn Order
1. **Player acts** (unless Stunned)
2. **Status effects tick** (Poison deals 4–7 damage/turn)
3. **Enemy acts** (AI-driven)

### Damage Formula
```
damage = attacker.attack - (defender.defense / 2) + rand(-3..+3)
```
- Minimum damage is always **1**
- **Critical hit** multiplies damage by **1.5×**
- **Defend** action reduces incoming damage by **50%** for one turn

### Critical Hit Chance
| Class | Crit % |
|-------|--------|
| Warrior | 10% |
| Mage | 8% |
| Rogue | 20% |

### Status Effects
| Effect | Source | Impact |
|--------|--------|--------|
| Poison | Vampire attack | 4–7 HP/turn for 3 turns |
| Stun | Dragon attack | Skip your next turn |
| Defend | Player action | -50% incoming damage for 1 turn |

Cure Poison/Stun with an **Antidote** from inventory.

### Enemy AI Behavior
| Enemy | Strategy |
|-------|---------|
| Goblin | Always attacks |
| Skeleton | Always attacks |
| Orc | Attacks; taunts at low HP |
| Vampire | Attacks; applies Poison randomly |
| Dragon (Boss) | Attacks; uses Stun; enrages at <50% HP for 2× damage |

---

## Map & Dungeon

### Generation Algorithm
1. Fill entire 50×20 grid with `#` walls
2. Place up to **12 random rooms** (min 4×4, max 10×6), checking for overlaps
3. Connect consecutive rooms with **L-shaped corridors**
4. Scatter **enemies** and **treasure chests** randomly on floor tiles
5. Place **stairs** `>` in the last room
6. On **Floor 5**, spawn the Dragon Boss instead of regular enemies

### Fog of War
- Uses a **circular FOV** with radius 5
- Tiles in range: `visible = 1`
- Previously seen tiles: `explored = 1` (shown dimly)
- Unseen tiles: rendered as empty space

### Floor Scaling
Each floor increases enemy stats:
```c
hp     = base_hp  * (1 + 0.3 * dungeon_level)
attack = base_atk * (1 + 0.2 * dungeon_level)
```

---

## Items & Inventory

### Item Types

| Item | Effect | How to Get |
|------|--------|-----------|
| Health Potion | Restore 30 HP | Starting inventory, enemy drops, treasure |
| Mana Potion | Restore 25 MP | Enemy drops, treasure |
| Weapon | +ATK permanently | Treasure chests |
| Armor | +DEF permanently | Treasure chests |
| Antidote | Clear Poison/Stun | Treasure chests |

- Inventory holds up to **10 slots**
- Identical items **stack** (quantity shown)
- Weapons and Armor are **consumed on use** and permanently boost stats
- Items can be used **during combat** (counts as your turn)

### Starting Inventory
All classes begin with **2× Health Potions**.

---

## Save & Load

### Save File
- Location: `savegame.dat` (same directory as the binary)
- Format: Binary blob with a magic header `TWAR` + version number
- Contains: **entire game state** — player stats, inventory, map, enemy positions, floor

### Usage
- Press **`S`** during exploration to open the Save/Load menu
- Choose `[1] Save` or `[2] Load`
- The game detects a corrupt or version-mismatched save and warns you gracefully

### Notes
- Save is **deleted** on death or winning (encourages re-run)
- If you quit without saving, progress is lost

---

## Example Gameplay

```
$ ./terminal_warrior

[Title screen appears]

MAIN MENU
  [1] New Game
  [2] Continue (no save)
  [3] How to Play
  [4] Quit

Choice: 1

Enter your hero's name: Vivek

Choose your class:
  [1] Warrior   HP:120  ATK:18  DEF:12  MP:30
  [2] Mage      HP: 60  ATK:22  DEF: 6  MP:100
  [3] Rogue     HP: 85  ATK:20  DEF: 8  MP:50

Your choice: 3

[Rogue ASCII art shown]
Welcome, Vivek the Rogue!

[Map renders with fog of war]
##################################################
#                                                #
#   @..........                                  #
#   ...........                                  #
#                                                #

[Player moves with WASD...]
[Walks into an E — combat starts]

╔══════════════════════════════════════════════════╗
║  COMBAT: Vivek vs Goblin                         ║
╠══════════════════════════════════════════════════╣
║  YOUR HP: ████████████████░░░░  72/85            ║
║  ENEMY HP: ███░░░░░░░░░░░░░░░   12/35            ║
╠══════════════════════════════════════════════════╣
║  [1] Attack  [2] Defend  [3] Item                ║
║  [4] Backstab (15 MP)    [5] Flee                ║
╚══════════════════════════════════════════════════╝

Choice: 4

  ★ BACKSTAB! Critical hit! Dealt 38 damage to Goblin!

  ██╗   ██╗██╗ ██████╗████████╗ ██████╗ ██████╗ ██╗
  You defeated: Goblin!
  Gained 20 XP!
  Loot: Health Potion dropped!

[Continue exploring, descend stairs, reach Floor 5...]
[Dragon Boss fight — defeat it to see the win screen!]
```

---

## Technical Design

### Key Structs (`types.h`)

```c
typedef struct {
    char name[32];   PlayerClass pclass;
    int hp, max_hp,  mp, max_mp;
    int attack,      defense, speed;
    int level,       xp, xp_to_next;
    int x, y;
    StatusEffect status;
    Inventory inv;
} Player;

typedef struct {
    Cell   grid[MAP_HEIGHT][MAP_WIDTH];  // 50×20 tile grid
    Enemy  enemies[MAX_ENEMIES];
    Item   loot[MAX_ITEMS];
    int    stairs_x, stairs_y;
} GameMap;

typedef struct {
    Player  player;
    GameMap map;
    GameState state;
    int     turn;
    char    message[256];
} Game;
```

### Raw Terminal Input
```c
// Switch to raw mode — reads one keypress instantly, no Enter needed
struct termios raw = orig;
raw.c_lflag &= ~(ICANON | ECHO);
tcsetattr(STDIN_FILENO, TCSANOW, &raw);
char c = getchar();
tcsetattr(STDIN_FILENO, TCSANOW, &orig);  // restore
```

### Save Format
```
[4 bytes]  Magic = 0x54574152 ("TWAR")
[4 bytes]  Version = 1
[N bytes]  Raw Game struct (entire game state)
```

---

## Known Warnings

These are **non-fatal compiler warnings** that do not affect gameplay:

| Warning | Location | Reason |
|---------|----------|--------|
| `unused parameter 'last_log'` | `combat_render()` | Parameter reserved for future use |
| `strncpy bound equals size` | `combat_log()` | Conservative buffer handling |
| `ignoring return value of fgets/fread` | `save.c`, `inventory.c` | Input validation handled contextually |

All warnings are **informational only** — zero errors on GCC with `-std=c11`.

---

## Future Improvements

- [ ] Multiple save slots
- [ ] More enemy types and floor themes (ice dungeon, lava cave)
- [ ] Ranged attacks and spell variety for Mage
- [ ] Shop NPCs on certain floors
- [ ] Minimap in HUD corner
- [ ] High score / death log persistence
- [ ] Config file for keybindings and difficulty
- [ ] Sound effects via terminal bell or external lib

---

## License

This project is released for educational use. Feel free to extend, fork, or submit it as coursework — just understand every line you submit!

---

*Built with ❤️ in C — no engines, no frameworks, just a terminal and imagination.*
