#include "game_input.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); return 1; } } while (0)

int main(void)
{
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.name = "HMBTN synthetic controller";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id && SDL_IsGamepad(id));
    HmbtnGameInput input = {0};
    input.focused = true;
    SDL_Event event = {0};
    event.type = SDL_EVENT_GAMEPAD_ADDED;
    event.gdevice.which = id;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(input.gamepad && SDL_GetGamepadID(input.gamepad) == id);
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(input.gamepad);
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, true));
    SDL_UpdateGamepads();
    hmbtn_game_input_poll(&input);
    CHECK(hmbtn_game_input_take_pressed(&input) == HMBTN_PAD_START);
    hmbtn_game_input_poll(&input);
    CHECK(hmbtn_game_input_take_pressed(&input) == 0);
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, false));
    CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, -20000));
    SDL_UpdateGamepads();
    hmbtn_game_input_poll(&input);
    CHECK(hmbtn_game_input_take_pressed(&input) == HMBTN_PAD_UP);
    CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 0));
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    SDL_UpdateGamepads();
    hmbtn_game_input_poll(&input);
    CHECK(hmbtn_game_input_take_pressed(&input) == HMBTN_PAD_CROSS);
    CHECK(SDL_DetachVirtualJoystick(id));
    event.type = SDL_EVENT_GAMEPAD_REMOVED;
    event.gdevice.which = id;
    hmbtn_game_input_event(&input, &event, 1);
    CHECK(!input.gamepad || SDL_GetGamepadID(input.gamepad) != id);
    hmbtn_game_input_close(&input);
    SDL_Quit();
    return 0;
}
