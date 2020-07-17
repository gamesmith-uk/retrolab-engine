#include "../global.h"
#include "video.h"

#include <stdio.h>
#include <SDL2/SDL.h>

#include "font.h"
#include "keyboard.h"
#include "joystick.h"
#include "memory.h"
#include "mmap.h"

#define SCREEN_W 120
#define SCREEN_H 135
#define BORDER   5

#define LINES     30
#define COLUMNS   40
#define CHAR_W     6
#define CHAR_H     9

static SDL_Window*   window  = NULL;
static SDL_Renderer* ren     = NULL;
static bool          running = true;
static double        zoom    = 2.0;
static SDL_Texture*  font    = NULL;

typedef struct CursorInfo {
    struct Cursor {
        uint8_t color    : 4;
        bool    visible  : 1;
        bool    whole    : 1;
        bool    blinking : 1;
        bool    unused   : 1;
    }*       cursor;
    uint16_t pos;
} CursorInfo;

// {{{ initialization

static void
load_font() {
    SDL_RWops* io = SDL_RWFromConstMem(font_bmp, font_bmp_len);
    SDL_Surface* sf = SDL_LoadBMP_RW(io, 1);
    if (!sf) {
        fprintf(stderr, "SDL_LoadBMP_RW: %s\n", SDL_GetError());
    }
    SDL_SetColorKey(sf, SDL_RLEACCEL, 0);
    font = SDL_CreateTextureFromSurface(ren, sf);
    // SDL_SetTextureBlendMode(font, SDL_BLENDMODE_MOD);
    SDL_FreeSurface(sf);
};

static void
video_init_data()
{
#define SET_COLOR(n, color) \
    ram[VIDEO_PALETTE + (n * 3)] = (color >> 16) & 0xff; \
    ram[VIDEO_PALETTE + (n * 3) + 1] = (color >> 8) & 0xff; \
    ram[VIDEO_PALETTE + (n * 3) + 2] = color & 0xff;

    // palette - https://lospec.com/palette-list/sweetie-16
    SET_COLOR(COLOR_BLACK,      0x1a1c2c);
    SET_COLOR(COLOR_PURPLE,     0x5d275d);
    SET_COLOR(COLOR_RED,        0xb13e53);
    SET_COLOR(COLOR_ORANGE,     0xef7d57);
    SET_COLOR(COLOR_YELLOW,     0xffcd75);
    SET_COLOR(COLOR_LIME,       0xa7f070);
    SET_COLOR(COLOR_GREEN,      0x38b764);
    SET_COLOR(COLOR_TURQUOISE,  0x257179);
    SET_COLOR(COLOR_DARK_BLUE,  0x29366f);
    SET_COLOR(COLOR_BLUE,       0x3b5dc9);
    SET_COLOR(COLOR_LIGHT_BLUE, 0x41a6f6);
    SET_COLOR(COLOR_CYAN,       0x73eff7);
    SET_COLOR(COLOR_WHITE,      0xf4f4f4);
    SET_COLOR(COLOR_LIGHT_GRAY, 0x94b0c2);
    SET_COLOR(COLOR_GRAY,       0x566c86);
    SET_COLOR(COLOR_DARK_GRAY,  0x333c57);

    // text color
    memset(&ram[VIDEO_TXT_COLOR], (COLOR_LIME << 4) | COLOR_BLACK, LINES);
    ram[VIDEO_CURSOR_INFO] = (1 << 4) | COLOR_ORANGE;  // visible, whole, non-blinking, orange
}

void
video_init()
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        "retrolab",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        (SCREEN_W + (BORDER * 2)) * 2 * zoom,
        (SCREEN_H + (BORDER * 2)) * 2 * zoom,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    load_font();
    video_init_data();
    SDL_StartTextInput();
}

void
video_destroy()
{
    SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
    SDL_EventState(SDL_KEYDOWN, SDL_DISABLE);
    SDL_EventState(SDL_KEYUP, SDL_DISABLE);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool
video_running()
{
    return running;
}

// }}}

// {{{ events

static void
do_events()
{
    joystic_update_state();

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYUP:
                joystick_interrupt(&e);
                break;
            case SDL_KEYDOWN:
                joystick_interrupt(&e);
                keyboard_interrupt(&e);
                break;
            case SDL_TEXTINPUT:
                keyboard_interrupt(&e);
                break;
        }
    }
}

// }}}

// {{{ draw

static SDL_Color
palette_color(uint8_t idx) {
    return (SDL_Color) {
        .r = ram[VIDEO_PALETTE + (idx * 3) + 0],
        .g = ram[VIDEO_PALETTE + (idx * 3) + 1],
        .b = ram[VIDEO_PALETTE + (idx * 3) + 2],
        .a = 0xff,
    };
}

static void
draw_border()
{
    uint8_t border_color_idx = ram[VIDEO_BORDER] & 0xf;
    SDL_Color border = palette_color(border_color_idx);

    SDL_RenderSetScale(ren, zoom * 2, zoom * 2);

    SDL_SetRenderDrawColor(ren, border.r, border.g, border.b, border.a);
    SDL_RenderClear(ren);
}

static void
draw_background()
{
    SDL_RenderSetScale(ren, zoom, zoom);
    for (size_t i = 0; i < LINES; ++i) {
        SDL_Color bg = palette_color(ram[VIDEO_TXT_COLOR + i] & 0xf);
        SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, bg.a);
        SDL_Rect r = { 
            .x = BORDER * 2,
            .y = (BORDER * 2) + (i * CHAR_H),
            .w = SCREEN_W * 2,
            .h = CHAR_H
        };
        SDL_RenderFillRect(ren, &r);
    }
}

static void
draw_text(CursorInfo* cursor)
{
    SDL_RenderSetScale(ren, zoom, zoom);

    for (size_t i = 0; i < (COLUMNS * LINES); ++i) {
        char c = ram[VIDEO_TXT + i];
        int line = i / COLUMNS;
        int orig_x = (c / 16) * CHAR_W;
        int orig_y = (c % 16) * CHAR_H;
        int dest_x = (i % COLUMNS) * CHAR_W + (BORDER * 2);
        int dest_y = (i / COLUMNS) * CHAR_H + (BORDER * 2);

        // if cursor is here
        if (cursor->cursor->visible && cursor->pos == i) {
            SDL_Color cg = palette_color(cursor->cursor->color);
            SDL_SetRenderDrawColor(ren, cg.r, cg.g, cg.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(ren, &(SDL_Rect) { dest_x, dest_y, CHAR_W, CHAR_H });
            SDL_Color bg = palette_color(ram[VIDEO_TXT_COLOR + i] & 0xf);
            SDL_SetTextureColorMod(font, bg.r, bg.g, bg.b);
        } else {
            SDL_Color fg = palette_color(ram[VIDEO_TXT_COLOR + line] >> 4);
            SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b);
        }
        if (c != 0 && c != 32) {
            SDL_RenderCopy(ren, font,
                    &(SDL_Rect) { orig_x, orig_y, CHAR_W, CHAR_H },
                    &(SDL_Rect) { dest_x, dest_y, CHAR_W, CHAR_H });
        }
    }
}

CursorInfo
cursor_info()
{
    CursorInfo info;
    info.cursor = (struct Cursor*) &ram[VIDEO_CURSOR_INFO];
    info.pos = ram_get16(VIDEO_CURSOR_POS);
    return info;
}

static void
draw_frame()
{
    CursorInfo cursor = cursor_info();
    draw_border();
    draw_background();
    draw_text(&cursor);
}

// }}}

void
video_tick()
{
    do_events();
    draw_frame();
    SDL_RenderPresent(ren);
}

void
video_set(uint16_t addr, uint8_t data)
{
    (void) addr;
    (void) data;
}

// {{{ debugging

int
video_dbg_json(char* buf, size_t bufsz)
{
    int n = 0;
#define PRINT(...) { n += snprintf(&buf[n], bufsz - n, __VA_ARGS__); }
    PRINT("\"video\":{");
    PRINT("\"palette\":[");
    for(size_t i = 0; i < 16; ++i)
        PRINT("\"#%02X%02X%02X\"%s",
                ram[VIDEO_PALETTE + (i * 3)], ram[VIDEO_PALETTE + (i * 3) + 1], ram[VIDEO_PALETTE + (i * 3) + 2],
                (i != 15) ? "," : "");
    PRINT("],");
    PRINT("\"borderColor\":%d,", ram[VIDEO_BORDER] & 0xf);
    PRINT("\"lineColor\":[");
    for(size_t i = 0; i < 30; ++i)
        PRINT("{\"bg\":%d,\"fg\":%d}%s",
                ram[VIDEO_TXT_COLOR+i] >> 4,
                ram[VIDEO_TXT_COLOR+i] & 0xf,
                (i != 29) ? "," : "");
    PRINT("],");
    PRINT("\"cursor\":{\"color\":%d,\"visible\":%s,\"position\":%d}",
            ram[VIDEO_CURSOR_INFO] & 0xf,
            ((ram[VIDEO_CURSOR_INFO] >> 4) & 1) ? "true" : "false",
            ram[VIDEO_CURSOR_POS]);
    PRINT("}");
#undef PRINT
    return n;
}

// }}}

// vim:st=4:sts=4:sw=4:expandtab:foldmethod=marker
