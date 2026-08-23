# Tetrivibes

Classic-style multiplayer tetromino combat written in C++17 with Qt 5.

Up to six players, special blocks, partyline chat, LAN host/join, and a practice mode against bots.

## Build

```bash
sudo apt install qtbase5-dev cmake g++
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tetrivibes
```

## Play

- **Host Game** — start a server (default port `31457`) and wait for friends
- **Join Game** — connect to a host IP/port
- **Practice vs Bots** — local match against 1–5 CPU opponents

The last player who can still place pieces wins.

## Controls

| Key | Action |
| --- | --- |
| ← → | Move |
| ↑ / X | Rotate clockwise |
| Z / Ctrl | Rotate counter-clockwise |
| ↓ | Soft drop |
| Space | Hard drop |
| 1–6 | Use first special on that player slot |
| Enter | Focus chat |

## Specials

Clearing lines collects any specials in those rows and plants more on your field.

| Letter | Name | Effect |
| --- | --- | --- |
| a | Add Line | Garbage line on the target |
| c | Clear Line | Remove the target's bottom line |
| b | Clear Specials | Turn target specials into normal blocks |
| r | Random Clear | Delete random blocks |
| o | Block Bomb | Explode bombs and scatter nearby blocks |
| q | Blockquake | Shift each row left/right |
| g | Gravity | Collapse columns and clear lines |
| s | Switch Field | Swap fields with the target |
| n | Nuke Field | Wipe the target's field |

Helpful specials (`c`, `g`, `n`, `b`) are worth using on yourself. The rest are attacks.
