#include "data_directory.h"
#include <SDL3/SDL.h>

bool hmbtn_data_directory_available(const char *root)
{
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/A_FILE/00_gf.bin", root) < 0) return false;
    SDL_PathInfo info;
    const bool found = SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
    SDL_free(path);
    return found;
}
char *hmbtn_user_data_directory(void)
{
    char *base=SDL_GetPrefPath("hmbtn","HarvestMoonBTN");
    if(!base) return NULL;
    char *path=NULL;SDL_asprintf(&path,"%sdata",base);SDL_free(base);return path;
}

char *hmbtn_data_directory(const char *explicit_path)
{
    if (explicit_path) return SDL_strdup(explicit_path);
    char *user=hmbtn_user_data_directory();
    if(user && hmbtn_data_directory_available(user)) return user;
    SDL_free(user);
    const char *base = SDL_GetBasePath();
    if (base) {
        static const char *relative[] = {"data", "../data", "../../data", "../../../data"};
        for (size_t i = 0; i < SDL_arraysize(relative); ++i) {
            char *candidate = NULL;
            if (SDL_asprintf(&candidate, "%s%s", base, relative[i]) < 0) return NULL;
            if (hmbtn_data_directory_available(candidate)) return candidate;
            SDL_free(candidate);
        }
    }
    return SDL_strdup("data"); /* Final fallback also gives a useful read error. */
}
