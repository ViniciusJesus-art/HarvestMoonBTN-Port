#include "game_input.h"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    HmbtnGameInput input = {0};
    input.focused = true;
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = 1;
    event.key.scancode = SDL_SCANCODE_RETURN;
    hmbtn_game_input_event(&input, &event, 1);
    event.type = SDL_EVENT_KEY_UP;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(hmbtn_game_input_held(&input) == 0);
    CHECK(hmbtn_game_input_take_pressed(&input) == HMBTN_PAD_START); /* Short tap is retained. */
    CHECK(hmbtn_game_input_take_pressed(&input) == 0);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.repeat = true;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(hmbtn_game_input_take_pressed(&input) == 0);
    event.key.repeat = false;
    event.key.scancode = SDL_SCANCODE_Z;
    hmbtn_game_input_event(&input, &event, 2);
    CHECK(hmbtn_game_input_take_pressed(&input) == 0);
    hmbtn_game_input_event(&input, &event, 1);
    event.key.scancode = SDL_SCANCODE_DOWN;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(hmbtn_game_input_take_pressed(&input) == (HMBTN_PAD_CROSS | HMBTN_PAD_DOWN));
    CHECK(hmbtn_game_input_held(&input) == (HMBTN_PAD_CROSS | HMBTN_PAD_DOWN));
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.windowID = 1;
    hmbtn_game_input_event(&input, &event, 1);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = 1;
    event.key.scancode = SDL_SCANCODE_Z;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(hmbtn_game_input_take_pressed(&input) == 0);
    CHECK(hmbtn_game_input_held(&input) == 0);
    event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    event.window.windowID = 1;
    hmbtn_game_input_event(&input, &event, 1);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = 1;
    event.key.scancode = SDL_SCANCODE_BACKSPACE;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(hmbtn_game_input_take_pressed(&input) == HMBTN_PAD_CIRCLE);
    const SDL_Scancode shoulder_keys[] = {SDL_SCANCODE_E, SDL_SCANCODE_2, SDL_SCANCODE_Q, SDL_SCANCODE_1};
    const uint32_t shoulder_buttons[] = {HMBTN_PAD_R1, HMBTN_PAD_R2, HMBTN_PAD_L1, HMBTN_PAD_L2};
    event.type = SDL_EVENT_KEY_UP;
    hmbtn_game_input_event(&input, &event, 1);
    for (unsigned i = 0; i < 4; ++i) {
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.scancode = shoulder_keys[i];
        hmbtn_game_input_event(&input, &event, 1);
        CHECK(hmbtn_game_input_take_pressed(&input) == shoulder_buttons[i]);
        CHECK(hmbtn_game_input_held(&input) == shoulder_buttons[i]);
        event.type = SDL_EVENT_KEY_UP;
        hmbtn_game_input_event(&input, &event, 1);
        CHECK(hmbtn_game_input_held(&input) == 0);
    }
    return 0;
}
