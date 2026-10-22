#ifndef HMBTN_GAME_INPUT_H
#define HMBTN_GAME_INPUT_H

#include <SDL3/SDL.h>
#include "pad_buttons.h"

typedef struct HmbtnGameInput {
    SDL_Gamepad *gamepad;
    uint32_t pending_pressed;
    uint32_t previous_gamepad;
    uint32_t keyboard_held;
    bool focused;
} HmbtnGameInput;

void hmbtn_game_input_init(HmbtnGameInput *input);
void hmbtn_game_input_close(HmbtnGameInput *input);
void hmbtn_game_input_event(HmbtnGameInput *input, const SDL_Event *event, SDL_WindowID window);
void hmbtn_game_input_poll(HmbtnGameInput *input);
uint32_t hmbtn_game_input_take_pressed(HmbtnGameInput *input);

uint32_t hmbtn_game_input_held(const HmbtnGameInput *input);

#endif
