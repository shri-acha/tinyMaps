#include "tinymap.h"

TM_Viewport tm_viewport_create(int screen_w, int screen_h) {
    TM_Viewport vp;
    vp.center.x = (float)screen_w / 2.0f;
    vp.center.y = (float)screen_h / 2.0f;
    vp.zoom = 1.0f;
    vp.screen_w = screen_w;
    vp.screen_h = screen_h;
    return vp;
}

void tm_viewport_pan(TM_Viewport *vp, float dx, float dy) {
    if (!vp) return;
    vp->center.x += dx / vp->zoom;
    vp->center.y += dy / vp->zoom;
}

void tm_viewport_zoom(TM_Viewport *vp, float factor) {
    if (!vp || factor <= 0.0f) return;
    vp->zoom *= factor;
    if (vp->zoom < 0.01f) vp->zoom = 0.01f;
    if (vp->zoom > 100.0f) vp->zoom = 100.0f;
}

void tm_viewport_center_on(TM_Viewport *vp, float x, float y) {
    if (!vp) return;
    vp->center.x = x;
    vp->center.y = y;
}

Point2 tm_world_to_screen(const TM_Viewport *vp, TM_Vec2 world_pos) {
    Point2 pt;
    if (!vp) {
        pt.x = (int)world_pos.x;
        pt.y = (int)world_pos.y;
        return pt;
    }
    float half_w = (float)vp->screen_w / 2.0f;
    float half_h = (float)vp->screen_h / 2.0f;
    
    pt.x = (int)((world_pos.x - vp->center.x) * vp->zoom + half_w);
    pt.y = (int)((world_pos.y - vp->center.y) * vp->zoom + half_h);
    return pt;
}

TM_Vec2 tm_screen_to_world(const TM_Viewport *vp, Point2 screen_pos) {
    TM_Vec2 wpos;
    if (!vp) {
        wpos.x = (float)screen_pos.x;
        wpos.y = (float)screen_pos.y;
        return wpos;
    }
    float half_w = (float)vp->screen_w / 2.0f;
    float half_h = (float)vp->screen_h / 2.0f;

    wpos.x = ((float)screen_pos.x - half_w) / vp->zoom + vp->center.x;
    wpos.y = ((float)screen_pos.y - half_h) / vp->zoom + vp->center.y;
    return wpos;
}
