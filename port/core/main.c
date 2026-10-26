#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_main.h>
#include "app.h"
#include "psx_data.h"
#include "disc_import.h"
#include "data_directory.h"

static void usage(const char *program)
{
    printf("Usage: %s --diagnostics | [--data DIR] [--window-test | --scene-preview | --intro-preview | --childhood-preview | --house-preview | --farm-preview | --area-preview ID] [--window-size WIDTHxHEIGHT] [--frames N] [--screenshot file.bmp]\n", program);
    puts("Default: title, registration and the first farm event. Enter=Start, arrows=select, Z=confirm.");
    puts("Data: --data DIR, user resources, executable/project. First launch can import a .bin.");
    puts("--save-file FILE: native save slot (default: user preferences/farm.hms). Sleep and save writes this slot; Continue/diary reads it.");
    puts("--load-save FILE: resume this native slot directly; cannot combine with preview/diagnostic modes.");
    puts("--setup: select a .bin and prepare user resources. First launch offers this automatically.");
    puts("--import-bin FILE [--data DIR]: import resources without Python; DIR must not exist.");
    puts("--scene-preview: farm geometry development view; arrows pan camera, Enter resets. No gameplay.");
    puts("--intro-preview: native arrival and childhood sequence; Z/X advance dialogue.");
    puts("--childhood-preview: development entry at the childhood meeting and song.");
    puts("--house-preview: house/farm; arrows walk, X runs, Z interacts, C uses tools, Enter opens farm status, V opens rucksack, E cycles tools, 2 cycles items. Native gameplay remains partial.");
    puts("--farm-preview: start outside the house, skipping Thomas's visit.");
    puts("--area-preview ID: start in region 0, 24..76 or 128..212 (steps of 4), 92, 96 or 112; fresh test session.");
    puts("--preview-weather TODAY:TOMORROW: development weather override (0 sun, 1 rain, 2 snow, 3 storm, 4 blizzard). Requires an area preview.");
    puts("--preview-date SEASON:DAY --preview-time HH:MM: set a region test session's date/time (season 0..3, day 1..30). Requires --area-preview or --farm-preview.");
}
static bool pair(const char *value,unsigned *first,unsigned *second)
{
    char *end,*last;errno=0;
    if(value[0]<'0' || value[0]>'9') return false;
    const unsigned long a=strtoul(value,&end,10);
    if(errno || *end!=':' || end[1]<'0' || end[1]>'9') return false;
    const unsigned long b=strtoul(end+1,&last,10);
    if(errno || *last || a>99 || b>99) return false;
    *first=(unsigned)a;*second=(unsigned)b;return true;
}

int main(int argc, char **argv)
{
    HmbtnAppOptions options = {0};
    hmbtn_farm_clock_init(&options.preview_clock);
    bool diagnostics = false;
    bool data_requested = false;
    const char *import_image=NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--setup") == 0) {
            options.setup=true;
        } else if (strcmp(argv[i], "--import-bin") == 0 && i+1<argc) {
            import_image=argv[++i];
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
        } else if (strcmp(argv[i], "--farm-preview") == 0) {
            options.area_preview=true;options.preview_area=0;
        } else if (strcmp(argv[i], "--area-preview") == 0 && i+1<argc) {
            const char *value=argv[++i];char *end;errno=0;
            const unsigned long area=strtoul(value,&end,10);
            if(errno || value[0]<'0' || value[0]>'9' || *end
                    || (area!=0 && area!=92 && area!=96 && area!=112
                        && !((area>=24 && area<=76 && area%4==0) || (area>=128 && area<=212 && area%4==0)))) {
                fputs("--area-preview requires 0, 24..76/128..212 in steps of 4, or 92/96/112.\n",stderr);return 2;
            }
            options.area_preview=true;options.preview_area=(unsigned)area;
        } else if (strcmp(argv[i], "--preview-date") == 0 && i+1<argc) {
            unsigned season,day;
            if(!pair(argv[++i],&season,&day) || season>3 || day<1 || day>30) {
                fputs("--preview-date requires SEASON:DAY, season 0..3 and day 1..30.\n",stderr);return 2;
            }
            options.preview_date_set=true;options.preview_clock.season=season;options.preview_clock.day=day-1;
            options.preview_clock.weekday=(season*30+day-1)%7;
        } else if (strcmp(argv[i], "--preview-time") == 0 && i+1<argc) {
            unsigned hour,minute;
            if(!pair(argv[++i],&hour,&minute) || hour>23 || minute>59) {
                fputs("--preview-time requires HH:MM, hour 0..23 and minute 0..59.\n",stderr);return 2;
            }
            options.preview_time_set=true;options.preview_clock.hour=hour;options.preview_clock.minute=minute;
        } else if(strcmp(argv[i],"--preview-weather")==0 && i+1<argc) {
            unsigned today,tomorrow;
            if(!pair(argv[++i],&today,&tomorrow) || today>=5 || tomorrow>=5) {
                fputs("--preview-weather requires TODAY:TOMORROW, 0 sun, 1 rain, 2 snow, 3 storm, 4 blizzard.\n",stderr);return 2;
            }
            options.preview_weather_set=true;options.preview_weather=today;options.preview_tomorrow=tomorrow;
        } else if(strcmp(argv[i],"--save-file")==0 && i+1<argc) {
            options.save_path=argv[++i];
            if(!*options.save_path) { fputs("Empty save path.\n",stderr);return 2; }
        } else if(strcmp(argv[i],"--load-save")==0 && i+1<argc) {
            options.load_save=true;options.save_path=argv[++i];
            if(!*options.save_path) { fputs("Empty save path.\n",stderr);return 2; }
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
    if((options.preview_date_set || options.preview_time_set || options.preview_weather_set) && !options.area_preview) {
        fputs("Preview date/time/weather requires --area-preview or --farm-preview.\n",stderr);return 2;
    }
    if (diagnostics && (options.frame_limit || options.screenshot_path || data_requested || options.window_test || options.scene_preview || options.intro_preview || options.childhood_preview || options.house_preview || options.area_preview || options.window_width)) {
        fputs("--diagnostics cannot be combined with window options.\n", stderr);
        return 2;
    }
    const unsigned modes = (unsigned)options.window_test+(unsigned)options.scene_preview
        +(unsigned)options.intro_preview+(unsigned)options.childhood_preview+(unsigned)options.house_preview+(unsigned)options.area_preview;
    if((options.load_save && modes) || (options.save_path && (diagnostics || options.window_test || options.setup || import_image))) {
        fputs("Native saves cannot be combined with setup/diagnostics; --load-save also excludes preview modes.\n",stderr);return 2;
    }
    if((import_image || options.setup) && (modes || diagnostics || options.frame_limit
            || options.screenshot_path || options.window_width || (import_image && options.setup))) {
        fputs("Data setup cannot be combined with game/diagnostic options.\n",stderr);return 2;
    }
    if(import_image) {
        char *out=options.data_root ? SDL_strdup(options.data_root) : hmbtn_user_data_directory();
        char error[512]={0};
        const bool ok=out && hmbtn_disc_import(import_image,out,NULL,NULL,error,sizeof(error));
        if(ok) printf("Native resources installed: %s\nNo PS1 executable, overlays or disc image retained.\n",out);
        else fprintf(stderr,"Import failed: %s\n",out ? error : SDL_GetError());
        SDL_free(out);return ok ? 0 : 1;
    }
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
    puts("Partial native port: title, opening, farm/village interiors, outdoor regions, clock, tools, inventory, forage and adult chickens. Native saves support stable gameplay checkpoints; remaining world systems are pending.");
    return diagnostics ? 0 : hmbtn_app_run(&options);
}
