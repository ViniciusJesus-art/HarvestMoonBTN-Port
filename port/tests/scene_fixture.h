#ifndef HMBTN_TEST_SCENE_FIXTURE_H
#define HMBTN_TEST_SCENE_FIXTURE_H
#include "scene_inspection.h"
#include "sdl_host.h"
#include "model_render.h"
#include "psx_data.h"
#include "resource_file.h"
#include "text_resources.h"
#include "event_resources.h"
#include "action_resources.h"
#include "feed_shop_ui.h"
#include "service_resources.h"
#include "service_audio.h"
#include "world_map.h"
#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
enum { FRAME_BYTES=320*240*4 };
static bool test_morning(HmbtnScenePreview *s,bool slept,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    return hmbtn_session_prepare_morning(hmbtn_scene_preview_session(s),&f,slept,error,size);
}
static HmbtnEventResult test_visit_command(HmbtnScenePreview *s,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    return hmbtn_session_visit_command(hmbtn_scene_preview_session(s),&f,c,error,size);
}
static bool test_action(HmbtnScenePreview *s,unsigned id,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    return f.exploration.action_bank(f.context,id,error,size);
}
static bool test_read(HmbtnScenePreview *s,HmbtnServiceKind kind,unsigned id,unsigned bank,uint8_t *data,size_t bytes,uint32_t *value,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    return f.read(f.context,kind,id,bank,data,bytes,value,error,size);
}
static const HmbtnServiceResult *test_audio(HmbtnScenePreview *s,HmbtnServiceKind kind,unsigned id,char *error,size_t size)
{
    HmbtnSessionFrame f=hmbtn_scene_preview_frame(s);
    return f.audio(f.context,kind,id,error,size);
}
#endif
