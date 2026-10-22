#include "data_directory.h"
#include <SDL3/SDL.h>

static bool contains_resources(const char *root)
{
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/A_FILE/00_gf.bin", root) < 0) return false;
    SDL_PathInfo info;
    const bool found = SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
    SDL_free(path);
    return found;
}

char *hmbtn_data_directory(const char *explicit_path)
{
    if (explicit_path) return SDL_strdup(explicit_path);
    const char *base = SDL_GetBasePath();
    if (base) {
        static const char *relative[] = {"data", "../data", "../../data", "../../../data"};
        for (size_t i = 0; i < SDL_arraysize(relative); ++i) {
            char *candidate = NULL;
            if (SDL_asprintf(&candidate, "%s%s", base, relative[i]) < 0) return NULL;
            if (contains_resources(candidate)) return candidate;
            SDL_free(candidate);
        }
    }
    return SDL_strdup("data"); /* Final fallback also gives a useful read error. */
}
