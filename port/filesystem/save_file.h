#ifndef HMBTN_SAVE_FILE_H
#define HMBTN_SAVE_FILE_H
#include "save.h"
/* UTF-8 paths through SDL. Default path is separate from imported game data;
 * the returned path is SDL-allocated. Explicit parents must already exist. */
char *hmbtn_save_default_path(void);
bool hmbtn_save_file_write(const char *path,const HmbtnSession *,char *error,size_t error_size);
HmbtnSession *hmbtn_save_file_read(const char *path,char *error,size_t error_size);
#endif
