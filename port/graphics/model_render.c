/* Native CPU implementation of the prelit model.c path. Integer view matrices,
 * affine UVs, 5-bit color and painter ordering; no MIPS code or PS1 SDK calls.
 * Exact GTE divide/normalization and GPU edge parity remain comparison work. */
#include "model_render.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct ScreenVertex { int32_t x, y, z; } ScreenVertex;
typedef struct ViewMatrix { int32_t m[3][3], t[3]; } ViewMatrix;
typedef struct DrawPolygon {
    HmbtnModelPrimitive primitive;
    ScreenVertex vertex[4];
    uint32_t order, bucket;
} DrawPolygon;
struct HmbtnModelRenderer { DrawPolygon *polygons; uint32_t capacity; };

HmbtnModelRenderer *hmbtn_model_renderer_create(uint32_t capacity)
{
    if (!capacity || capacity > 16384) return NULL;
    HmbtnModelRenderer *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->polygons = calloc(capacity, sizeof(*r->polygons));
    if (!r->polygons) { free(r); return NULL; }
    r->capacity = capacity;
    return r;
}

void hmbtn_model_renderer_destroy(HmbtnModelRenderer *r)
{
    if (r) free(r->polygons);
    free(r);
}

static int64_t floor_div(int64_t n, int64_t d)
{
    const int64_t q = n / d;
    return q - (n < 0 && n % d != 0);
}

static uint64_t integer_sqrt(uint64_t value)
{
    uint64_t result = 0, bit = UINT64_C(1) << 62;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return result;
}

static bool normalize(int32_t v[3])
{
    uint64_t sum = 0;
    for (unsigned i = 0; i < 3; ++i) sum += (uint64_t)((int64_t)v[i] * v[i]);
    /* Keep twelve fractional bits even for a short direction such as (1,1,1).
     * build_view bounds ensure sum<<24 fits in uint64_t. */
    const uint64_t length = integer_sqrt(sum << 24);
    if (!length) return false;
    for (unsigned i = 0; i < 3; ++i) v[i] = (int32_t)((int64_t)v[i] * 16777216 / (int64_t)length);
    return true;
}

static bool build_view(const HmbtnCamera *c, ViewMatrix *v)
{
    if (!c || !c->projection || c->projection > 4096) return false;
    for (unsigned i = 0; i < 3; ++i) {
        if (c->eye[i] < -131072 || c->eye[i] > 131072
                || c->target[i] < -131072 || c->target[i] > 131072) return false;
        v->m[2][i] = c->target[i] - c->eye[i];
    }
    if (!normalize(v->m[2])) return false;
    v->m[0][0] = v->m[2][2]; v->m[0][1] = 0; v->m[0][2] = -v->m[2][0];
    if (!normalize(v->m[0])) { v->m[0][0] = 4096; v->m[0][2] = 0; }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned a = (i + 1) % 3, b = (i + 2) % 3;
        v->m[1][i] = (int32_t)floor_div((int64_t)v->m[2][a] * v->m[0][b]
            - (int64_t)v->m[2][b] * v->m[0][a], 4096);
    }
    if (!normalize(v->m[1])) return false;
    for (unsigned i = 0; i < 3; ++i) {
        int64_t dot = 0;
        for (unsigned j = 0; j < 3; ++j) dot -= (int64_t)v->m[i][j] * c->eye[j];
        v->t[i] = (int32_t)floor_div(dot, 4096);
    }
    return true;
}

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    return value < low ? low : (value > high ? high : value);
}

static bool project(const ViewMatrix *v, uint16_t h, const int32_t p[3], ScreenVertex *out)
{
    int32_t view[3];
    for (unsigned i = 0; i < 3; ++i) {
        int64_t dot = 0;
        for (unsigned j = 0; j < 3; ++j) dot += (int64_t)v->m[i][j] * p[j];
        view[i] = (int32_t)floor_div(dot, 4096) + v->t[i];
    }
    if (view[2] <= h / 2 || view[2] > 65535
            || view[0] < -32768 || view[0] > 32767 || view[1] < -32768 || view[1] > 32767) return false;
    out->x = clamp(160 + (int32_t)floor_div((int64_t)view[0] * h, view[2]), -1024, 1023);
    out->y = clamp(120 + (int32_t)floor_div((int64_t)view[1] * h, view[2]), -1024, 1023);
    out->z = view[2];
    return true;
}

bool hmbtn_model_project_point(const HmbtnCamera *camera,const int32_t pos[3],int *x,int *y)
{
    ViewMatrix view;ScreenVertex screen;
    if(!pos || !x || !y || !build_view(camera,&view) || !project(&view,camera->projection,pos,&screen)) return false;
    *x=screen.x;*y=screen.y;return true;
}
static int compare_polygon(const void *left, const void *right)
{
    const DrawPolygon *a = left, *b = right;
    if (a->bucket != b->bucket) return a->bucket < b->bucket ? 1 : -1;
    return a->order < b->order ? 1 : (a->order > b->order ? -1 : 0);
}

static bool sample(const HmbtnImageAtlas *a, const HmbtnModelPrimitive *p,
    unsigned u, unsigned v, uint16_t *color)
{
    const unsigned depth = (p->page >> 7) & 3u, per_word = 4u >> depth;
    const unsigned x = (((p->page & 15u) * 64) + (u & 255u) / per_word) & 1023u;
    const unsigned y = (((p->page & 16u) * 16) + (v & 255u)) & 511u;
    const size_t at = (size_t)y * 1024 + x;
    if (!a->present[at]) return false;
    const uint16_t texel = a->words[at];
    if (depth == 2) { *color = texel; return true; }
    const unsigned bits = 4u << depth;
    const unsigned index = (texel >> (((u & 255u) % per_word) * bits)) & ((1u << bits) - 1u);
    const unsigned cx = (((p->clut & 63u) * 16) + index) & 1023u, cy = (p->clut >> 6) & 511u;
    const size_t palette = (size_t)cy * 1024 + cx;
    if (!a->present[palette]) return false;
    *color = a->words[palette];
    return true;
}

static int64_t edge(const ScreenVertex *a, const ScreenVertex *b, int32_t x, int32_t y)
{
    return (int64_t)(b->x - a->x) * (y - 2 * a->y)
        - (int64_t)(b->y - a->y) * (x - 2 * a->x);
}

static bool inclusive(const ScreenVertex *a, const ScreenVertex *b)
{
    return b->y < a->y || (b->y == a->y && b->x > a->x);
}

static bool draw_triangle(const DrawPolygon *poly, unsigned ai, unsigned bi, unsigned ci,
    const HmbtnImageAtlas *atlas, uint8_t *rgba)
{
    static const int dither[4][4] = {{-4,0,-3,1},{2,-2,3,-1},{-3,1,-4,0},{3,-1,2,-2}};
    const ScreenVertex *a = &poly->vertex[ai], *b = &poly->vertex[bi], *c = &poly->vertex[ci];
    const int64_t area = edge(a, b, 2 * c->x, 2 * c->y);
    if (area <= 0) return true;
    int min_x = a->x, max_x = a->x, min_y = a->y, max_y = a->y;
    const ScreenVertex *others[2] = {b,c};
    for (unsigned i = 0; i < 2; ++i) {
        if (others[i]->x < min_x) min_x = others[i]->x;
        if (others[i]->x > max_x) max_x = others[i]->x;
        if (others[i]->y < min_y) min_y = others[i]->y;
        if (others[i]->y > max_y) max_y = others[i]->y;
    }
    min_x = clamp(min_x, 0, 320); max_x = clamp(max_x, -1, 319);
    min_y = clamp(min_y, 0, 240); max_y = clamp(max_y, -1, 239);
    const HmbtnModelPrimitive *p = &poly->primitive;
    const bool textured = (p->command & 4u) != 0, semi = (p->command & 2u) != 0;
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const int64_t wa = edge(b,c,2*x+1,2*y+1), wb = edge(c,a,2*x+1,2*y+1), wc = edge(a,b,2*x+1,2*y+1);
            if (wa < 0 || wb < 0 || wc < 0 || (!wa && !inclusive(b,c))
                    || (!wb && !inclusive(c,a)) || (!wc && !inclusive(a,b))) continue;
            uint16_t texel = 0;
            if (textured) {
                const unsigned u = (unsigned)((wa*p->uv[ai][0] + wb*p->uv[bi][0] + wc*p->uv[ci][0]) / area);
                const unsigned v = (unsigned)((wa*p->uv[ai][1] + wb*p->uv[bi][1] + wc*p->uv[ci][1]) / area);
                if (!sample(atlas,p,u,v,&texel)) return false;
                if (!texel) continue;
            }
            uint8_t *pixel = rgba + ((size_t)y*320+(unsigned)x)*4;
            for (unsigned channel = 0; channel < 3; ++channel) {
                int color = (int)((wa*p->color[ai][channel] + wb*p->color[bi][channel]
                    + wc*p->color[ci][channel]) / area);
                if (textured) color = (int)((texel >> (channel*5)) & 31u) * color / 16;
                color = clamp(color + dither[y&3][x&3],0,255) >> 3;
                if (semi && (!textured || (texel & 0x8000u))) {
                    const int background = pixel[channel] >> 3;
                    switch ((p->page >> 5) & 3u) {
                    case 0: color = background/2 + color/2; break;
                    case 1: color = background + color; break;
                    case 2: color = background - color; break;
                    case 3: color = background + color/4; break;
                    }
                    color = clamp(color,0,31);
                }
                pixel[channel] = (uint8_t)((color << 3) | (color >> 2));
            }
            pixel[3] = 255;
        }
    }
    return true;
}

bool hmbtn_model_render(HmbtnModelRenderer *r, const HmbtnModel *m,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera, uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size)
{
    return hmbtn_model_render_sprites(r,m,atlas,camera,NULL,0,rgba,rgba_size,drawn,error,error_size);
}

static bool draw_actor(const HmbtnWorldSprite *s,const ScreenVertex *screen,unsigned index,
    const HmbtnWorldAttachment *attachments,unsigned count,uint8_t *rgba,size_t bytes,char *error,size_t size)
{
    if(!hmbtn_animation_draw(s->animation,s->atlas,&s->player,screen->x,screen->y,
            rgba,320,240,bytes,error,size)) return false;
    for(unsigned i=0;i<count;++i) if(attachments[i].sprite==index) {
        const HmbtnWorldAttachment *a=&attachments[i];
        if(!hmbtn_animation_draw(a->animation,a->atlas,&a->player,screen->x+a->screen_x,
                screen->y+a->screen_y,rgba,320,240,bytes,error,size)) return false;
    }
    return true;
}

bool hmbtn_model_render_instances(HmbtnModelRenderer *r,
    const HmbtnModelInstance *instances,unsigned instance_count,
    const HmbtnImageAtlas *atlas,const HmbtnCamera *camera,
    const HmbtnWorldSprite *sprites,unsigned sprite_count,uint8_t *rgba,
    size_t rgba_size,uint32_t *drawn,char *error,size_t error_size)
{
    return hmbtn_model_render_instances_attached(r,instances,instance_count,atlas,camera,
        sprites,sprite_count,NULL,0,rgba,rgba_size,drawn,error,error_size);
}

bool hmbtn_model_render_instances_attached(HmbtnModelRenderer *r,
    const HmbtnModelInstance *instances, unsigned instance_count,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera,
    const HmbtnWorldSprite *sprites, unsigned sprite_count,
    const HmbtnWorldAttachment *attachments,unsigned attachment_count,uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size)
{
    ViewMatrix view;
    if (drawn) *drawn = 0;
    if (!r || !instances || !instance_count || instance_count > 32 || !atlas
            || !rgba || rgba_size < 320*240*4 || sprite_count > 512
            || (sprite_count && !sprites) || (attachment_count && !attachments)
            || attachment_count>512 || !build_view(camera,&view)) {
        snprintf(error,error_size,"Invalid model renderer, frame or camera"); return false;
    }
    for(unsigned i=0;i<attachment_count;++i) {
        const HmbtnWorldAttachment *a=&attachments[i];
        if(a->sprite>=sprite_count || !a->animation || !a->atlas
                || a->screen_x < -320 || a->screen_x > 320 || a->screen_y < -240 || a->screen_y > 240) {
            snprintf(error,error_size,"Invalid world sprite attachment");return false;
        }
    }
    uint32_t total = 0, count = 0, order = 0;
    for (unsigned instance = 0; instance < instance_count; ++instance) {
        const HmbtnModelInstance *item = &instances[instance];
        const HmbtnModel *m = item->model;
        if (!m || !m->data || m->primitive_count > r->capacity-total) {
            snprintf(error,error_size,"Scene exceeds model ordering capacity"); return false;
        }
        total += m->primitive_count;
        int32_t sine[3], cosine[3];
        const bool rotated = ((item->angle[0]|item->angle[1]|item->angle[2])&4095u) != 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (item->position[axis] < -32768 || item->position[axis] > 32767) {
                snprintf(error,error_size,"Scene translation outside signed 16-bit bounds"); return false;
            }
            const double angle = (item->angle[axis]&4095u)*6.2831853071795864769/4096;
            sine[axis] = (int32_t)lround(sin(angle)*4096);
            cosine[axis] = (int32_t)lround(cos(angle)*4096);
        }
        for (uint32_t i = 0; i < m->object_count; ++i) {
            HmbtnModelObject object;
            if (!hmbtn_model_object(m,i,&object)) return false;
            size_t at = 12 + (size_t)object.primitives;
            for (uint32_t j = 0; j < object.primitive_count; ++j, ++order) {
                DrawPolygon poly = {0};
                if (!hmbtn_model_primitive(m,at,&poly.primitive)) return false;
                at += poly.primitive.bytes;
                bool visible = true;
                int32_t sum_z = 0;
                for (unsigned k = 0; k < poly.primitive.count; ++k) {
                    int32_t position[3];
                    hmbtn_model_vertex(m,&object,poly.primitive.vertex[k],position);
                    const unsigned axes[3] = {2,0,1};
                    for (unsigned rotation = 0; rotated && rotation < 3; ++rotation) {
                        const unsigned axis = axes[rotation], a = (axis+1)%3, b = (axis+2)%3;
                        const int32_t pa = position[a], pb = position[b];
                        position[a] = (int32_t)floor_div((int64_t)pa*cosine[axis]-(int64_t)pb*sine[axis],4096);
                        position[b] = (int32_t)floor_div((int64_t)pa*sine[axis]+(int64_t)pb*cosine[axis],4096);
                    }
                    for (unsigned axis = 0; axis < 3; ++axis) position[axis] += item->position[axis];
                    if (!project(&view,camera->projection,position,&poly.vertex[k])) visible = false;
                    sum_z += poly.vertex[k].z;
                }
                if (!visible || edge(&poly.vertex[0],&poly.vertex[1],2*poly.vertex[2].x,2*poly.vertex[2].y) <= 0) continue;
                const int32_t depth = sum_z * (poly.primitive.count == 3 ? 341 : 256) / 4096;
                poly.bucket = (uint32_t)(depth + ((poly.primitive.flags & 0x8000u) ? 0 : 35)
                    + ((poly.primitive.flags & 0x4000u) ? 1 : 0)) / 4;
                if (poly.bucket >= 2048) continue;
                poly.order = order;
                r->polygons[count++] = poly;
            }
        }
    }
    qsort(r->polygons,count,sizeof(*r->polygons),compare_polygon);
    struct { const HmbtnWorldSprite *sprite; ScreenVertex screen; uint32_t bucket; unsigned index; } actor[512];
    unsigned actors = 0;
    for (unsigned i = 0; i < sprite_count; ++i) {
        if (!sprites[i].animation || !sprites[i].atlas) {
            snprintf(error,error_size,"World sprite is missing its animation or atlas"); return false;
        }
        int32_t position[3];
        for (unsigned k = 0; k < 3; ++k) {
            if (sprites[i].position[k] < -32768 || sprites[i].position[k] > 32767) {
                snprintf(error,error_size,"World sprite position outside signed 16-bit bounds"); return false;
            }
            position[k] = sprites[i].position[k] + (k == 0 ? 150 : k == 1 ? -100 : -150);
        }
        ScreenVertex screen;
        if (!project(&view,camera->projection,position,&screen)) continue;
        if (sprites[i].screen_y < -240 || sprites[i].screen_y > 240) {
            snprintf(error,error_size,"World sprite screen offset outside bounds"); return false;
        }
        screen.y += sprites[i].screen_y;
        const uint32_t bucket = (uint32_t)screen.z / 16;
        if (bucket >= 2048) continue;
        unsigned j = actors++;
        while (j && actor[j-1].bucket <= bucket) { actor[j] = actor[j-1]; --j; }
        actor[j].sprite = &sprites[i]; actor[j].screen = screen; actor[j].bucket = bucket; actor[j].index=i;
    }
    unsigned next_actor = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const DrawPolygon *p = &r->polygons[i];
        while (next_actor < actors && actor[next_actor].bucket >= p->bucket) {
            const HmbtnWorldSprite *s = actor[next_actor].sprite;
            if (!draw_actor(s,&actor[next_actor].screen,actor[next_actor].index,
                    attachments,attachment_count,rgba,rgba_size,error,error_size)) return false;
            ++next_actor;
        }
        if (!draw_triangle(p,0,1,2,atlas,rgba)
                || (p->primitive.count == 4 && !draw_triangle(p,1,3,2,atlas,rgba))) {
            snprintf(error,error_size,"Model polygon reads a missing texture or palette (page %x, clut %x, uv %u,%u)",
                p->primitive.page,p->primitive.clut,p->primitive.uv[0][0],p->primitive.uv[0][1]); return false;
        }
    }
    while (next_actor < actors) {
        const HmbtnWorldSprite *s = actor[next_actor].sprite;
        if (!draw_actor(s,&actor[next_actor].screen,actor[next_actor].index,
                attachments,attachment_count,rgba,rgba_size,error,error_size)) return false;
        ++next_actor;
    }
    if (drawn) *drawn = count;
    return true;
}

bool hmbtn_model_render_sprites(HmbtnModelRenderer *r, const HmbtnModel *m,
    const HmbtnImageAtlas *atlas, const HmbtnCamera *camera,
    const HmbtnWorldSprite *sprites, unsigned sprite_count, uint8_t *rgba,
    size_t rgba_size, uint32_t *drawn, char *error, size_t error_size)
{
    const HmbtnModelInstance instance = {m,{0},{0}};
    return hmbtn_model_render_instances(r,&instance,1,atlas,camera,sprites,sprite_count,
        rgba,rgba_size,drawn,error,error_size);
}
