# Little8-lua

A small fantasy console built on [Little-8](https://github.com/...), with Lua cartridge scripting. Write one `running()` function in `cart.lua` — Little8-lua handles the window, input, sound, and rendering loop.

```lua
function running()
    cls(0)
    if btn(0, 0) then tone(440, 4, 50, 0) end
    oval(60, 60, 8, 8, 8, true)
end
```

## Specs

- **Screen:** 128×128, 16-color palette (Sweetie 16)
- **Sprites:** 256 slots, any size up to 128×128
- **Save slots:** 64 × int64 (512 bytes on disk)
- **Audio:** 4 channels (pulse 12.5%, pulse 25%, triangle, noise)
- **Input:** 2 players × 6 buttons
- **Scripting:** Lua 5.4
- **Code size:** no limit

## Build

Requires CMake ≥ 3.16 and a C11 toolchain. Raylib 6.0 (Windows, MinGW-w64) and Lua 5.4.9 sources ship inside the repo.

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The resulting `build/little8-lua.exe` is self-contained (raylib is linked statically by default). To link against `raylib.dll` instead:

```bash
cmake -S . -B build -DL8_RAYLIB_DLL=ON
```

On Linux/macOS, replace the raylib prebuilt in `raylib/` with a native build.

## Running

```bash
little8-lua.exe cart.lua
```

If no argument is given, `cart.lua` in the current directory is loaded. The engine opens a 128×128 window (default scale ×4), runs a fixed 60 FPS loop, and calls the cartridge's `running()` each frame. The window closes on `ESC` or the close button.

## Cartridge

Every cartridge is a single `.lua` file. Define a global function `running()` — it is called 60 times per second. State lives in globals or upvalues and persists between calls.

```lua
local t = 0

function running()
    cls(0)
    print(time(), 4, 4, 12)
    t = t + 1
end
```

> **Important:** `running` must be a **global** (`function running()`), not a local. The host calls `lua_getglobal(L, "running")` each frame — a `local function running()` is invisible to it and the screen will stay blank.

The cartridge runs in a reduced Lua environment: only `base`, `table`, `string`, `math`, and `coroutine` are available. `io`, `os`, `debug`, and `package` are not loaded, and `dofile`/`loadfile` are removed.

## API

### Screen

```lua
cls(color)                  -- clear to color 0..15
pset(x, y, color)           -- set pixel
pget(x, y)                  -- get pixel (0 outside)
```

### Drawing

```lua
line(x0, y0, x1, y1, color)
rect(x, y, w, h, color, filled)
oval(x, y, w, h, color, filled)   -- circle: w == h
```

`filled` — `true` for solid, `false` for 1 px outline.

### Text

```lua
print(s [, x, y, color])
```

Built-in 3×4 proportional font. Uppercase only. `\n` starts a new line.

Defaults: `x = 0`, `y = 0`, `color = 12` (white). So `print("hello")` prints in the top-left corner, visible on a black background.

> **Note:** `print` draws to the screen — it does **not** write to stdout. This is by design (PICO-8 style). If you're used to Lua's `print`, remember this is a drawing function now.

### Sprites

```lua
sprdata(id, data, w, h)                 -- load (id 0..255, w/h 1..128)
spr(id, x, y [, flip_x, flip_y, colorkey])   -- draw
```

`data` is a string of numbers `0..15` separated by commas, spaces, or newlines. `w*h` numbers must be present — if fewer, `sprdata` raises an error. The C API copies the data internally, so the string can be freed after the call.

Recommended syntax — Lua long strings `[[...]]`:

```lua
sprdata(1, [[
    0,2,2,0, 0,2,2,0,
    2,0,0,2, 2,0,0,2,
    2,0,0,0, 0,0,0,2,
    2,0,0,0, 0,0,0,2,
    0,2,0,0, 0,0,2,0,
    0,0,2,0, 0,2,0,0,
    0,0,0,2, 2,0,0,0,
    0,0,0,0, 0,0,0,0,
]], 8, 8)
```

Anything non-digit is ignored — commas, spaces, newlines, comments-in-string, whatever. So you can format the grid however you like. Only the numbers matter.

`spr` defaults: `flip_x = false`, `flip_y = false`, `colorkey = -1` (solid — all pixels drawn).

```lua
spr(1, 10, 10)                     -- solid, no flip
spr(1, 20, 10, false, false, 0)    -- color 0 transparent
spr(1, 30, 10, true,  false, 0)    -- flipped horizontally
spr(1, 40, 10, false, true,  0)    -- flipped vertically
spr(1, 50, 10, true,  true,  0)    -- flipped both
```

`colorkey` — which color is **not** drawn:

| value | behavior |
|---|---|
| `-1` | all pixels drawn (no transparency) |
| `0..15` | pixels of this color are skipped |

Calls to `sprdata` should happen **once**, not every frame. Either at cartridge top-level, or guarded by a `t == 0` check inside `running()` (if `sprdata` depends on state that only exists at runtime).
```

### Input

```lua
btn (id, player)    -- held
btnp(id, player)    -- pressed this frame (edge)
```

`player` is `0` (P1) or `1` (P2).

| id | P1 | P2 |
|---|---|---|
| 0 | A | ← |
| 1 | D | → |
| 2 | W | ↑ |
| 3 | S | ↓ |
| 4 | J | Num 4 |
| 5 | K | Num 5 |

### Audio

```lua
tone(frequency, duration, volume, channel)
```

| channel | wave |
|---|---|
| 0 | pulse 12.5% |
| 1 | pulse 25% |
| 2 | triangle |
| 3 | noise (frequency ignored) |

`frequency` in Hz, `duration` in frames (1/60 s), `volume` in 0–100.
`tone(0, 0, 0, ch)` stops the channel.

### Save / Load

```lua
dset(slot, value)    -- slot 0..63
dget(slot)           -- returns int64
```

Written to `save/storage.bin` next to the executable. Each `dset` writes to disk immediately (skipped if value unchanged). Out-of-range slot is silently ignored (`dset`) or returns `0` (`dget`).

> Writes happen **synchronously**. If you call `dset` every frame, you'll hit disk 60 times per second. Buffer state in Lua and save only on meaningful events (level end, pause, quit).

### Time

```lua
time()     -- milliseconds since start (monotonic)
utime()    -- Unix time in seconds
```

`time()` is the one you want for game logic — it's monotonic, unaffected by system clock changes, and starts at 0 when the cartridge launches. Use it for animation, cooldowns, timers:

```lua
local x = (time() % 2000) / 2000 * 120   -- 0..120, one cycle per 2 seconds
```

`utime()` is Unix epoch seconds — useful for timestamps in save data, useless for gameplay. It is **non-deterministic**: if you ever build replay or deterministic tests, cartridges that use `utime()` in game logic will break. Don't.

## Palette

| # | Hex | # | Hex |
|---|---|---|---|
| 0 | `1a1c2c` | 8 | `29366f` |
| 1 | `5d275d` | 9 | `3b5dc9` |
| 2 | `b13e53` | 10 | `41a6f6` |
| 3 | `ef7d57` | 11 | `73eff7` |
| 4 | `ffcd75` | 12 | `f4f4f4` |
| 5 | `a7f070` | 13 | `94b0c2` |
| 6 | `38b764` | 14 | `566c86` |
| 7 | `257179` | 15 | `333c57` |

## Complete example

```lua
-- cart.lua — a movable heart with beep on J and save on K

local px, py = 60, 60

sprdata(1, [[
    0,2,2,0, 0,2,2,0,
    2,0,0,2, 2,0,0,2,
    2,0,0,0, 0,0,0,2,
    2,0,0,0, 0,0,0,2,
    0,2,0,0, 0,0,2,0,
    0,0,2,0, 0,2,0,0,
    0,0,0,2, 2,0,0,0,
    0,0,0,0, 0,0,0,0,
]], 8, 8)

function running()
    cls(0)

    -- movement
    if btn(0, 0) then px = px - 1 end
    if btn(1, 0) then px = px + 1 end
    if btn(2, 0) then py = py - 1 end
    if btn(3, 0) then py = py + 1 end
    px = math.max(0, math.min(120, px))
    py = math.max(0, math.min(120, py))

    -- beep
    if btnp(4, 0) then
        tone(440, 4, 50, 0)
    end

    -- save
    if btnp(5, 0) then
        dset(0, px)
        dset(1, py)
        tone(660, 4, 50, 2)
    end

    -- draw
    spr(1, px, py, false, false, 0)
    print(string.format("X%d Y%d", px, py), 4, 4, 12)
    print(string.format("T %.1f", time() / 1000), 4, 12, 13)
end
```