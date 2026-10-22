#ifndef HMBTN_DATA_DIRECTORY_H
#define HMBTN_DATA_DIRECTORY_H

/* SDL-allocated UTF-8 result; free with SDL_free. Explicit paths take priority.
 * Default search: beside executable, up three parents (development layout), cwd. */
char *hmbtn_data_directory(const char *explicit_path);

#endif
