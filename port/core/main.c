#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_main.h>
#include "app.h"
#include "psx_data.h"

static void usage(const char *program)
{
    printf("Usage: %s --diagnostics | [--data DIR] [--window-test | --scene-preview | --intro-preview | --childhood-preview | --house-preview] [--window-size WIDTHxHEIGHT] [--frames N] [--screenshot file.bmp]\n", program);
    puts("Default: title, registration and the first farm event. Enter=Start, arrows=select, Z=confirm.");
    puts("Data is located beside the executable or in the project; --data DIR overrides it.");
    puts("--scene-preview: farm geometry development view; arrows pan camera, Enter resets. No gameplay.");
    puts("--intro-preview: native arrival and childhood sequence; Z/X advance dialogue.");
    puts("--childhood-preview: development entry at the childhood meeting and song.");
    puts("--house-preview: initial house; arrows walk, Circle/X runs. Exit and interactions pending.");
}

int main(int argc, char **argv)
{
    HmbtnAppOptions options = {0};
    bool diagnostics = false;
    bool data_requested = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--diagnostics") == 0) {
            diagnostics = true;
        } else if (strcmp(argv[i], "--window-test") == 0) {
            options.window_test = true;
        } else if (strcmp(argv[i], "--scene-preview") == 0) {
            options.scene_preview = true;
        } else if (strcmp(argv[i], "--intro-preview") == 0) {
            options.intro_preview = true;
        } else if (strcmp(argv[i], "--childhood-preview") == 0) {
            options.childhood_preview = true;
        } else if (strcmp(argv[i], "--house-preview") == 0) {
            options.house_preview = true;
        } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            options.data_root = argv[++i];
            data_requested = true;
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            const char *size = argv[++i];
            char *end, *height_end;
            errno = 0;
            const unsigned long width = strtoul(size, &end, 10);
            if (errno || size[0] < '0' || size[0] > '9' || *end != 'x'
                    || width < 320 || width > 7680 || end[1] < '0' || end[1] > '9') {
                fputs("--window-size requires WIDTHxHEIGHT, from 320x240 to 7680x4320.\n", stderr);
                return 2;
            }
            const unsigned long height = strtoul(end + 1, &height_end, 10);
            if (errno || *height_end || height < 240 || height > 4320) {
                fputs("Invalid --window-size height.\n", stderr);
                return 2;
            }
            options.window_width = (unsigned)width;
            options.window_height = (unsigned)height;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            const char *text = argv[++i];
            char *end;
            errno = 0;
            const unsigned long value = strtoul(text, &end, 10);
            if (errno || text[0] < '0' || text[0] > '9' || *end || value == 0 || value > 1000000UL) {
                fputs("--frames requires an integer between 1 and 1000000.\n", stderr);
                return 2;
            }
            options.frame_limit = (uint32_t)value;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options.screenshot_path = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (diagnostics && (options.frame_limit || options.screenshot_path || data_requested || options.window_test || options.scene_preview || options.intro_preview || options.childhood_preview || options.house_preview || options.window_width)) {
        fputs("--diagnostics cannot be combined with window options.\n", stderr);
        return 2;
    }
    const unsigned modes = (unsigned)options.window_test+(unsigned)options.scene_preview
        +(unsigned)options.intro_preview+(unsigned)options.childhood_preview+(unsigned)options.house_preview;
    if (modes > 1 || (options.window_test && data_requested)) {
        fputs("Window diagnostic modes are exclusive; --window-test cannot load --data.\n", stderr);
        return 2;
    }
    const uint8_t sample[] = {0x00, 0x00, 0x01, 0x80};
    if (hmbtn_read_le32(sample) != UINT32_C(0x80010000)) {
        fputs("PS1 data format check failed.\n", stderr);
        return 1;
    }
    puts("Harvest Moon: Back to Nature - PC port");
    puts("Native foundation ready (Linux / Windows target).");
    printf("Host pointer width: %u bits\n", (unsigned)(sizeof(void *) * CHAR_BIT));
    puts("Partial native port: title, registration, opening and initial house movement. Farm exit, interactions, full gameplay and saves remain pending.");
    return diagnostics ? 0 : hmbtn_app_run(&options);
}
