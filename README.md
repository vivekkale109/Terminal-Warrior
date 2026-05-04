# Terminal Warrior

A turn-based roguelike dungeon crawler for the terminal, written in C using **ncurses**. Explore procedurally generated dungeons, fight monsters, level up, and slay the Dragon boss.

## Features

- **Procedural Dungeons** — Room-and-corridor generation with fog-of-war and field-of-view.
- **3 Character Classes** — Warrior (Shield Bash), Mage (Fireball), Rogue (Shadow Strike).
- **Turn-Based Combat** — Status effects (poison, stun, defend), enemy AI, and a Dragon boss fight.
- **Inventory System** — Potions, scrolls, and equipment pickups.
- **Save / Load** — Binary save file stored at `~/.terminal_warrior_save`.
- **Colorful TUI** — Title screen, character creator, HUD, and scrollable battle log.

## Requirements

- A C compiler (`gcc` or `clang`)
- The **ncurses** development library
  - Debian/Ubuntu: `sudo apt install libncurses-dev`
  - Fedora: `sudo dnf install ncurses-devel`
  - macOS: `brew install ncurses` (or use the system version)

## Build

```bash
gcc -O2 -o terminal_warrior terminal_warrior.c -lncurses
```

## Run

```bash
./terminal_warrior
```

> Use a terminal at least **80x24** in size for the best experience.

## Controls

### Map / Exploration
| Key | Action |
|-----|--------|
| `W` `A` `S` `D` / Arrow keys | Move |
| `I` | Open inventory |
| `Q` | Save and quit |

### Combat
| Key | Action |
|-----|--------|
| `1` | Attack |
| `2` | Defend (reduce incoming damage) |
| `3` | Use item |
| `4` | Special ability (class-specific) |
| `5` | Flee |

## Map Legend

| Symbol | Meaning |
|--------|---------|
| `@` | Player |
| `.` | Floor |
| `#` | Wall |
| `+` | Door / corridor |
| `g` `o` `D` | Enemies (Goblin, Orc, Dragon) |
| `!` `?` `$` | Items (potion, scroll, gold) |
| `>` | Stairs to next level |

## Save Files

Progress is saved to `~/.terminal_warrior_save` when you quit with `Q`. Delete this file to start a fresh run.

## License

Released under the MIT License. Free to play, modify, and share.
