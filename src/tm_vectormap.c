#include "tinymap.h"
#include <stdlib.h>
#include <string.h>

TM_VectorMap* tm_vectormap_create(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 16;
    TM_VectorMap *vmap = (TM_VectorMap*)malloc(sizeof(TM_VectorMap));
    if (!vmap) return NULL;
    vmap->features = (TM_Feature*)calloc(initial_capacity, sizeof(TM_Feature));
    vmap->count = 0;
    vmap->capacity = initial_capacity;
    return vmap;
}

void tm_vectormap_destroy(TM_VectorMap *vmap) {
    if (!vmap) return;
    if (vmap->features) {
        for (int i = 0; i < vmap->count; i++) {
            if (vmap->features[i].points) {
                free(vmap->features[i].points);
            }
        }
        free(vmap->features);
    }
    free(vmap);
}

static bool ensure_capacity(TM_VectorMap *vmap) {
    if (vmap->count >= vmap->capacity) {
        int new_cap = vmap->capacity * 2;
        TM_Feature *new_feats = (TM_Feature*)realloc(vmap->features, new_cap * sizeof(TM_Feature));
        if (!new_feats) return false;
        vmap->features = new_feats;
        vmap->capacity = new_cap;
    }
    return true;
}

int tm_vectormap_add_marker(TM_VectorMap *vmap, TM_Vec2 pos, int radius, Color color) {
    if (!vmap || !ensure_capacity(vmap)) return -1;
    
    TM_Feature *feat = &vmap->features[vmap->count++];
    feat->type = TM_FEATURE_POINT;
    feat->color = color;
    feat->fill_color = color;
    feat->filled = true;
    feat->radius = radius > 0 ? radius : 3;
    feat->point_count = 1;
    feat->points = (TM_Vec2*)malloc(sizeof(TM_Vec2));
    feat->points[0] = pos;
    
    return vmap->count - 1;
}

int tm_vectormap_add_line(TM_VectorMap *vmap, const TM_Vec2 *points, int count, Color color) {
    if (!vmap || !points || count < 2 || !ensure_capacity(vmap)) return -1;

    TM_Feature *feat = &vmap->features[vmap->count++];
    feat->type = TM_FEATURE_LINE;
    feat->color = color;
    feat->fill_color = color;
    feat->filled = false;
    feat->radius = 0;
    feat->point_count = count;
    feat->points = (TM_Vec2*)malloc(count * sizeof(TM_Vec2));
    memcpy(feat->points, points, count * sizeof(TM_Vec2));

    return vmap->count - 1;
}

int tm_vectormap_add_polygon(TM_VectorMap *vmap, const TM_Vec2 *points, int count, Color stroke, Color fill, bool filled) {
    if (!vmap || !points || count < 3 || !ensure_capacity(vmap)) return -1;

    TM_Feature *feat = &vmap->features[vmap->count++];
    feat->type = TM_FEATURE_POLY;
    feat->color = stroke;
    feat->fill_color = fill;
    feat->filled = filled;
    feat->radius = 0;
    feat->point_count = count;
    feat->points = (TM_Vec2*)malloc(count * sizeof(TM_Vec2));
    memcpy(feat->points, points, count * sizeof(TM_Vec2));

    return vmap->count - 1;
}

void tm_vectormap_render(renderContext *rc, const TM_VectorMap *vmap, const TM_Viewport *vp) {
    if (!rc || !vmap || !vmap->features) return;

    for (int i = 0; i < vmap->count; i++) {
        const TM_Feature *feat = &vmap->features[i];

        switch (feat->type) {
            case TM_FEATURE_POINT: {
                if (feat->point_count > 0) {
                    Point2 p = tm_world_to_screen(vp, feat->points[0]);
                    int r = (int)(feat->radius * (vp ? vp->zoom : 1.0f));
                    if (r < 1) r = 1;
                    renderCircle2D(rc, p, r, feat->color);
                }
                break;
            }
            case TM_FEATURE_LINE: {
                for (int j = 0; j < feat->point_count - 1; j++) {
                    Point2 p1 = tm_world_to_screen(vp, feat->points[j]);
                    Point2 p2 = tm_world_to_screen(vp, feat->points[j + 1]);
                    renderLine2D(rc, p1, p2, feat->color);
                }
                break;
            }
            case TM_FEATURE_POLY: {
                if (feat->point_count < 3) break;

                /* If filled, triangulate as fan */
                if (feat->filled) {
                    Point2 p0 = tm_world_to_screen(vp, feat->points[0]);
                    for (int j = 1; j < feat->point_count - 1; j++) {
                        Point2 p1 = tm_world_to_screen(vp, feat->points[j]);
                        Point2 p2 = tm_world_to_screen(vp, feat->points[j + 1]);
                        Point2 *tri[3] = { &p0, &p1, &p2 };
                        renderTriangle2D(rc, tri, feat->fill_color);
                    }
                }

                /* Render stroke / outline */
                for (int j = 0; j < feat->point_count; j++) {
                    Point2 p1 = tm_world_to_screen(vp, feat->points[j]);
                    Point2 p2 = tm_world_to_screen(vp, feat->points[(j + 1) % feat->point_count]);
                    renderLine2D(rc, p1, p2, feat->color);
                }
                break;
            }
        }
    }
}
