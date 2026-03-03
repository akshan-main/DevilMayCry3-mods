# DevilMayCry3-mods

Found mods I wrote for DMC 3 almost a decade ago on my older PC, uploading here so they dont get lost again. Also added HD Collection support while I was at it. If you use this and find any issues, please raise it as an issue in this repo and I will do my best to resolve it.

DISCLAIMER: UNOFFICIAL FAN MODS, REQUIRES ORIGINAL COPY OF THE GAME

![Jackpot](https://preview.redd.it/pht6zch48ox71.jpg?width=1080&crop=smart&auto=webp&s=a181cbbf0fc9436e7f78174d3604bcd7536e42e4)

---

Supports two versions of DMC3 Special Edition on PC:
- **2006 Ubisoft port** (32-bit `dmc3se.exe`) — trainer + DX9 overlay with enemy HP bar and combo tracker
- **HD Collection** (64-bit, Steam) — trainer with mid-mission style switching

## 2006 Port

Trainer + DX9 overlay for the original 2006 Ubisoft port. This was removed from Steam sometime in 2024 based on my research, but if you purchased it prior to that it still works. I do not encourage pirating games.

| Key | What it does |
|---|---|
| F1 | Infinite HP |
| F2 | Infinite DT |
| F3 | Cycle damage multiplier (1x/2x/5x/10x) |
| F4 | One-hit kill |
| F5 | Turbo mode (1.2x speed) |
| F6 | Infinite air hike |
| F7 | Toggle enemy HP bar |
| F8 | Toggle combo tracker |

**Enemy HP bar** — shows above the locked-on enemy (gradient + numbers). Doesn't show for all enemies because of how enemy data is structured in memory (each type is a different struct size). Lockon pointer is way more reliable.

**Combo tracker** — hooks the damage calc function so it catches every hit. Resets after 2s of no hits. Text goes orange at 20+ and red at 50+.

**Turbo** — hotkey for turbo mode. The game has turbo in the main menu but this lets you toggle it mid-game. Configurable speed in the ini.

### Setup (2006 port)

1. Copy `injector.exe`, `mod2006.dll`, and `dmc3_mod.ini` next to `dmc3se.exe`
2. Start the game
3. Run `injector.exe` (admin if it fails)
4. Check `dmc3_mod.log` if something's off

---

## HD Collection

Trainer for the Steam HD Collection (64-bit). Uses a `dinput8.dll` proxy so no separate injector is needed — drops into the game folder and loads automatically.

| Key | What it does |
|---|---|
| F1 | Infinite HP |
| F2 | Infinite DT |
| F3 | Turbo (1.2x) |
| F4 | Infinite air hike |
| F5 | Toggle mid-mission style switch |
| F6 | Cycle style (when style switch is on) |

**Style switch** — normally you can only change style at a divinity statue. This lets you switch mid-mission with F5 to enable, then F6 to cycle through all 6 styles (Trickster, Swordmaster, Gunslinger, Royalguard, Quicksilver, Doppelganger).

### Setup (HD Collection)

1. Copy `dinput8.dll`, `modhd.dll`, and `dmc3_mod.ini` into the game folder
2. Start the game — the mod loads automatically
3. Check `dmc3_mod.log` if something's off

---

## Building from source

Visual Studio + CMake.

- `build.bat` — 2006 port (32-bit), output in `build/out/Release/`
- `build_hd.bat` — HD Collection (64-bit), output in `build_hd/out_hd/Release/`

---
This made me write and review cpp after so long. This is special.
