#include "title_preview.h"
#include "asset_decode.h"
#include "resource_file.h"
#include "sdl_host.h"
#include "title_audio.h"
#include "new_game_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TITLE_OFFSET = 0x2000, TITLE_SIZE = 0x23000, TITLE_ANI_SIZE = 0xD1C,
    PIXEL_BYTES = HMBTN_SCREEN_WIDTH * HMBTN_SCREEN_HEIGHT * 4 };
struct HmbtnTitlePreview {
    uint8_t *block, *pixels;
    HmbtnImageAtlas *atlas;
    HmbtnAnimation animation;
    HmbtnAnimationPlayer actors[3], columns[3];
    uint16_t actor_timers[3], column_timers[3], scroll_a, scroll_b;
    uint32_t random_seed, scene_frames;
    SDL_Texture *texture;
    Uint64 accumulated_ns;
    uint32_t pending_pressed;
    HmbtnTitleFlow flow;
    HmbtnTitleAudio audio;
    HmbtnNewGame new_game;
    HmbtnNewGameUI *new_game_ui;
    char *data_root;
    bool frame_ready;
};

void hmbtn_title_preview_discard_input(HmbtnTitlePreview *p) { p->pending_pressed = 0; }
void hmbtn_title_preview_destroy(HmbtnTitlePreview *p)
{
    if (!p) return;
    hmbtn_title_audio_close(&p->audio);
    hmbtn_new_game_ui_destroy(p->new_game_ui);
    SDL_free(p->data_root);
    SDL_DestroyTexture(p->texture);
    free(p->atlas); free(p->block); free(p->pixels); free(p);
}
HmbtnTitlePreview *hmbtn_title_preview_create(SDL_Renderer *renderer, const char *data_root,
    char *error, size_t error_size)
{
    HmbtnTitlePreview *p = calloc(1, sizeof(*p));
    if (!p) { snprintf(error, error_size, "Cannot allocate title preview"); return NULL; }
    p->block = malloc(TITLE_SIZE);
    p->data_root = SDL_strdup(data_root);
    p->atlas = malloc(sizeof(*p->atlas));
    p->pixels = malloc(PIXEL_BYTES);
    if (!p->block || !p->atlas || !p->pixels || !p->data_root) {
        snprintf(error, error_size, "Cannot allocate title resource buffers"); goto failed;
    }
    if (!hmbtn_resource_read(data_root, "A_FILE/00_gf.bin", TITLE_OFFSET,
            p->block, TITLE_SIZE, error, error_size)
            || !hmbtn_animation_open(&p->animation, p->block, TITLE_ANI_SIZE, error, error_size)
            || !hmbtn_bank_decode(p->block + TITLE_ANI_SIZE, TITLE_SIZE - TITLE_ANI_SIZE,
                p->atlas, error, error_size)) goto failed;
    if (p->animation.count[3] <= 37) {
        snprintf(error, error_size, "Title animation layout is incompatible"); goto failed;
    }
    p->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        HMBTN_SCREEN_WIDTH, HMBTN_SCREEN_HEIGHT);
    if (!p->texture || !SDL_SetTextureScaleMode(p->texture, SDL_SCALEMODE_NEAREST)
            || !SDL_SetTextureBlendMode(p->texture, SDL_BLENDMODE_NONE)) {
        snprintf(error, error_size, "Cannot create title texture: %s", SDL_GetError()); goto failed;
    }
    if (!hmbtn_title_audio_open(&p->audio, data_root, error, error_size)) goto failed;
    p->actors[0].animation = 0; p->actors[1].animation = 10; p->actors[2].animation = 19;
    for (unsigned i = 0; i < 3; ++i) p->columns[i].animation = (uint16_t)(22 + i * 2);
    hmbtn_title_flow_init(&p->flow);
    return p;
failed:
    hmbtn_title_preview_destroy(p);
    return NULL;
}

static unsigned random_range(HmbtnTitlePreview *p, unsigned range)
{
    p->random_seed = p->random_seed * UINT32_C(0x41C64E6D) + 12345;
    return ((p->random_seed >> 16) * range) >> 16;
}
static void select_animation(HmbtnAnimationPlayer *player, unsigned animation)
{
    *player = (HmbtnAnimationPlayer){(uint16_t)animation, 0, 0};
}
static bool sprite(HmbtnTitlePreview *p, const HmbtnAnimationPlayer *player, int x, int y,
    char *error, size_t error_size)
{
    return hmbtn_animation_draw(&p->animation, p->atlas, player, x, y, p->pixels,
        HMBTN_SCREEN_WIDTH, HMBTN_SCREEN_HEIGHT, PIXEL_BYTES, error, error_size);
}
static bool still(HmbtnTitlePreview *p, unsigned sequence, int x, int y, char *error, size_t error_size)
{
    const HmbtnAnimationPlayer player = {(uint16_t)sequence, 0, 0};
    return sprite(p, &player, x, y, error, error_size);
}

static bool background(HmbtnTitlePreview *p, bool advance, char *error, size_t error_size)
{
    if (!still(p, 29, 0, 0, error, error_size)) return false;
    const int phase = ((int)p->scroll_b << 12) / 120;
    const double angle = (phase & 4095) * (6.2831853071795864769 / 4096.0);
    /* Same 12-bit angle and integer coordinate arithmetic as title_draw_bg_scroll.
     * Host trig rounding still requires pixel comparison against the SDK table. */
    const int trig[2] = {(int)SDL_round(SDL_cos(angle) * 4096), (int)SDL_round(SDL_sin(angle) * 4096)};
    /* Same-OT entries are prepended on PS1: B is displayed before A. */
    for (int layer = 1; layer >= 0; --layer) {
        const int scroll = layer ? p->scroll_b : p->scroll_a, band = scroll / 240;
        const int top = layer ? scroll - band * 240 : band * 240 - scroll;
        const int left = 320 - band * 80 + trig[layer] * 12 / 4096;
        for (int row = 2; row >= 0; --row)
            for (int col = 2; col >= 0; --col)
                if (!still(p, 37, layer ? top + left - col * 320 : top - left + col * 320,
                        layer ? top - row * 240 : top + row * 240, error, error_size)) return false;
    }
    if (advance) {
        if (p->scene_frames & 1) p->scroll_a = (uint16_t)((p->scroll_a + 1) % 960);
        p->scroll_b = (uint16_t)((p->scroll_b + 1) % 960);
    }
    return true;
}

static bool characters(HmbtnTitlePreview *p, bool advance, char *error, size_t error_size)
{
    static const int actor_x[3] = {282,33,51}, actor_y[3] = {173,146,182};
    static const int column_x[3][5] = {{295,258,280,0,0},{63,10,9,295,294},{23,6,36,0,0}};
    static const int column_y[3][5] = {{205,138,121,0,0},{149,188,214,52,30},{30,109,223,0,0}};
    static const unsigned instances[3] = {3,5,3}, waits[3] = {15,20,30};
    HmbtnAnimationPlayer actors[3], columns[3][5];
    memcpy(actors, p->actors, sizeof(actors));
    for (unsigned i = 0; i < 3; ++i) {
        if (!advance) continue;
        if (!hmbtn_animation_tick(&p->animation, &p->actors[i], error, error_size)) return false;
        if (++p->actor_timers[i] >= (i == 0 ? 90 : 60)) {
            p->actor_timers[i] = 0;
            unsigned mode;
            if (i < 2) {
                mode = random_range(p, i ? 9 : 10) + (i ? 10 : 0);
                if (!(mode & 1) && mode) --mode;
                if (i && mode < 10) mode = 10;
            } else {
                mode = random_range(p, 3) + 19;
                if (mode == 19) p->actor_timers[i] = 30;
            }
            select_animation(&p->actors[i], mode);
        }
    }
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < instances[i]; ++j) {
            columns[i][j] = p->columns[i];
            if (advance && !hmbtn_animation_tick(&p->animation, &p->columns[i], error, error_size)) return false;
        }
        if (advance && ++p->column_timers[i] >= waits[i]) {
            p->column_timers[i] = 0;
            select_animation(&p->columns[i], random_range(p, 2) + 22 + i * 2);
        }
    }
    for (int i = 2; i >= 0; --i)
        for (int j = (int)instances[i] - 1; j >= 0; --j)
            if (!sprite(p, &columns[i][j], column_x[i][j], column_y[i][j], error, error_size)) return false;
    for (int i = 2; i >= 0; --i)
        if (!sprite(p, &actors[i], actor_x[i], actor_y[i], error, error_size)) return false;
    return true;
}

static bool compose(HmbtnTitlePreview *p, bool advance, char *error, size_t error_size)
{
    const bool publisher = p->flow.step < 0;
    memset(p->pixels, publisher ? 255 : 0, PIXEL_BYTES);
    for (size_t i = 3; i < PIXEL_BYTES; i += 4) p->pixels[i] = 255;
    if (publisher) {
        if (!still(p, 30, 0, 0, error, error_size)) return false;
    } else {
        if (!background(p, advance, error, error_size) || !characters(p, advance, error, error_size)) return false;
        const bool new_game = p->flow.step >= 11 && p->flow.step <= 28;
        if (!new_game && (p->flow.visible & 1) && !still(p, 31, 0, p->flow.draw_slide, error, error_size)) return false;
        if (!new_game && (p->flow.visible & 2) && !still(p, 32, 0, p->flow.draw_drop, error, error_size)) return false;
        const bool menu = p->flow.step >= HMBTN_TITLE_MENU && !new_game;
        if (menu) {
            if (!still(p, 34, 0, 0, error, error_size) || !still(p, 35, 0, 0, error, error_size)
                    || !still(p, 36, p->flow.logo_frames % 30 < 15 ? 100 : 104,
                        p->flow.menu_row ? 179 : 155, error, error_size)) return false;
        } else if (!new_game && (p->flow.visible & 4) && !still(p, 33, p->flow.draw_wipe, 0, error, error_size)) return false;
    }
    if (p->flow.step >= 11 && p->flow.step <= 28 && p->new_game_ui
            && !hmbtn_new_game_ui_draw(p->new_game_ui,&p->new_game,p->scene_frames,p->pixels,error,error_size)) return false;
    if (!SDL_UpdateTexture(p->texture, NULL, p->pixels, HMBTN_SCREEN_WIDTH * 4)) {
        snprintf(error, error_size, "Cannot upload title frame: %s", SDL_GetError()); return false;
    }
    p->frame_ready = true;
    return true;
}

bool hmbtn_title_preview_draw(HmbtnTitlePreview *p, SDL_Renderer *renderer,
    Uint64 elapsed_ns, uint32_t pressed, uint32_t held, char *error, size_t error_size)
{
    /* US NTSC title uses VSync(2): about 29.97 logic frames/s. Cache the composed
     * frame, so presentation frequency cannot advance sprites or scroll twice. */
    const Uint64 tick_ns = SDL_NS_PER_SECOND * 1001 / 30000;
    p->accumulated_ns += SDL_min(elapsed_ns, SDL_NS_PER_SECOND / 4);
    p->pending_pressed |= pressed;
    if (!p->frame_ready && !compose(p, false, error, error_size)) return false;
    while (p->accumulated_ns >= tick_ns) {
        const HmbtnTitleStep previous = p->flow.step;
        const uint16_t previous_row = p->flow.menu_row;
        random_range(p, 65535); /* main.c advances the generator every game tick. */
        hmbtn_title_flow_tick(&p->flow, p->pending_pressed);
        if (previous == HMBTN_TITLE_NEW_GAME && p->flow.step == HMBTN_TITLE_NEW_GAME_TEXT_OPEN) {
            hmbtn_new_game_init(&p->new_game);
            if (!p->new_game_ui) p->new_game_ui = hmbtn_new_game_ui_create(p->data_root,error,error_size);
            if (!p->new_game_ui) return false;
        } else if (p->flow.step >= 11 && p->flow.step <= 28) {
            hmbtn_new_game_tick(&p->new_game,p->pending_pressed,held);
            if (!hmbtn_new_game_ui_load(p->new_game_ui,p->data_root,&p->new_game,error,error_size)) return false;
            if (p->new_game.sound >= 0) hmbtn_sound_effect(p->audio.sound,(unsigned)p->new_game.sound);
            p->flow.step = (HmbtnTitleStep)p->new_game.step;
            if (p->new_game.step==27 || p->new_game.step==28)
                p->flow.brightness=(uint16_t)(10-p->new_game.fade);
            if (p->new_game.finished) {
                p->flow.destination = HMBTN_TITLE_DESTINATION_NEW_GAME;
                p->flow.step = HMBTN_TITLE_UNAVAILABLE;
                puts("Registration accepted: player name, birthday, farm name and dog name. Farm scene/gameplay are not ported yet.");
            }
        }
        p->pending_pressed = 0;
        hmbtn_title_audio_tick(&p->audio, &p->flow, previous);
        if ((previous == HMBTN_TITLE_INTRO_FADE_OUT && p->flow.step == HMBTN_TITLE_SCENE_FADE_IN)
                || (previous == HMBTN_TITLE_START_WAIT && p->flow.step == HMBTN_TITLE_MENU)) {
            memset(p->actor_timers, 0, sizeof(p->actor_timers));
            memset(p->column_timers, 0, sizeof(p->column_timers));
        }
        if (p->flow.step != previous || p->flow.menu_row != previous_row) {
            printf("Title state: %d, row: %u, destination: %d\n", (int)p->flow.step,
                (unsigned)p->flow.menu_row, (int)p->flow.destination);
            if (p->flow.step == HMBTN_TITLE_UNAVAILABLE)
                puts("Destination not ported (farm scene / saves / demo); Start, Cross or Circle returns to title menu.");
            fflush(stdout);
        }
        if (!compose(p, true, error, error_size)) return false;
        ++p->scene_frames;
        p->accumulated_ns -= tick_ns;
    }
    const unsigned denominator = p->flow.step < 0 ? HMBTN_TITLE_PUBLISHER_FADE_TICKS : HMBTN_TITLE_FADE_TICKS;
    const Uint8 brightness = p->flow.step == HMBTN_TITLE_UNAVAILABLE ? 255
        : (Uint8)(p->flow.brightness * 255 / denominator);
    if (!hmbtn_title_audio_pump(&p->audio, error, error_size)) return false;
    if (!SDL_SetTextureColorMod(p->texture, brightness, brightness, brightness)
            || !SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) || !SDL_RenderClear(renderer)
            || !SDL_RenderTexture(renderer, p->texture, NULL, NULL)) {
        snprintf(error, error_size, "Cannot draw title preview: %s", SDL_GetError()); return false;
    }
    return true;
}
