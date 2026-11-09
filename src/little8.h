/*
 * Little-8 — a small fantasy console framework for C
 *
 * Copyright (c) 2026 Pururun26
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software
 *      in a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not
 *      be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define L8_API

#ifdef __cplusplus
extern "C" {
#endif

// ---- Точка входа ----
// little8() запускает консоль. callback — функция, которую движок
// вызывает каждый кадр (60 раз в секунду). Имя в твоей игре — любое.
// Если callback == NULL — окно откроется с чёрным экраном.
//
// Пример:
//     static void my_game(void) { ... }
//     int main(void) { little8(my_game); return 0; }
L8_API void little8(void (*callback)(void));

// Очистить экран цветом (0..15)
L8_API void cls(uint8_t color);

// Поставить пиксель (0..15)
L8_API void pset(int x, int y, uint8_t color);

// Прочитать цвет пикселя (0..15). За границами — 0.
L8_API uint8_t pget(int x, int y);

// Печать строки. '\n' — перенос, lowercase → uppercase, неизвестное → '?'.
L8_API void print(const char *s, int x, int y, uint8_t color);


// Линия от (x0,y0) до (x1,y1) — алгоритм Брезенхэма.
L8_API void line(int x0, int y0, int x1, int y1, uint8_t color);

// Прямоугольник. filled=true — залитый, false — рамка 1px.
L8_API void rect(int x, int y, int w, int h, uint8_t color, bool filled);

// Овал/круг (круг = w==h). filled=true — залитый, false — контур.
L8_API void oval(int x, int y, int w, int h, uint8_t color, bool filled);


// Кнопка зажата сейчас (0=left 1=right 2=up 3=down 4=A 5=B)
L8_API bool btn(int id, int player);

// Кнопка нажата в этом кадре (edge, «только что»)
L8_API bool btnp(int id, int player);


// ---- Спрайты ----
// Загрузить спрайт id (0..255) размером w×h (пикселей).
// data — row-major, w*h байт, значения 0..15, 0 = прозрачный.
// Данные КОПИРУЮТСЯ внутрь DLL — массив можно освободить после вызова.
L8_API void sprdata(int id, const uint8_t *data, int w, int h);

// Нарисовать спрайт id в (x,y). flip_x/flip_y — отражение.
// colorkey: цвет (0..15), который НЕ рисуется; -1 = все пиксели видны.
L8_API void spr(int id, int x, int y, bool flip_x, bool flip_y, int colorkey);


// ---- Звук ----
L8_API void tone(int frequency, int duration, int volume, int channel);


// ---- Сохранения ----
// 64 слота по int64_t. Данные пишутся в <exe_dir>/save/storage.bin
// при каждом dset (файл 512 байт — запись мгновенная).
// Если slot вне [0, 63] — dset игнорируется, dget возвращает 0.
L8_API void    dset(int slot, int64_t value);
L8_API int64_t dget(int slot);


// ---- Time ----
// Миллисекунды с момента старта little8(). Монотонные, не зависят от
// системных часов, не прыгают при переводе времени.
L8_API int64_t l8_time(void);

// Unix-время в секундах. Для меток в сейвах, не для игровой логики.
// Недетерминированно — картриджи с реплеями/тестами не должны на него
// опираться.
L8_API int64_t l8_utime(void);

#ifdef __cplusplus
}
#endif