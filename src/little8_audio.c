#include "little8_audio.h"
#include "little8.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#define TONE_SAMPLE_RATE 22050
#define TONE_CHANNELS    4

typedef struct {
    Sound sound;
    bool  used;
} ToneChannel;

static ToneChannel g_channels[TONE_CHANNELS];
static bool        g_audio_ready = false;
static bool        g_seeded      = false;

// ------------------------------------------------------------
//  Внутренние — вызываются из little8() в little8_api.c
// ------------------------------------------------------------
void l8_audio_init(void) {
    InitAudioDevice();
    g_audio_ready = IsAudioDeviceReady();
    for (int i = 0; i < TONE_CHANNELS; i++)
        g_channels[i].used = false;
}

void l8_audio_close(void) {
    for (int i = 0; i < TONE_CHANNELS; i++) {
        if (g_channels[i].used) {
            StopSound(g_channels[i].sound);
            UnloadSound(g_channels[i].sound);
            g_channels[i].used = false;
        }
    }
    if (g_audio_ready) CloseAudioDevice();
    g_audio_ready = false;
}

// ------------------------------------------------------------
//  Генераторы
// ------------------------------------------------------------
static void gen_pulse(short *s, int n, float freq, int vol, float duty) {
    for (int i = 0; i < n; i++) {
        float t     = (float)i / TONE_SAMPLE_RATE;
        float phase = fmodf(t * freq, 1.0f);
        float v     = (phase < duty) ? 1.0f : -1.0f;
        s[i]        = (short)(v * vol * 327.67f);
    }
}

static void gen_triangle(short *s, int n, float freq, int vol) {
    for (int i = 0; i < n; i++) {
        float t     = (float)i / TONE_SAMPLE_RATE;
        float phase = fmodf(t * freq, 1.0f);
        float v     = 2.0f * fabsf(2.0f * phase - 1.0f) - 1.0f;
        s[i]        = (short)(v * vol * 327.67f);
    }
}

static void gen_noise(short *s, int n, int vol) {
    for (int i = 0; i < n; i++) {
        float v = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s[i]    = (short)(v * vol * 327.67f);
    }
}

// ------------------------------------------------------------
//  Публичный API — tone()
// ------------------------------------------------------------
L8_API void tone(int frequency, int duration, int volume, int channel) {
    if (channel < 0 || channel >= TONE_CHANNELS) return;
    if (!g_audio_ready) return;

    // freq=0 или volume=0 — остановить канал
    if (frequency <= 0 || volume <= 0) {
        if (g_channels[channel].used) {
            StopSound(g_channels[channel].sound);
            UnloadSound(g_channels[channel].sound);
            g_channels[channel].used = false;
        }
        return;
    }

    if (duration <= 0) return;

    if (!g_seeded) {
        srand((unsigned)time(NULL));
        g_seeded = true;
    }

    float duration_sec = duration / 60.0f;
    int   sample_count = (int)(TONE_SAMPLE_RATE * duration_sec);
    if (sample_count <= 0) return;

    short *samples = (short *)malloc(sample_count * sizeof(short));
    if (!samples) return;

    switch (channel) {
        case 0: gen_pulse(samples, sample_count, (float)frequency, volume, 0.125f); break;
        case 1: gen_pulse(samples, sample_count, (float)frequency, volume, 0.250f); break;
        case 2: gen_triangle(samples, sample_count, (float)frequency, volume);      break;
        case 3: gen_noise(samples, sample_count, volume);                           break;
    }

    Wave wave = {
        .frameCount = (unsigned int)sample_count,
        .sampleRate = TONE_SAMPLE_RATE,
        .sampleSize = 16,
        .channels   = 1,
        .data       = samples
    };

    Sound s = LoadSoundFromWave(wave);
    free(samples);
    if (s.frameCount == 0) return;

    if (g_channels[channel].used) {
        StopSound(g_channels[channel].sound);
        UnloadSound(g_channels[channel].sound);
    }

    g_channels[channel].sound = s;
    g_channels[channel].used  = true;
    PlaySound(s);
}