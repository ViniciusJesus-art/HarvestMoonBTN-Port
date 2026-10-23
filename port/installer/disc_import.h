#ifndef HMBTN_DISC_IMPORT_H
#define HMBTN_DISC_IMPORT_H
#include <SDL3/SDL.h>
/* Imports resources only. Never copies the disc, executable, HDT or MIPS overlays.
 * Destination must not exist. A failure/cancel removes only our staging files. */
bool hmbtn_disc_import(const char *image,const char *destination,SDL_AtomicInt *progress,
    SDL_AtomicInt *cancel,char *error,size_t error_size);
#endif
