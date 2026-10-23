#ifndef HMBTN_DATA_DIRECTORY_H
#define HMBTN_DATA_DIRECTORY_H
#include <stdbool.h>

/* SDL-allocated UTF-8 result; free with SDL_free. Explicit paths take priority.
 * Default search: user resource folder, beside executable, up three parents
 * (development layout), cwd. First launch can import into the user folder. */
char *hmbtn_data_directory(const char *explicit_path);
char *hmbtn_user_data_directory(void);
bool hmbtn_data_directory_available(const char *root);

#endif
