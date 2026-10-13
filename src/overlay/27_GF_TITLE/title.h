#ifndef TITLE_H
#define TITLE_H

/* The title screen. Its objects are all static in title.c, so this header declares none. The
 * title-screen control state is a set of separate globals rather than one struct. */


void title_step_dispatch(void);

#endif /* TITLE_H */
