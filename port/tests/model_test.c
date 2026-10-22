#include "model_render.h"
#include "psx_data.h"
#include "scene_decode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"Failed at line %d: %s (%s)\n",__LINE__,#condition,error); \
    result = 1; goto cleanup; } } while (0)

/* Synthetic prelit model, with one quad/triangle at z=2000. No game data. */
static size_t make_model(uint8_t bytes[256], unsigned key)
{
    static const int16_t vertices[4][3] = {{-80,-80,2000},{80,-80,2000},{-80,80,2000},{80,80,2000}};
    unsigned count, length, indices, colors;
    const bool textured = (key & 0x0400u) != 0, gouraud = (key & 0x1000u) != 0;
    const bool quad = (key & 0x0800u) != 0;
    count = quad ? 4 : 3;
    if (textured) {
        colors = 4 + count*4;
        indices = colors + (gouraud ? count*4 : 4);
        length = (indices + count*2 + 3) & ~3u;
    } else {
        colors = 4;
        indices = 4 + (gouraud ? count*4 : 4);
        length = (indices + count*2 + 3) & ~3u;
    }
    memset(bytes,0,256);
    hmbtn_write_le32(bytes,0x42);
    hmbtn_write_le32(bytes+8,1);
    hmbtn_write_le32(bytes+12,28); /* All addresses relative to the object table. */
    hmbtn_write_le32(bytes+16,4);
    hmbtn_write_le32(bytes+28,60);
    hmbtn_write_le32(bytes+32,1);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < 3; ++c)
            hmbtn_write_le16(bytes+40+i*8+c*2,(uint16_t)vertices[i][c]);
    uint8_t *p = bytes+72;
    hmbtn_write_le16(p,0x8000); /* Repurposed header bytes, not a packet length. */
    hmbtn_write_le16(p+2,(uint16_t)key);
    if (textured) hmbtn_write_le16(p+6,64); /* CLUT (0,1). UV=0, page=0. */
    for (unsigned i = 0; i < count; ++i) {
        hmbtn_write_le16(p+indices+i*2,(uint16_t)i);
        if (gouraud || !i) {
            p[colors+(gouraud ? i*4 : 0)] = textured ? 128 : 255;
            if (textured) p[colors+(gouraud ? i*4 : 0)+1] = p[colors+(gouraud ? i*4 : 0)+2] = 128;
        }
    }
    return 72+length;
}

static void clear_pixels(uint8_t *pixels)
{
    for (size_t i = 0; i < 320*240*4; i += 4) {
        pixels[i] = pixels[i+1] = 0; pixels[i+2] = 40; pixels[i+3] = 255;
    }
}

static void make_sprite(uint8_t ani[82])
{
    const uint16_t counts[5]={2,1,1,1,1};
    const uint32_t offsets[5]={36,52,56,68,72};
    memset(ani,0,82); hmbtn_write_le32(ani,14);
    for (unsigned i=0;i<5;++i) {
        hmbtn_write_le16(ani+4+i*2,counts[i]); hmbtn_write_le32(ani+16+i*4,offsets[i]);
    }
    hmbtn_write_le16(ani+40,2); hmbtn_write_le16(ani+42,8); /* 8x8 sprite. */
    hmbtn_write_le16(ani+46,8); hmbtn_write_le16(ani+48,16); hmbtn_write_le16(ani+50,1);
    hmbtn_write_le16(ani+52,1); hmbtn_write_le16(ani+58,1);
    hmbtn_write_le16(ani+68,1);
    hmbtn_write_le16(ani+74,(uint16_t)-4); hmbtn_write_le16(ani+76,(uint16_t)-4);
    ani[78]=1; ani[79]=255;
}

int main(void)
{
    int result = 0;
    char error[256] = {0};
    uint8_t bytes[256];
    uint8_t *pixels = malloc(320*240*4);
    HmbtnImageAtlas *atlas = calloc(1,sizeof(*atlas));
    HmbtnModelRenderer *renderer = hmbtn_model_renderer_create(4);
    HmbtnModel model = {0};
    const HmbtnCamera camera = {{0,0,0},{0,0,4096},1000};
    const size_t inside = ((size_t)80*320+121)*4;
    uint32_t drawn;
    CHECK(pixels && atlas && renderer);
    atlas->present[0] = atlas->present[1025] = 1;
    atlas->words[0] = 0x1111;
    atlas->words[1025] = 0x001F;
    const unsigned keys[8] = {0x2101,0x2901,0x3101,0x3901,0x2501,0x2D01,0x3501,0x3D01};
    for (unsigned i = 0; i < 8; ++i) {
        const size_t size = make_model(bytes,keys[i]);
        CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
        CHECK(model.object_count == 1 && model.primitive_count == 1);
        clear_pixels(pixels);
        CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
        CHECK(drawn == 1 && pixels[inside] == 255 && pixels[inside+1] == 0 && pixels[inside+2] == 0);
        CHECK(pixels[0] == 0 && pixels[2] == 40); /* Coverage stays inside the polygon. */
        if (keys[i] == 0x2901) CHECK(pixels[((size_t)159*320+199)*4] > 240);
        for (size_t cut = 0; cut < size; ++cut) {
            CHECK(!hmbtn_model_open(&model,bytes,cut,error,sizeof(error)));
            CHECK(model.data == NULL); /* Failure never exposes a partly validated view. */
        }
    }
    size_t size = make_model(bytes,0x2501);
    hmbtn_write_le32(bytes,0x41); /* House furniture: same bounded packet layout. */
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(drawn==1 && pixels[inside]==255);
    hmbtn_write_le32(bytes+12,UINT32_MAX);
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    size=make_model(bytes,0x2501);
    hmbtn_write_le32(bytes,0x40);
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    size=make_model(bytes,0x2501);
    hmbtn_write_le16(bytes+92,4); /* Vertex index outside the declared four vertices. */
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    make_model(bytes,0x2501); hmbtn_write_le32(bytes+12,UINT32_MAX);
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    make_model(bytes,0x2501); hmbtn_write_le32(bytes+4,1); /* Absolute PS1 addresses. */
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    make_model(bytes,0x2501); hmbtn_write_le16(bytes+74,0x2400); /* Unsupported lit format. */
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    make_model(bytes,0x2501); hmbtn_write_le16(bytes+82,0x0180); /* Reserved texture depth. */
    CHECK(!hmbtn_model_open(&model,bytes,size,error,sizeof(error)));

    size = make_model(bytes,0x2501);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    atlas->words[1025] = 0; clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside+2] == 40); /* A zero BGR555 texel is transparent. */
    atlas->words[1025] = 0x8000; clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside] == 0 && pixels[inside+2] == 0); /* Opaque black. */
    atlas->present[1025] = 0;
    CHECK(!hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    atlas->present[1025] = 1; atlas->words[1025] = 0x801F;
    hmbtn_write_le16(bytes+74,0x2701); /* Semi-transparent textured triangle. */
    for (unsigned blend = 0; blend < 4; ++blend) {
        hmbtn_write_le16(bytes+82,(uint16_t)(blend << 5)); clear_pixels(pixels);
        CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
        CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
        const uint8_t expected[4] = {123,255,0,57};
        CHECK(pixels[inside] == expected[blend]);
    }
    /* Exercise 8-bit and direct 16-bit pages independently of indexed 4-bit. */
    hmbtn_write_le16(bytes+74,0x2501); hmbtn_write_le16(bytes+82,0x80);
    atlas->words[0] = 1; atlas->words[1025] = 0x001F;
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside] == 255);
    hmbtn_write_le16(bytes+82,0x100); atlas->words[0] = 0x001F;
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside] == 255);

    /* Equal depth: prepend-only ordering means the first submitted polygon wins. */
    size = make_model(bytes,0x2101);
    memcpy(bytes+size,bytes+72,16); hmbtn_write_le32(bytes+32,2);
    bytes[size+4] = 0; bytes[size+5] = 255;
    CHECK(hmbtn_model_open(&model,bytes,size+16,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(drawn == 2 && pixels[inside] == 255 && pixels[inside+1] == 0);
    /* Independently placed models must share depth ordering with the terrain. */
    uint8_t other_bytes[256]; HmbtnModel other;
    const size_t other_size = make_model(other_bytes,0x2901);
    other_bytes[76] = 0; other_bytes[77] = 255;
    CHECK(hmbtn_model_open(&other,other_bytes,other_size,error,sizeof(error)));
    HmbtnModelInstance instances[HMBTN_SCENE_MODELS+1] = {{&model,{0},{0}}, {&other,{0,0,-400},{0}}};
    clear_pixels(pixels);
    CHECK(hmbtn_model_render_instances(renderer,instances,2,atlas,&camera,NULL,0,
        pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(drawn == 3 && pixels[inside+1] == 255 && pixels[inside] == 0);
    instances[1].position[2] = 400;
    clear_pixels(pixels);
    CHECK(hmbtn_model_render_instances(renderer,instances,2,atlas,&camera,NULL,0,
        pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside] == 255 && pixels[inside+1] == 0);
    instances[1].angle[2] = 1024; instances[1].position[2] = -400;
    clear_pixels(pixels);
    CHECK(hmbtn_model_render_instances(renderer,instances,2,atlas,&camera,NULL,0,
        pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside+1] == 255);
    instances[1].position[0] = INT32_MAX;
    CHECK(!hmbtn_model_render_instances(renderer,instances,2,atlas,&camera,NULL,0,
        pixels,320*240*4,&drawn,error,sizeof(error)));
    /* Serialized scene table: model offset, placement, spin and hidden slots. */
    uint8_t section[0x2200] = {0}; HmbtnSceneData scene;
    memcpy(section+0x2000,other_bytes,other_size);
    memcpy(section+0x2100,other_bytes,other_size);
    for (unsigned i = 0; i < 15; ++i) hmbtn_write_le32(section+0x948+i*44,0x8000);
    hmbtn_write_le32(section+0x948,0x2000);
    hmbtn_write_le32(section+0x94C,0x100);
    hmbtn_write_le16(section+0x948+20,(uint16_t)-5000);
    hmbtn_write_le16(section+0x948+28,1024);
    hmbtn_write_le16(section+0x948+36,16);
    hmbtn_write_le32(section+0x948+44,0x1101);
    hmbtn_write_le32(section+0x94C+44,0x100);
    CHECK(hmbtn_scene_data_open(&scene,section,sizeof(section),error,sizeof(error)));
    CHECK(scene.primitive_capacity == 2);
    hmbtn_scene_data_tick(&scene);
    CHECK(hmbtn_scene_data_instances(&scene,instances) == 2);
    CHECK(instances[0].position[0] == -5000 && instances[0].angle[0] == 1040);
    hmbtn_write_le32(section+0x94C,UINT32_MAX);
    CHECK(!hmbtn_scene_data_open(&scene,section,sizeof(section),error,sizeof(error)));
    hmbtn_write_le16(bytes+72,0); /* +35 original OT depth bias makes red draw first. */
    CHECK(hmbtn_model_open(&model,bytes,size+16,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[inside] == 0 && pixels[inside+1] == 255);
    /* World sprites join the same painter ordering as terrain/buildings. */
    uint8_t ani_bytes[82]; HmbtnAnimation animation;
    make_sprite(ani_bytes);
    CHECK(hmbtn_animation_open(&animation,ani_bytes,sizeof(ani_bytes),error,sizeof(error)));
    for (unsigned row=0;row<8;++row) for (unsigned col=0;col<2;++col) {
        atlas->present[row*1024+col]=1; atlas->words[row*1024+col]=0x1111;
    }
    atlas->present[8*1024+1]=1; atlas->words[8*1024+1]=0x03E0;
    HmbtnWorldSprite sprite={&animation,atlas,{0},{-186,64,1950},0};
    size=make_model(bytes,0x2101);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render_sprites(renderer,&model,atlas,&camera,&sprite,1,pixels,
        320*240*4,&drawn,error,sizeof(error)));
    const size_t actor_pixel=((size_t)100*320+140)*4;
    CHECK(pixels[actor_pixel]==0 && pixels[actor_pixel+1]==255);
    sprite.position[0]=-194; sprite.position[1]=56; sprite.position[2]=2350;
    clear_pixels(pixels);
    CHECK(hmbtn_model_render_sprites(renderer,&model,atlas,&camera,&sprite,1,pixels,
        320*240*4,&drawn,error,sizeof(error)));
    CHECK(pixels[actor_pixel]>240 && pixels[actor_pixel+1]==0); /* Red occludes the farther actor. */
    CHECK(!hmbtn_model_render_sprites(renderer,&model,atlas,&camera,&sprite,513,pixels,
        320*240*4,NULL,error,sizeof(error)));
    /* Winding/near plane: rejected geometry cannot write pixels. */
    size = make_model(bytes,0x2101);
    hmbtn_write_le16(bytes+82,2); hmbtn_write_le16(bytes+84,1);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    clear_pixels(pixels);
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(drawn == 0 && pixels[inside+2] == 40);
    size = make_model(bytes,0x2101);
    for (unsigned i = 0; i < 4; ++i) hmbtn_write_le16(bytes+44+i*8,1);
    CHECK(hmbtn_model_open(&model,bytes,size,error,sizeof(error)));
    CHECK(hmbtn_model_render(renderer,&model,atlas,&camera,pixels,320*240*4,&drawn,error,sizeof(error)));
    CHECK(drawn == 0);
    HmbtnCamera bad = camera; bad.target[2] = 0;
    CHECK(!hmbtn_model_render(renderer,&model,atlas,&bad,pixels,320*240*4,NULL,error,sizeof(error)));
    bad = camera; bad.eye[0] = INT32_MAX;
    CHECK(!hmbtn_model_render(renderer,&model,atlas,&bad,pixels,320*240*4,NULL,error,sizeof(error)));
    CHECK(!hmbtn_model_render(renderer,&model,atlas,&camera,pixels,1,NULL,error,sizeof(error)));
    puts("Bounded prelit TMD, projection, raster coverage, indexed textures and ordering passed.");
cleanup:
    free(pixels); free(atlas); hmbtn_model_renderer_destroy(renderer);
    return result;
}
