#ifndef HMBTN_SCENE_INSPECTION_H
#define HMBTN_SCENE_INSPECTION_H
#include "scene_preview.h"
#include "runtime.h"
#include "house_ui.h"
#include "farm_menu_ui.h"
#include "estate_art.h"
#include "village_art.h"
#include "title_audio.h"
#include "xa_stream.h"
/* Borrowed inspection/resource views for tools and integration hosts. Never
 * retain across reload/destruction. The opaque renderer has no gameplay copy. */
typedef struct HmbtnSceneResources {
    const uint8_t *section,*schedules;
    const HmbtnSceneData *scene;
    const char *root;
    HmbtnNewGameUI *ui;
    HmbtnHouseUI *house_ui;
    HmbtnFarmMenuUI *menu_ui;
    const HmbtnEstateArt *estate_art;
    const HmbtnVillageArt *village_art;
    HmbtnTitleAudio *audio;
    HmbtnXAStream *xa;
} HmbtnSceneResources;
HmbtnSession *hmbtn_scene_preview_session(HmbtnScenePreview *scene);
HmbtnSceneResources hmbtn_scene_preview_resources(HmbtnScenePreview *scene);
HmbtnSessionFrame hmbtn_scene_preview_frame(HmbtnScenePreview *scene);
/* Explicit development/resource operations. Gameplay transitions use session tick. */
bool hmbtn_scene_preview_load_area(HmbtnScenePreview *,unsigned,char *,size_t);
void hmbtn_scene_preview_invalidate(HmbtnScenePreview *);
bool hmbtn_scene_preview_rebind(HmbtnScenePreview *,const uint8_t *,size_t,char *,size_t);
bool hmbtn_scene_preview_area_location(HmbtnScenePreview *,unsigned,unsigned *,char *,size_t);
#endif
