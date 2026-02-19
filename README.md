# DevilMayCry3-mods

Found these mods I wrote for DMC 3 almost a decade ago on my older PC. Uploading here so they dont get lost again. Should still work if you have the old 2006 Ubisoft port. If you use this and find any issues, please raise it as an issue in this repo and I will do my best to resolve it. 

DISCLAIMER: UNOFFICIAL FAN MODS, REQUIRES ORIGINAL COPY OF THE GAME

![Jackpot](https://preview.redd.it/pht6zch48ox71.jpg?width=1080&crop=smart&auto=webp&s=a181cbbf0fc9436e7f78174d3604bcd7536e42e4)

---

## What is this

Trainer + DX9 overlay for **DMC3 Special Edition** (2006 Ubisoft PC port). This was removed from Steam sometime in 2024 based on my research, but if you purchased it prior to that it still works. 
<mark> I do not encourage to pirate games</mark>

Has the standard trainer stuff (inf HP/DT, damage mult, one-hit kill) but the main thing is the overlay, draws an **enemy HP bar** above whoever you're locked onto, and a **combo tracker** that shows total hits + damage dealt. Also adds **turbo mode** which the 2006 port never had (was PS2 only).

## Hotkeys

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

**Enemy HP bar** — shows above the locked-on enemy(with gradient and numbers). (Doesn't show for all enemeies because how that data is present in the game(flat array, each type different struct size)). Lockon pointer's way more reliable.

**Combo tracker** — hooks the damage calc function so it catches every hit. Resets after 2s of no hits. Text goes orange at 20+ and red at 50+.

**Turbo** — patches entity tick rate. 1.2x = PS2 turbo speed. Configurable in the ini.

## How to use

1. Copy `injector.exe`, `mod2006.dll`, and `dmc3_mod.ini` next to `dmc3se.exe`
2. Start the game
3. Run `injector.exe` (admin if it fails)
4. Check `dmc3_mod.log` if something's off

---

If you want to build from source, you need Visual Studio and CMake. Just run `build.bat`. Has to be 32-bit.

--- 
This made me write and review cpp after so long. This is special.