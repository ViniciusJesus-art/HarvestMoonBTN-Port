#ifndef CALENDAR_H
#define CALENDAR_H

/* The farm calendar: the month grid, its cursor, the day marks and the event messages. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void farm_calendar_open(void);
void farm_calendar_frame(void);

#endif /* CALENDAR_H */
