# CoreInk Tamagotchi

A classic virtual pet for the **M5Stack CoreInk (K048)**, a 1.54" 200×200 e-ink ESP32 board, written in C++ with PlatformIO and M5Unified. The same code also builds as a **PC simulator**, so you can play and tune it without the hardware.

## The game

| Stage | Lasts | Notes |
|---|---|---|
| Egg | 5 min | hatches by itself |
| Baby | 1 h | hungry often, poops often |
| Child | 1 day | |
| Teen | 2 days | |
| Adult | 10–25 days | one of four, depending on how you raised it (below) |

Which adult you get is decided at the end of Teen, in this order:

| Adult | How | Lives |
|---|---|---|
| Gruñón | more than 3 care mistakes | 10 days |
| Jedi | won *Fuerza* 8+ times (since Child) and Discipline ≥ 60 | 25 days |
| Glotón | 25+ sweets (since Child) or overweight | 14 days |
| Mandaloriano | everyone else | 20 days |

- **Stats:** Food, Fun, Energy, Clean, Health and Discipline. Look at them under *Más → Info*.
- **Menu:** Comer · Jugar · Luz · Lavar · Cura · Regañar · Más (Tienda / Info / Álbum).
- It **sleeps from 21:00 to 08:00**. Turn the lights off when it goes to sleep, or it counts as a care mistake.
- **Waking it at night:** with the lights off, turn them on (*Luz*) and it wakes up, a bit grumpy (−10 Fun). It can eat and play as usual. Turn the lights off to put it back to bed; if you don't, it falls asleep again by itself after an hour (with the lights still on, so turn them off).
- **Naps:** tired during the day? Turn the lights off (*Luz*) while Energy is below 80 and it naps. A nap fills Energy completely in **5 minutes** (40 Energy left to go takes 2). It wakes by itself when Energy is full, or when you turn the light back on. A nap still running at 21:00 just becomes the night's sleep.
- Poop lowers Clean. Dirt, starvation or being overweight can make it **sick**. Sickness drains Health until you give medicine.
- Health 0 means it dies. Press the dial to hatch a new egg.
- A **care mistake** happens when hunger or fun sits at 0, or sickness goes untreated, for 30 minutes.
- An inverted **"!"** in the status bar means it needs you. The device also beeps once when that starts.

### Minigames
*Jugar* opens five games plus *Vestir*. All are turn-based, so they suit e-ink. Winning gives more Fun than losing and earns **ranas** (frogs, the currency).
- **Izquierda o derecha.** 5 rounds: guess which way Grogu will look (dial up = left, down = right). 3 or more correct wins.
- **Más/Menos.** A number from 1 to 9 is shown: will the next one be higher (dial up) or lower (dial down)? It's never the same number twice. Keep guessing until you miss; 4 in a row wins, and 8 in a row ends the game. One rana per guess in a row past 3.
- **Fuerza.** Memorise a sequence of ▲ ▼ ●, then repeat it with dial up / down / press. It starts at 3 moves and grows to 8. Repeating 4 or more counts as a win and as Force training for the Jedi.

- **Ordenar.** KonMari tidying: six piles of folded clothes are shuffled on a shelf. Sort them from smallest to biggest. Move the cursor with the dial, press to pick a pile up, and press on another pile to swap them (top button puts it back). It always counts as a win: 3 stars (and ranas) for the fewest possible swaps, 2 stars for up to 2 extra, 1 otherwise. 3 stars says *¡Chispa de alegría!*
- **¿Alegría?** First it shows the 3 things that spark joy for Grogu (from: Bola, Rana, Macarón, calcetín, camiseta, libro). Then 8 things come up one by one: keep it (dial up) or thank it and let it go (dial down). 6 or more right calls win, one rana per right call past 5.

### Vestir
*Jugar → Vestir* dresses Grogu up. Turn the dial to pick *Cabeza* (Gorro, Corona, Lazo, Gafas), *Cuello* (Bufanda, Pajarita, Collar) or *¡Listo!*. Press to change the item, with a live preview, or to put the outfit on. The outfit stays on the home screen, and on the sleeping frame too. A new outfit gives +15 Fun, at most once an hour.

### Tienda
*Más → Tienda* sells, for ranas: **Macarón** (3, big Fun boost, sweet), **Huevos** (2, a big meal), **Rana** (1, a little food and fun), **Bacta** (4, cures and restores Health, used from *Cura*) and the **Bola** of the Razor Crest (6, permanent, +Fun on every win). Foods you own appear in *Comer*.

### Tantrums and discipline
Sometimes it calls you ("!") without needing anything. That's a tantrum: *Regañar* it to raise Discipline. Scolding when it did nothing wrong makes it sad. A tantrum ignored for 30 minutes counts as a care mistake. Low discipline means more tantrums.

### Random events
While it's awake, roughly once every 7 hours something happens: **Mando visits** with a gift, **a frog hops by** (+2 ranas), or Grogu **Force-steals a macaron** (fun, but bad for discipline). Events that happen while the device is off are shown the next time you wake it.

### Álbum
*Más → Álbum* keeps what you've discovered **across all generations**: the adults raised, the items and events seen, and your best scores (page 3).

All tunable numbers live in [`src/game/rules.h`](src/game/rules.h).

The pet is drawn Grogu-style (sprites in [`src/ui/sprites.h`](src/ui/sprites.h)). The on-screen UI is in **Spanish**: menu Comer · Jugar · Luz · Lavar · Cura · Info. The built-in font is ASCII-only, so á é í ó ú ü ñ Ñ ¡ ¿ come from small bitmaps in [`src/ui/render.cpp`](src/ui/render.cpp). Add more there if you need other characters.

## Controls

| CoreInk | Simulator | Action |
|---|---|---|
| Dial up | ↑ | move cursor |
| Dial down | ↓ | move cursor |
| Dial press | Enter / Space / → | select |
| Top button | Esc / Backspace / ← | back |
| Side (power) button | – | wake up, or force a full refresh |
| – | F | fast-forward 1 hour |
| – | S | toggle 1× / 60× time speed |
| – | N | new pet |
| – | Z | power off now (any key wakes) |
| – | 1…6 | window zoom |

## Power / battery

After **45 s without input** the device draws a "sleeping" frame. E-ink keeps the image with no power. It then cuts its own power and sets the RTC to wake it every **15 min**. On each wake-up it updates the pet and redraws only if something visible changed. It beeps if the pet needs you, then powers off again. To play, press the **side button**.

## Project layout

```
src/game/      pure game logic (pet.cpp, minigames.cpp, rules.h); unit tested
src/ui/        sprites (editable ASCII art) and screen rendering
src/hal/       hardware layer: hal_coreink.cpp (device) / hal_sim.cpp (PC)
src/main.cpp   screens, menu, refresh and power policy
test/          Unity tests: pio test -e test
```

## Setup (once)

1. Install **PlatformIO**. Either:
   - install the VS Code extension "PlatformIO IDE", or
   - run `python -m pip install --user platformio` (the CLI ends up in `%APPDATA%\Python\Python312\Scripts\pio.exe`).
2. For the simulator and tests only, install **MSYS2** with gcc and SDL2:
   ```powershell
   winget install MSYS2.MSYS2
   C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2"
   ```
   Then add `C:\msys64\mingw64\bin` to your PATH. PlatformIO's `native` platform uses that gcc, and the simulator needs `SDL2.dll` from there.

## Simulate on the PC

```powershell
pio run -e sim -t exec     # build and open the simulator window
pio test -e test           # run the game-logic tests
```

The simulator saves to `sim_save.bin` in the project folder. Delete the file, or press **N**, to start over.

## Flash the real device

1. Connect the CoreInk with USB-C. If no new COM port shows up, install the **CH9102** USB-serial driver from M5Stack's docs (some units use a CP210x chip instead).
2. If it doesn't power on, press the side button.
3. Run:
   ```powershell
   pio run -e coreink -t upload
   pio device monitor          # optional serial log
   ```
4. On the first boot the RTC clock is set to the time the firmware was **compiled**. Flash right after building to get the clock roughly right.

The CoreInk normally auto-resets into the bootloader. If the upload can't connect, unplug it, press the side button and try again.
