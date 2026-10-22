#include "resource_file.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static bool relative_path_valid(const char *path)
{
    if (!path || !*path || path[0] == '/' || strchr(path, '\\') || strchr(path, ':')) {
        return false;
    }
    const char *part = path;
    for (const char *p = path; ; ++p) {
        if (*p == '/' || *p == '\0') {
            const size_t size = (size_t)(p - part);
            if (size == 0 || (size == 1 && part[0] == '.')
                    || (size == 2 && part[0] == '.' && part[1] == '.')) {
                return false;
            }
            part = p + 1;
        }
        if (!*p) {
            return true;
        }
    }
}

bool hmbtn_resource_read(const char *root, const char *relative_path, uint32_t offset,
    uint8_t *destination, size_t length, char *error, size_t error_size)
{
    if (!root || !*root || !relative_path_valid(relative_path) || (!destination && length)) {
        snprintf(error, error_size, "Invalid resource path or output buffer");
        return false;
    }
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/%s", root, relative_path) < 0) {
        snprintf(error, error_size, "Cannot allocate resource path");
        return false;
    }
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    SDL_free(path);
    if (!file) {
        snprintf(error, error_size, "Cannot open %s: %s", relative_path, SDL_GetError());
        return false;
    }
    const Sint64 size = SDL_GetIOSize(file);
    bool success = false;
    if (size < 0 || (uint64_t)offset > (uint64_t)size
            || (uint64_t)length > (uint64_t)size - offset) {
        snprintf(error, error_size, "Resource interval outside %s", relative_path);
    } else if (SDL_SeekIO(file, (Sint64)offset, SDL_IO_SEEK_SET) < 0
            || SDL_ReadIO(file, destination, length) != length) {
        snprintf(error, error_size, "Cannot read %s: %s", relative_path, SDL_GetError());
    } else {
        success = true;
    }
    if (!SDL_CloseIO(file) && success) {
        snprintf(error, error_size, "Cannot close resource: %s", SDL_GetError());
        success = false;
    }
    return success;
}
