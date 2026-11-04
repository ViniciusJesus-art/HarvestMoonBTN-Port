#ifndef HMBTN_SAVE_LIBRARY_H
#define HMBTN_SAVE_LIBRARY_H
#include "save_file.h"
typedef struct HmbtnSaveEntry {
    char *path;
    uint32_t farm_id[4],checksum;
    char player[40],farm[40];
    unsigned gold,berries;
    HmbtnFarmClock clock;
    int64_t modified;
} HmbtnSaveEntry;
typedef struct HmbtnSaveLibrary {
    HmbtnSaveEntry *entries;
    size_t count,skipped;
} HmbtnSaveLibrary;
/* Archives are beside the latest slot, under saves/<stable farm identity>/.
 * No simulation updates or resource loading are performed when listing them. */
char *hmbtn_save_library_root(const char *slot);
void hmbtn_save_library_adopt(HmbtnSession *,const char *loaded_path);
bool hmbtn_save_library_write(const char *root,const char *slot,HmbtnSession *,char *error,size_t size);
bool hmbtn_save_library_scan(HmbtnSaveLibrary *,const char *root,const char *slot,char *error,size_t size);
void hmbtn_save_library_close(HmbtnSaveLibrary *);
bool hmbtn_save_same_farm(const HmbtnSaveEntry *,const HmbtnSaveEntry *);
void hmbtn_save_name(char *out,size_t size,const uint16_t *name);
#endif
