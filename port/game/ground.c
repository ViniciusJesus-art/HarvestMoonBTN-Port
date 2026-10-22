#include "ground.h"
#include "psx_data.h"
#include <limits.h>

static int32_t half(const uint8_t *p)
{
    const uint16_t v = hmbtn_read_le16(p);
    return v < 32768 ? v : (int32_t)v-65536;
}
static bool triangle(const int32_t v[4][3], unsigned a, unsigned b, unsigned c,
    const int32_t pos[3], unsigned tolerance, int32_t *height)
{
    const int32_t *p = v[a], *q = v[b], *r = v[c];
    const int64_t dy = (int64_t)pos[1]-(p[1]+q[1]+r[1])/3;
    if (dy > (int64_t)tolerance || dy < -(int64_t)tolerance) return false;
    const int64_t e0 = (int64_t)(q[0]-p[0])*(pos[2]-p[2])-(int64_t)(q[2]-p[2])*(pos[0]-p[0]);
    const int64_t e1 = (int64_t)(r[0]-q[0])*(pos[2]-q[2])-(int64_t)(r[2]-q[2])*(pos[0]-q[0]);
    const int64_t e2 = (int64_t)(p[0]-r[0])*(pos[2]-r[2])-(int64_t)(p[2]-r[2])*(pos[0]-r[0]);
    if ((e0 > 0 || e1 > 0 || e2 > 0) && (e0 < 0 || e1 < 0 || e2 < 0)) return false;
    const int64_t ux=q[0]-p[0], uy=q[1]-p[1], uz=q[2]-p[2];
    const int64_t vx=r[0]-q[0], vy=r[1]-q[1], vz=r[2]-q[2];
    const int64_t nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
    const int64_t y = ny ? -((pos[0]-q[0])*nx+(pos[2]-q[2])*nz)/ny+q[1] : pos[1];
    if (y < INT32_MIN || y > INT32_MAX) return false;
    *height=(int32_t)y;
    return true;
}
bool hmbtn_ground_find(const HmbtnModel *m, const int32_t pos[3],
    uint16_t mask, unsigned tolerance, HmbtnGroundHit *hit)
{
    if (!m || !m->data || !pos || !hit) return false;
    for(unsigned axis=0;axis<3;++axis) if(pos[axis]<-32768 || pos[axis]>32767) return false;
    /* The original's multi-object centre table starts with a signed clip word. */
    const size_t centres = 12+(size_t)m->object_count*28;
    const uint32_t word=m->object_count > 1 ? hmbtn_read_le32(m->data+centres) : 0;
    const int64_t clip=word <= INT32_MAX ? word : (int64_t)word-INT64_C(4294967296);
    const int64_t limit=clip/2+600;
    for (uint32_t i=0;i<m->object_count;++i) {
        if (m->object_count > 1) {
            const uint8_t *centre=m->data+centres+4+(size_t)i*8;
            const int64_t dx=(int64_t)half(centre)-pos[0], dz=(int64_t)half(centre+4)-pos[2];
            if (dx <= -limit || dx >= limit || dz <= -limit || dz >= limit) continue;
        }
        HmbtnModelObject object;
        if (!hmbtn_model_object(m,i,&object)) return false;
        size_t at=12+(size_t)object.primitives;
        for (uint32_t j=0;j<object.primitive_count;++j) {
            HmbtnModelPrimitive p;
            if (!hmbtn_model_primitive(m,at,&p)) return false;
            if (!(p.flags&mask)) {
                int32_t v[4][3], height;
                for (unsigned k=0;k<p.count;++k) hmbtn_model_vertex(m,&object,p.vertex[k],v[k]);
                bool found;
                if (p.count==3) found=triangle((const int32_t (*)[3])v,0,1,2,pos,tolerance,&height);
                else if (p.vertex[0]==p.vertex[1] || p.vertex[0]==p.vertex[2])
                    found=triangle((const int32_t (*)[3])v,2,1,3,pos,tolerance,&height);
                else if (p.vertex[3]==p.vertex[1] || p.vertex[3]==p.vertex[2])
                    found=triangle((const int32_t (*)[3])v,0,1,2,pos,tolerance,&height);
                else found=triangle((const int32_t (*)[3])v,0,1,2,pos,tolerance,&height)
                    || triangle((const int32_t (*)[3])v,3,2,1,pos,tolerance,&height);
                if (found) { *hit=(HmbtnGroundHit){height,p.flags,i,at}; return true; }
            }
            at+=p.bytes;
        }
    }
    return false;
}
