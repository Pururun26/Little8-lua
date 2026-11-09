#include "little8_save.h"
#include "little8.h"      // для прототипов dset/dget с L8_API
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define SAVE_SLOTS 64
#define SAVE_BYTES ((int)(SAVE_SLOTS * sizeof(int64_t)))

static int64_t g_save[SAVE_SLOTS];
static int     g_ready = 0;
static char    g_path[1024];

void l8_save_open(void) {
    memset(g_save, 0, sizeof(g_save));
    g_ready = 0;

    // <exe_dir>/save/
    const char *appdir = GetApplicationDirectory();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%ssave", appdir);
    if (!DirectoryExists(dir)) MakeDirectory(dir);

    // <exe_dir>/save/storage.bin
    snprintf(g_path, sizeof(g_path), "%ssave/storage.bin", appdir);

    if (FileExists(g_path)) {
        int size = 0;
        unsigned char *data = LoadFileData(g_path, &size);
        if (data) {
            if (size == SAVE_BYTES) {
                memcpy(g_save, data, SAVE_BYTES);
            } else {
                TraceLog(LOG_WARNING,
                    "Little-8: save size %d != %d, ignoring", size, SAVE_BYTES);
            }
            UnloadFileData(data);
        }
    }

    g_ready = 1;
}

void l8_save_close(void) {
    g_ready = 0;
}

// ---- публичный API ----
L8_API void dset(int slot, int64_t value) {
    if (!g_ready || slot < 0 || slot >= SAVE_SLOTS) return;
    if (g_save[slot] == value) return;   // не пишем, если не изменилось

    g_save[slot] = value;
    SaveFileData(g_path, g_save, SAVE_BYTES);
}

L8_API int64_t dget(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    return g_save[slot];
}