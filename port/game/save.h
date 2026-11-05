#ifndef HMBTN_SAVE_H
#define HMBTN_SAVE_H
#include "session.h"
/* Native versioned field codec, never a memory image of HmbtnSession/PS1.
 * Caller owns encoded bytes and the newly allocated, resource-detached session.
 * Decode failure returns NULL and cannot mutate a live session. */
enum { HMBTN_SAVE_VERSION=28,HMBTN_SAVE_MAX_BYTES=4*1024*1024 };
bool hmbtn_save_available(const HmbtnSession *,char *error,size_t error_size);
bool hmbtn_save_encode(const HmbtnSession *,uint8_t **data,size_t *bytes,char *error,size_t error_size);
HmbtnSession *hmbtn_save_decode(const uint8_t *data,size_t bytes,char *error,size_t error_size);
/* Validate an existing snapshot and add/replace only its native farm identity.
 * Gameplay sections remain byte-for-byte unchanged; no resource-ready fiction. */
bool hmbtn_save_with_identity(const uint8_t *,size_t,const uint32_t id[4],uint8_t **out,size_t *bytes,char *error,size_t size);
#endif
