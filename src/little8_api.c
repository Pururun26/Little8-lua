#include "little8.h"
#include "little8_audio.h"
#include "little8_font3x4.h"
#include "little8_save.h"
#include "raylib.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define L8_SCREEN_W 128
#define L8_SCREEN_H 128

#define L8_NAME         "Little-8"
#define L8_WINDOW_SCALE 4
#define L8_TARGET_FPS   60

#define L8_SPR_COUNT  256
#define L8_SPR_MAX_W  128
#define L8_SPR_MAX_H  128

typedef struct {
    uint8_t *data;   // w*h байт, row-major; NULL = пустой спрайт
    int w, h;
} L8Sprite;

static L8Sprite g_sprites[L8_SPR_COUNT];

// Палитра 16 цветов. Игра знает только индексы.
static const uint32_t palette[16] = {
    0x1a1c2c, 0x5d275d, 0xb13e53, 0xef7d57,
    0xffcd75, 0xa7f070, 0x38b764, 0x257179,
    0x29366f, 0x3b5dc9, 0x41a6f6, 0x73eff7,
    0xf4f4f4, 0x94b0c2, 0x566c86, 0x333c57
};

static uint8_t  fb[L8_SCREEN_H][L8_SCREEN_W];
static uint32_t pixels[L8_SCREEN_H * L8_SCREEN_W];


// ---------- Внутреннее ----------
static void flush_fb(void) {
    for (int y = 0; y < L8_SCREEN_H; y++) {
        for (int x = 0; x < L8_SCREEN_W; x++) {
            uint32_t c = palette[fb[y][x]];
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >>  8) & 0xFF;
            uint8_t b =  c        & 0xFF;
            pixels[y * L8_SCREEN_W + x] =
                (uint32_t)r | ((uint32_t)g << 8) |
                ((uint32_t)b << 16) | 0xFF000000u;
        }
    }
}


void l8_time_reset(void);


// ---------- Точка входа ----------
void little8(void (*callback)(void)) {
    InitWindow(L8_SCREEN_W * L8_WINDOW_SCALE,
               L8_SCREEN_H * L8_WINDOW_SCALE, L8_NAME);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(L8_SCREEN_W, L8_SCREEN_H);
    SetTargetFPS(L8_TARGET_FPS);

    l8_time_reset();
    l8_audio_init();
    l8_save_open();

    Image img = GenImageColor(L8_SCREEN_W, L8_SCREEN_H, BLACK);
    Texture2D screen = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(screen, TEXTURE_FILTER_POINT);

    while (!WindowShouldClose()) {
        if (callback) callback();
        flush_fb();
        UpdateTexture(screen, pixels);

        BeginDrawing();
            ClearBackground(BLACK);

            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            int scale_x = sw / L8_SCREEN_W;
            int scale_y = sh / L8_SCREEN_H;
            int scale   = (scale_x < scale_y) ? scale_x : scale_y;
            if (scale < 1) scale = 1;

            int dw = L8_SCREEN_W * scale;
            int dh = L8_SCREEN_H * scale;
            int ox = (sw - dw) / 2;
            int oy = (sh - dh) / 2;

            DrawTexturePro(
                screen,
                (Rectangle){ 0, 0, L8_SCREEN_W, L8_SCREEN_H },
                (Rectangle){ (float)ox, (float)oy, (float)dw, (float)dh },
                (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadTexture(screen);
    CloseWindow();
    l8_save_close();
    l8_audio_close();
}


void cls(uint8_t color) {
    for (int y = 0; y < L8_SCREEN_H; y++)
        for (int x = 0; x < L8_SCREEN_W; x++)
            fb[y][x] = color & 0x0F;
}

void pset(int x, int y, uint8_t color) {
    if (x < 0 || x >= L8_SCREEN_W || y < 0 || y >= L8_SCREEN_H) return;
    fb[y][x] = color & 0x0F;
}

uint8_t pget(int x, int y) {
    if (x < 0 || x >= L8_SCREEN_W || y < 0 || y >= L8_SCREEN_H) return 0;
    return fb[y][x];
}

void print(const char *s, int x, int y, uint8_t color) {
    if (!s) return;

    int cx = x, cy = y;

    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\n') { cx = x; cy += FONT_CELL_H + 1; continue; }
        if (*p == '\r') continue;

        unsigned char u = *p;
        if (u >= 'a' && u <= 'z') u -= 32;

        int idx = (int)u - FONT_FIRST;
        if (idx < 0 || idx >= FONT_COUNT) idx = '?' - FONT_FIRST;

        int w = l8_font_w[idx];
        for (int row = 0; row < FONT_CELL_H; row++) {
            unsigned char bits = l8_font[idx][row];
            for (int col = 0; col < FONT_CELL_W; col++) {
                if ((bits >> (FONT_CELL_W - 1 - col)) & 1)
                    pset(cx + col, cy + row, color);
            }
        }
        cx += w + 1;
    }
}


void line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        pset(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void rect(int x, int y, int w, int h, uint8_t color, bool filled) {
    if (w <= 0 || h <= 0) return;

    if (filled) {
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                pset(x + i, y + j, color);
    } else {
        for (int i = 0; i < w; i++) {
            pset(x + i, y,         color);
            pset(x + i, y + h - 1, color);
        }
        for (int j = 0; j < h; j++) {
            pset(x,         y + j, color);
            pset(x + w - 1, y + j, color);
        }
    }
}

void oval(int x, int y, int w, int h, uint8_t color, bool filled) {
    if (w <= 0 || h <= 0) return;

    if (w == 1 || h == 1) {
        // вырожденный случай — просто линия
        rect(x, y, w, h, color, true);
        return;
    }

    if (filled) {
        float rx = (w - 1) / 2.0f;
        float ry = (h - 1) / 2.0f;
        float cx = x + rx;

        for (int j = 0; j < h; j++) {
            float dy = (j - ry) / ry;
            float dy2 = dy * dy;
            if (dy2 > 1.0f) continue;
            float dx = sqrtf(1.0f - dy2) * rx;

            int x0 = (int)(cx - dx + 0.5f);
            int x1 = (int)(cx + dx + 0.5f);

            if (x0 < x)     x0 = x;
            if (x1 > x+w-1) x1 = x + w - 1;

            for (int i = x0; i <= x1; i++)
                pset(i, y + j, color);
        }
    } else {
        // midpoint ellipse (Zingl)
        int x0 = x, y0 = y, x1 = x + w - 1, y1 = y + h - 1;
        long a  = abs(x1 - x0);
        long b  = abs(y1 - y0);
        long b1 = b & 1;
        long dx = 4 * (1 - a) * b * b;
        long dy = 4 * (b1 + 1) * a * a;
        long err = dx + dy + b1 * a * a;

        if (x0 > x1) { x0 = x1; x1 += a; }
        if (y0 > y1) y0 = y1;
        y0 += (b + 1) / 2;
        y1 = y0 - b1;
        a *= 8 * a;
        b1 = 8 * b * b;

        do {
            pset(x1, y0, color); pset(x0, y0, color);
            pset(x0, y1, color); pset(x1, y1, color);
            long e2 = 2 * err;
            if (e2 <= dy) { y0++; y1--; err += dy += a; }
            if (e2 >= dx || 2 * err > dy) { x0++; x1--; err += dx += b1; }
        } while (x0 <= x1);

        while (y0 - y1 < b) {
            pset(x0 - 1, y0, color); pset(x1 + 1, y0++, color);
            pset(x0 - 1, y1, color); pset(x1 + 1, y1--, color);
        }
    }
}


bool btn(int id, int player) {
    switch (player) {
        case 0:  // P1: WASD + J/K
            switch (id) {
                case 0: return IsKeyDown(KEY_A);
                case 1: return IsKeyDown(KEY_D);
                case 2: return IsKeyDown(KEY_W);
                case 3: return IsKeyDown(KEY_S);
                case 4: return IsKeyDown(KEY_J);
                case 5: return IsKeyDown(KEY_K);
            }
            break;
        case 1:  // P2: стрелки + Numpad 4/5
            switch (id) {
                case 0: return IsKeyDown(KEY_LEFT);
                case 1: return IsKeyDown(KEY_RIGHT);
                case 2: return IsKeyDown(KEY_UP);
                case 3: return IsKeyDown(KEY_DOWN);
                case 4: return IsKeyDown(KEY_KP_4);
                case 5: return IsKeyDown(KEY_KP_5);
            }
            break;
    }
    return false;
}

bool btnp(int id, int player) {
    switch (player) {
        case 0:
            switch (id) {
                case 0: return IsKeyPressed(KEY_A);
                case 1: return IsKeyPressed(KEY_D);
                case 2: return IsKeyPressed(KEY_W);
                case 3: return IsKeyPressed(KEY_S);
                case 4: return IsKeyPressed(KEY_J);
                case 5: return IsKeyPressed(KEY_K);
            }
            break;
        case 1:
            switch (id) {
                case 0: return IsKeyPressed(KEY_LEFT);
                case 1: return IsKeyPressed(KEY_RIGHT);
                case 2: return IsKeyPressed(KEY_UP);
                case 3: return IsKeyPressed(KEY_DOWN);
                case 4: return IsKeyPressed(KEY_KP_4);
                case 5: return IsKeyPressed(KEY_KP_5);
            }
            break;
    }
    return false;
}

void sprdata(int id, const uint8_t *data, int w, int h) {
    if (id < 0 || id >= L8_SPR_COUNT) return;
    if (!data || w <= 0 || h <= 0 || w > L8_SPR_MAX_W || h > L8_SPR_MAX_H) return;

    L8Sprite *s = &g_sprites[id];

    // освобождаем старые данные, если спрайт уже был загружен
    if (s->data) {
        free(s->data);
        s->data = NULL;
    }

    s->data = (uint8_t *)malloc((size_t)w * (size_t)h);
    if (!s->data) { s->w = s->h = 0; return; }

    memcpy(s->data, data, (size_t)w * (size_t)h);
    s->w = w;
    s->h = h;
}

void spr(int id, int x, int y, bool flip_x, bool flip_y, int colorkey) {
    if (id < 0 || id >= L8_SPR_COUNT) return;

    const L8Sprite *s = &g_sprites[id];
    if (!s->data || s->w <= 0 || s->h <= 0) return;

    for (int row = 0; row < s->h; row++) {
        for (int col = 0; col < s->w; col++) {
            int sx = flip_x ? (s->w - 1 - col) : col;
            int sy = flip_y ? (s->h - 1 - row) : row;
            uint8_t c = s->data[sy * s->w + sx];

            if (colorkey >= 0 && c == (uint8_t)colorkey) continue;

            pset(x + col, y + row, c);
        }
    }
}


// ---- monotonic clock ----
// На Windows mingw-w64 предоставляет clock_gettime + CLOCK_MONOTONIC
// через libmingwex (линкуется по умолчанию). На Linux/macOS — из libc.
// Одна реализация на всё, без #ifdef _WIN32.

static int64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

static int64_t g_time_start_ns = 0;

void l8_time_reset(void) {
    g_time_start_ns = now_ns();
}

int64_t l8_time(void) {
    return (now_ns() - g_time_start_ns) / 1000000LL;
}

int64_t l8_utime(void) {
    return (int64_t)time(NULL);
}