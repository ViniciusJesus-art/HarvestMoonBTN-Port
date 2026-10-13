#ifndef MESSAGES_H
#define MESSAGES_H

/* Farm messages: the calendar and special-date messages, the message pairs and
 * their scrolling window, and the ending check. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void message_state_reset(void);
void message_machine_start(void);
void message_machine_step(void);

#endif /* MESSAGES_H */
