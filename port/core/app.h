#ifndef HMBTN_APP_H
#define HMBTN_APP_H

#include <stdint.h>
#include <stdbool.h>

typedef struct HmbtnAppOptions {
    uint32_t frame_limit; /* Zero runs until the user closes the window. */
    unsigned window_width, window_height; /* Zero uses the default; never game coordinates. */
    const char *screenshot_path; /* Finite run: last frame. Otherwise: first frame. */
    const char *data_root;
    bool window_test; /* M1 diagnostic; does not require game data. */
    bool scene_preview; /* Development view of farm geometry; no gameplay. */
    bool intro_preview; /* Original opening, including the childhood sequence. */
    bool childhood_preview; /* Development entry at the childhood meeting. */
    bool house_preview; /* Initial house, with the implemented walking subset. */
} HmbtnAppOptions;

int hmbtn_app_run(const HmbtnAppOptions *options);

#endif
