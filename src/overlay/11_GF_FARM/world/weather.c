/* Weather for the farm: the weather tomorrow rolled per season, the falling particles and the sky tint. */
#include "types.h"
#include "anim.h"
#include "animals.h"
#include "clock.h"
#include "estate.h"
#include "event_flags.h"
#include "farm_data.h"
#include "farm_state.h"
#include "field.h"
#include "main.h"
#include "scene.h"
#include "screen_fade.h"
#include "ui_icons.h"
#include "villager.h"
#include "weather.h"

typedef struct {
    s16 season;
    s16 day;
} FarmSeasonDay;

typedef struct {
    s16 x;
    s16 y;
} FarmPoint;

#define PARTICLE_DELAY_STAGGER 3

static void date_next_day(FarmSeasonDay *date);
static s32 villager_birthday_is_today(s16 *date);
static void weather_show_icon(void);
static void particle_clear(FarmParticle *particle);
static void drift_step(void);
static void drift_set_target(s32 steps, s32 *target);
static void particles_step(void);
static void particle_respawn(FarmParticle *particle);

/* Call field_gate_open on every cell of the 45 by 25 field grid. */
void field_reset_cells(void) {
    u32 outer = 0;
    u32 inner;

    do {
        inner = 0;
        do {
            field_gate_open(0, inner, outer, 1);
            inner++;
        } while (inner < 45);
        outer++;
    } while (outer < 25);
}

/* Roll tomorrow's weather. Nothing falls on a clock_date_index day, on the
 * wedding day or on a bachelorette's birthday. Otherwise each season has its own
 * random draw. If the day after tomorrow is a clock_date_index day, a storm or a
 * blizzard drops by 2. */
s16 weather_roll_tomorrow(void) {
    FarmState *farm = &game_state;
    FarmSeasonDay next;
    FarmSeasonDay after;
    s32 roll;

    next.season = farm->progress.clock.date.season;
    next.day = farm->progress.clock.date.day;
    date_next_day(&next);
    farm->world.weatherTomorrow = WEATHER_SUNNY;
    after.season = next.season;
    after.day = next.day;
    date_next_day(&after);

    if (clock_date_index(next.season, next.day) == 0 &&
        (next.day != farm->player.wedding.date.day || next.season != farm->player.wedding.date.season) &&
        villager_birthday_is_today(&next.season) != 1) {
        switch (next.season) {
        case SEASON_SPRING:
            roll = rand_range(10) + 1;
            if (roll % 3 == 0) {
                farm->world.weatherTomorrow = WEATHER_RAIN;
            }
            break;
        case SEASON_SUMMER:
            roll = rand_range(20);
            if (roll == 14) {
                farm->world.weatherTomorrow = WEATHER_STORM;
            } else if (roll == 3) {
                farm->world.weatherTomorrow = WEATHER_RAIN;
            }
            break;
        case SEASON_FALL:
            if (next.day != 12 && rand_range(5) == 3) {
                farm->world.weatherTomorrow = WEATHER_RAIN;
            }
            break;
        case SEASON_WINTER:
            roll = rand_range(10);
            if (roll == 7) {
                farm->world.weatherTomorrow = WEATHER_BLIZZARD;
            } else if (roll & 1) {
                farm->world.weatherTomorrow = WEATHER_SNOW;
            }
            break;
        }

        if (clock_date_index(after.season, after.day) != 0 && game_state.world.weatherTomorrow >= WEATHER_STORM) {
            game_state.world.weatherTomorrow -= 2;
        }
    }
    return game_state.world.weatherTomorrow;
}

/* Step a (season, day) to the next day: 30 days a season, the four seasons in turn, the year
 * wrapping back to spring. */
static void date_next_day(FarmSeasonDay *date) {
    if (date->day + 1 < DAYS_PER_SEASON) {
        date->day++;
        return;
    }
    date->day = 0;
    if (date->season + 1 < SEASONS_PER_YEAR) {
        date->season++;
        return;
    }
    date->season = 0;
}

/* Advance to the new day. Today's weather is the one rolled yesterday; it is cleared on a dated
 * day (clock_date_index), on the wedding day and on a bachelorette's birthday. A storm or a
 * blizzard drops by 2 when tomorrow is a dated day. */
s16 weather_step_today(void) {
    FarmState *farm = &game_state;
    FarmSeasonDay next;
    FarmPoint current;

    next.season = farm->progress.clock.date.season;
    current.x = next.season;
    next.day = farm->progress.clock.date.day;
    current.y = next.day;
    date_next_day(&next);

    farm->world.weather = game_state.world.weatherTomorrow;
    if (clock_date_index(farm->progress.clock.date.season, farm->progress.clock.date.day) != 0) {
        farm->world.weather = 0;
    }
    if (clock_date_index(next.season, next.day) != 0 && farm->world.weather >= WEATHER_STORM) {
        farm->world.weather -= 2;
    }

    if (event_flag_get(EVENT_FLAG_WEDDING_TODAY) == 1) {
        game_state.world.weather = 0;
    }
    if (villager_birthday_is_today(&current.x) == 1) {
        game_state.world.weather = 0;
    }

    weather_show_icon();
    return game_state.world.weather;
}

/* The bachelorettes' ids (Karen, Ann, Elli, Mary, Popuri): a birthday of one of them is a day
 * without weather. The sixth entry is never read; villager_birthday_is_today stops at five. */
static s16 birthday_villager_ids[6] = {
    CHARACTER_KAREN, CHARACTER_ANN, CHARACTER_ELLI, CHARACTER_MARY, CHARACTER_POPURI, 25134,
};

/* True when one of the bachelorettes has a birthday on `date` (season, day). */
static s32 villager_birthday_is_today(s16 *date) {
    s32 i = 0;

    do {
        FarmVillager *r = &game_state.villagers[birthday_villager_ids[i]];
        if (r->birthday.season == date[0] && r->birthday.day == date[1]) {
            return 1;
        }
        i++;
    } while (i < 5);
    return 0;
}

/* Show the weather icon: clear the five weather flags, then set the one for the current weather. */
static void weather_show_icon(void) {
    u32 i = 0;

    do {
        event_flag_set((i + EVENT_FLAG_WEATHER_SUNNY) & 0xFFFF, 0);
        i++;
    } while (i < 5);
    event_flag_set((game_state.world.weather + EVENT_FLAG_WEATHER_SUNNY) & 0xFFFF, 1);
}

static void weather_set(s16 weather) {
    game_state.world.weather = weather;
}

static FarmParticle weather_particles[40];
static AnimState weather_anim;
/* The drift table in use, rows of three words: weather_fx_init points it at
 * drift_by_hour, the weather at drift_by_weather; drift_select_hour hands row `index` to
 * drift_set_target, drift_reset reads row 0. */
static s32 (*weather_drift_table)[3];

/* Refresh the sky tint and the particles for one frame. The particles step unless the zone's flags
 * say 0x1000; nothing runs in the greenhouse or when the zone's flags say 0x800. When an hour has
 * passed the tint starts to move to the new hour's colours over 256 frames, and the tint steps
 * while no menu is open. */
void weather_fx_frame(void)
{
  FarmZoneHeader *record;
  u16 flags;
  s32 index;

  record = &farm_area.mode.data->head;
  flags = record->flags;
  if (flags & 0x800)
    return;
  if (record->place.location == PLACE_GREENHOUSE)
    return;
  if ((flags & 0x1000) == 0)
    particles_step();
  if (game_state.globalFlags & GLOBAL_FLAG_HOUR_PASSED)
  {
    if (game_state.drift.index != (index = clock_time_period(game_state.progress.clock.time.hm.hour)))
    {
      drift_set_target(256, weather_drift_table[index]);
      game_state.drift.index = index;
    }
  }
  if ((game_state.globalFlags & (GLOBAL_FLAG_MENU_OPEN | GLOBAL_FLAG_BIT21)) == 0)
  {
    if ((farm_area.mode.data->head.flags & 0x1000) == 0)
      drift_step();
  }
}

/* Point the tint at the hour's colours at once, in one step. */
void drift_select_hour(void) {
    s32 index;
    FarmState *farm = &game_state;

    index = clock_time_period(farm->progress.clock.time.hm.hour);
    drift_set_target(1, weather_drift_table[index]);
    game_state.drift.index = index;
}

/* Clear the particles and point the tint at the hour table. */
void weather_fx_init(void) {
    FarmParticle *p = weather_particles;
    s32 i;
    for (i = 39; i >= 0; i--) {
        particle_clear(p);
        p++;
    }
    weather_drift_table = drift_by_hour;
}

void weather_frame_update_hook(void) {
}

/* Set the tint to row 0 of the drift table in use and clear its steps. */
void drift_reset(void) {
    FarmState *q = &game_state;
    s32 *p = weather_drift_table[0];
    q->drift.red = p[0];
    q->drift.blue = p[2];
    q->drift.green = p[1];
    q->drift.stepRed = 0;
    q->drift.stepBlue = 0;
    q->drift.stepGreen = 0;
    q->drift.stepsToTake = 0;
    q->drift.stepsTaken = 0;
    q->drift.index = 0;
}

/* Empty a particle slot: no sprite, flags 0x8000. */
static void particle_clear(FarmParticle *particle) {
    u16 val = 0x8000;
    particle->x = 0;
    particle->y = 0;
    particle->sprite = 0;
    particle->frame = 0;
    particle->age = 0;
    particle->delay = 0;
    particle->flags = val;
}

/* Fill the screen with the tint and move the tint one step toward its target. */
static void drift_step(void) {
    FarmDrift *drift = &game_state.drift;
    s32 sum;

    sum = drift->red + drift->green + drift->blue;
    if (sum != 0) {
        screen_fill_quad(drift->red, drift->green, drift->blue);
    }
    if (drift->stepsToTake > drift->stepsTaken) {
        drift->stepsTaken = drift->stepsTaken + 1;
        drift->red = drift->red + drift->stepRed;
        drift->green = drift->green + drift->stepGreen;
        drift->blue = drift->blue + drift->stepBlue;
    }
}

/* Aim the tint at `target` over `steps` frames: each channel moves by its distance divided by steps. */
static void drift_set_target(s32 steps, s32 *target) {
    FarmDrift *drift = &game_state.drift;

    drift->stepsTaken = 0;
    drift->stepsToTake = steps;
    drift->stepRed = (target[0] - drift->red) / steps;
    drift->stepGreen = (target[1] - drift->green) / steps;
    drift->stepBlue = (target[2] - drift->blue) / steps;
}

/* Step one drifting particle. A positive delay only counts down. A moving
 * particle (flag 0x4000) falls by 1, or 5 when fast (0x1000), doubled for
 * 0x2000. It drifts left by 7 when fast and wobbles one pixel by age, then ages
 * until 0x37/0x6E and is respawned by particle_respawn. A still particle only
 * animates (frame = age / 2, respawned at frame 3). While the game is paused
 * nothing ages. The particle then scrolls with farm_area.mode.scroll and is drawn if
 * it is on screen. */
static void particle_step(FarmParticle *entry) {
    s32 dy;
    s32 dx;
    s32 x;
    s32 step;
    s32 age;
    s32 frame;
    s16 y;
    FarmScroll *scroll;

    if (entry->delay >= 0) {
        if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
            entry->delay = entry->delay - 1;
        }
        return;
    }
    if (entry->flags & 0x4000) {
        if (entry->flags & 0x1000) {
            dy = 5;
            dx = 7;
        } else {
            dy = 1;
            dx = 0;
        }
        y = entry->y;
        if (entry->flags & 0x2000) {
            entry->y = y + dy * 2;
        } else {
            entry->y = y + dy;
        }
        x = (u16)entry->x - dx;
        step = (entry->age & 7) != 7;
        entry->x = x;
        if (entry->age & 0x8) {
            entry->x = x + step;
        } else {
            entry->x = x - step;
        }
        if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) {
            goto draw;
        }
        age = entry->age;
        if (entry->flags & 0x2000) {
            if (age >= 55) {
                goto reset;
            }
        } else if (age >= 110) {
            goto reset;
        }
        goto advance;
    } else {
        frame = entry->age >> 1;
        entry->frame = frame;
        if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED)) {
            if (frame >= 3) {
            reset:
                particle_respawn(entry);
                return;
            }
        advance:
            entry->age++;
        } else {
            entry->frame = (s16)frame % 3;
        }
    }
draw:
    scroll = &farm_area.mode.scroll;
    entry->x += scroll->dx;
    entry->y += scroll->dy;
    if (estate_point_on_screen(entry->x, entry->y) != 0) {
        anim_draw_sprt_cursor(current_frame->ot, &ui_icon_anim, entry->sprite, entry->frame,
                      entry->x, entry->y);
    }
}

/* Step every particle slot that has a sprite. The first slot with no delay is left alone; each later
 * moving slot with no delay gets PARTICLE_DELAY_STAGGER added to its delay. */
static void particles_step(void) {
    FarmParticle *entry;
    u32 index;
    s32 seen_idle;

    seen_idle = 0;
    entry = weather_particles;
    index = 0;
    do {
        if (entry->sprite != 0) {
            particle_step(entry);
        }
        if (entry->delay == 0) {
            if (seen_idle == 0) {
                seen_idle = 1;
            } else if ((entry->flags & 0x4000) != 0) {
                entry->delay = entry->delay + PARTICLE_DELAY_STAGGER;
            }
        }
        index++;
        entry++;
    } while (index < 40);
}

/* Respawn a particle at a random place and delay, by its kind of motion. */
static void particle_respawn(FarmParticle *particle) {
    u16 flags;
    s32 base;
    s32 add0;
    s32 add2;
    s32 add4;
    s32 add6;
    s32 add8;

    flags = particle->flags;
    if (flags & 0x4000) {
        base = 0x4A;
        if (!(flags & 0x1000)) {
            base = 0x69A;
            add0 = 240;
            add4 = 60;
            add8 = 20;
            add2 = 5;
            add6 = 15;
        } else {
            add0 = 300;
            add4 = 180;
            add8 = 12;
            add2 = 10;
            add6 = 30;
        }
    } else {
        base = 0x19;
        add0 = 240;
        add4 = 160;
        add8 = 15;
        add2 = 20;
        add6 = 20;
    }
    base &= 0xFFFF;
    particle->delay = rand_range(base) + add8;
    rand_range(base);
    particle->x = rand_range(add0) + add6;
    rand_range(base);
    particle->y = rand_range(add4) + add2;
    particle->age = 0;
}

/* Set the weather particles' sprite and motion flags for `mode` (1..4), point the tint at the
 * weather table and bind the weather animation; each particle is then respawned. */
void weather_fx_set_mode(s16 mode) {
    FarmParticle *entry;
    s32 base;
    u32 i;
    u16 flags;

    if ((u16)(mode - 1) < 4) {
        mode--;
        base = mode - 2;
        if (mode < 2)
            base = mode;
        weather_drift_table = drift_by_weather;
        drift_reset();
        anim_bind_file(&weather_anim, (AnimFile *)farm_area.stageData);
        entry = weather_particles;
        i = 0;
        do {
            entry->sprite = base + 0x19;
            if (mode == 1) {
                entry->flags |= 0x4000;
                if (!(i & 3))
                    entry->flags |= 0x2000;
                if (!(i & 7))
                    entry->sprite = 0;
            }
            if (mode == 3) {
                flags = entry->flags | 0x4000;
                entry->flags = flags | 0x1000;
                if (!(i & 1))
                    entry->flags = flags | 0x3000;
            }
            particle_respawn(entry);
            i++;
            entry++;
        } while (i < 40);
    }
}

/* In a storm or a blizzard, on every eighth unpaused frame, shake the scroll's centre z by up to 50
 * either way. */
void weather_scroll_nudge(s32 unused) {
    FarmState *farm;
    u32 flags;
    u32 mode;
    u32 elapsed;

    farm = &game_state;
    flags = farm->globalFlags;
    if ((flags & GLOBAL_FLAG_PAUSED) == 0) {
        if ((u32)((u16)farm->world.weather - WEATHER_STORM) < 2) {
            mode = farm->frameCount;
            if ((mode & 7) == 3) {
                elapsed = rand_range(100);
                farm_area.mode.scroll.centre.vz -= elapsed - 50;
            }
        }
    }
}

/* Roll whether a greenhouse with a level is lost (31 in 100). If so, clear its level, its cells, its
 * estate flag and the two greenhouse event flags, and put the blocked animals back. */
void greenhouse_reset_level(void) {
    u16 gate;
    u32 flags;

    gate = game_state.progress.estate.greenhouseLevel;
    if (gate != 0) {
        if ((u32)rand_range(100) < 31) {
            game_state.progress.estate.greenhouseLevel = 0;
            greenhouse_clear_cells();
            farm_put_back_blocked_animals();
            estate_trigger_stamp_first();
            estate_trigger_stamp_second();
            flags = game_state.progress.estate.estateFlags;
            game_state.progress.estate.estateFlags = flags & ~ESTATE_FLAG_GREENHOUSE;
            event_flag_set(EVENT_FLAG_GREENHOUSE_BUILT, 0);
            event_flag_set(EVENT_FLAG_GREENHOUSE_LEVEL_1, 0);
        }
    }
}
