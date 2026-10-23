#include "game_input.h"
#include <string.h>

static uint32_t key_button(SDL_Scancode code)
{
    switch (code) {
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return HMBTN_PAD_START;
    case SDL_SCANCODE_UP: return HMBTN_PAD_UP;
    case SDL_SCANCODE_DOWN: return HMBTN_PAD_DOWN;
    case SDL_SCANCODE_LEFT: return HMBTN_PAD_LEFT;
    case SDL_SCANCODE_RIGHT: return HMBTN_PAD_RIGHT;
    case SDL_SCANCODE_TAB: return HMBTN_PAD_SELECT;
    case SDL_SCANCODE_Z: return HMBTN_PAD_CROSS;
    case SDL_SCANCODE_X: case SDL_SCANCODE_BACKSPACE: return HMBTN_PAD_CIRCLE;
    case SDL_SCANCODE_C: return HMBTN_PAD_SQUARE;
    case SDL_SCANCODE_V: return HMBTN_PAD_TRIANGLE;
    case SDL_SCANCODE_Q: return HMBTN_PAD_L1;
    case SDL_SCANCODE_E: return HMBTN_PAD_R1;
    case SDL_SCANCODE_1: return HMBTN_PAD_L2;
    case SDL_SCANCODE_2: return HMBTN_PAD_R2;
    default: return 0;
    }
}

void hmbtn_game_input_init(HmbtnGameInput *input)
{
    memset(input, 0, sizeof(*input));
    input->focused = true;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (count > 0) input->gamepad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}

void hmbtn_game_input_close(HmbtnGameInput *input)
{
    SDL_CloseGamepad(input->gamepad);
    memset(input, 0, sizeof(*input));
}

void hmbtn_game_input_event(HmbtnGameInput *input, const SDL_Event *event, SDL_WindowID window)
{
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.windowID == window && !event->key.repeat && input->focused) {
        input->pending_pressed |= key_button(event->key.scancode);
        input->keyboard_held |= key_button(event->key.scancode);
    } else if (event->type == SDL_EVENT_KEY_UP && event->key.windowID == window) {
        input->keyboard_held &= ~key_button(event->key.scancode);
    } else if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST && event->window.windowID == window) {
        input->pending_pressed = 0;
        input->keyboard_held = 0;
        input->focused = false;
    } else if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED && event->window.windowID == window) {
        input->focused = true;
    } else if (event->type == SDL_EVENT_GAMEPAD_ADDED && !input->gamepad) {
        input->gamepad = SDL_OpenGamepad(event->gdevice.which);
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED && input->gamepad
            && event->gdevice.which == SDL_GetGamepadID(input->gamepad)) {
        const bool focused = input->focused;
        hmbtn_game_input_close(input);
        hmbtn_game_input_init(input);
        input->focused = focused;
    }
}

void hmbtn_game_input_poll(HmbtnGameInput *input)
{
    uint32_t held = 0;
    if (input->gamepad) {
        static const struct { SDL_GamepadButton button; uint32_t mask; } buttons[] = {
            {SDL_GAMEPAD_BUTTON_START, HMBTN_PAD_START}, {SDL_GAMEPAD_BUTTON_BACK, HMBTN_PAD_SELECT},
            {SDL_GAMEPAD_BUTTON_SOUTH, HMBTN_PAD_CROSS}, {SDL_GAMEPAD_BUTTON_EAST, HMBTN_PAD_CIRCLE},
            {SDL_GAMEPAD_BUTTON_WEST, HMBTN_PAD_SQUARE}, {SDL_GAMEPAD_BUTTON_NORTH, HMBTN_PAD_TRIANGLE},
            {SDL_GAMEPAD_BUTTON_DPAD_UP, HMBTN_PAD_UP}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, HMBTN_PAD_DOWN},
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, HMBTN_PAD_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, HMBTN_PAD_RIGHT},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, HMBTN_PAD_L1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, HMBTN_PAD_R1}
        };
        for (size_t i = 0; i < SDL_arraysize(buttons); ++i) {
            if (SDL_GetGamepadButton(input->gamepad, buttons[i].button)) held |= buttons[i].mask;
        }
        const Sint16 x = SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 y = SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        if (x < -16000) held |= HMBTN_PAD_LEFT;
        if (x > 16000) held |= HMBTN_PAD_RIGHT;
        if (y < -16000) held |= HMBTN_PAD_UP;
        if (y > 16000) held |= HMBTN_PAD_DOWN;
        if (SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) held |= HMBTN_PAD_L2;
        if (SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) held |= HMBTN_PAD_R2;
    }
    if (input->focused) input->pending_pressed |= held & ~input->previous_gamepad;
    input->previous_gamepad = held;
}

uint32_t hmbtn_game_input_take_pressed(HmbtnGameInput *input)
{
    const uint32_t pressed = input->pending_pressed;
    input->pending_pressed = 0;
    return pressed;
}

uint32_t hmbtn_game_input_held(const HmbtnGameInput *input)
{
    return input->focused ? input->keyboard_held | input->previous_gamepad : 0;
}
