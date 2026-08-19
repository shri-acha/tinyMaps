#include "tinymap.h"
#include <stdlib.h>

TM_MapContext* tm_context_create(renderContext *rc, int screen_w, int screen_h) {
    TM_MapContext *ctx = (TM_MapContext*)malloc(sizeof(TM_MapContext));
    if (!ctx) return NULL;

    ctx->rc = rc;
    ctx->viewport = tm_viewport_create(screen_w, screen_h);
    ctx->tilemap = NULL;
    ctx->vectormap = NULL;
    ctx->heightmap = NULL;
    ctx->background_color = tm_color_rgb(18, 20, 26);
    ctx->show_grid = false;
    ctx->grid_spacing = 32;

    return ctx;
}

void tm_context_destroy(TM_MapContext *ctx) {
    if (!ctx) return;
    free(ctx);
}

void tm_context_set_tilemap(TM_MapContext *ctx, TM_TileMap *tilemap) {
    if (!ctx) return;
    ctx->tilemap = tilemap;
}

void tm_context_set_vectormap(TM_MapContext *ctx, TM_VectorMap *vectormap) {
    if (!ctx) return;
    ctx->vectormap = vectormap;
}

void tm_context_set_heightmap(TM_MapContext *ctx, TM_HeightMap *heightmap) {
    if (!ctx) return;
    ctx->heightmap = heightmap;
}

void tm_context_handle_event(TM_MapContext *ctx, Event e) {
    if (!ctx) return;
    tm_viewport_handle_event(&ctx->viewport, e);
}

static void render_background(renderContext *rc, Color bg) {
    if (!rc || !rc->frame_buffer) return;
    frameBuffer *fb = rc->frame_buffer;
    int total = fb->width * fb->height;
    for (int i = 0; i < total; i++) {
        fb->buffer[i].color = bg;
    }
}

static void render_grid(renderContext *rc, const TM_Viewport *vp, int spacing) {
    if (!rc || !vp || spacing <= 0) return;

    Point2 top_left = { 0, 0 };
    Point2 bot_right = { vp->screen_w, vp->screen_h };
    TM_Vec2 w_min = tm_screen_to_world(vp, top_left);
    TM_Vec2 w_max = tm_screen_to_world(vp, bot_right);

    float start_x = ((int)(w_min.x / spacing) - 1) * (float)spacing;
    float end_x   = ((int)(w_max.x / spacing) + 1) * (float)spacing;
    float start_y = ((int)(w_min.y / spacing) - 1) * (float)spacing;
    float end_y   = ((int)(w_max.y / spacing) + 1) * (float)spacing;

    Color grid_col = TM_COLOR_GRID;

    for (float x = start_x; x <= end_x; x += spacing) {
        Point2 p1 = tm_world_to_screen(vp, (TM_Vec2){ x, w_min.y });
        Point2 p2 = tm_world_to_screen(vp, (TM_Vec2){ x, w_max.y });
        renderLine2D(rc, p1, p2, grid_col);
    }
    for (float y = start_y; y <= end_y; y += spacing) {
        Point2 p1 = tm_world_to_screen(vp, (TM_Vec2){ w_min.x, y });
        Point2 p2 = tm_world_to_screen(vp, (TM_Vec2){ w_max.x, y });
        renderLine2D(rc, p1, p2, grid_col);
    }
}

void tm_context_render(TM_MapContext *ctx) {
    if (!ctx || !ctx->rc) return;

    /* 1. Clear background */
    render_background(ctx->rc, ctx->background_color);

    /* 2. Optional Grid */
    if (ctx->show_grid) {
        render_grid(ctx->rc, &ctx->viewport, ctx->grid_spacing);
    }

    /* 3. Render TileMap */
    if (ctx->tilemap) {
        tm_tilemap_render(ctx->rc, ctx->tilemap, &ctx->viewport);
    }

    /* 4. Render Vector Features */
    if (ctx->vectormap) {
        tm_vectormap_render(ctx->rc, ctx->vectormap, &ctx->viewport);
    }

    /* 5. Render 3D Heightmap if present */
    if (ctx->heightmap) {
        Point3 origin = { ctx->viewport.screen_w / 2, ctx->viewport.screen_h / 2, 0 };
        tm_heightmap_render_solid(ctx->rc, ctx->heightmap, origin, ctx->viewport.zoom);
    }
}
