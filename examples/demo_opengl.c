#include "tinymap.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

static TM_MapContext *g_map_ctx = NULL;
static tinyWindow *g_window = NULL;

static void handle_event(Event e) {
    if (e.ev_typ == KEYBOARD && e.ke.state == DOWN && e.ke.keycode == 256 /* GLFW_KEY_ESCAPE */) {
        if (g_window) {
            glfwSetWindowShouldClose((GLFWwindow*)g_window->handle, GLFW_TRUE);
        }
        return;
    }

    tm_context_handle_event(g_map_ctx, e);
}

int main(void) {
    frameBuffer *fb = createFrameBuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!fb) {
        fprintf(stderr, "Failed to create frame buffer\n");
        return -1;
    }

    renderContext rc = {
        .frame_buffer = fb,
        .render_mode = FILLED,
        .origin = (Index){ .x = 0, .y = 0, .z = 0 },
        .scene_context = NULL,
        .projection = ORTHOGRAPHIC,
        .camera_position = (Point3){ .x = 0, .y = 0, .z = 0 }
    };

    registerEventHandler(&rc, handle_event);

    tinyWindow *window = tinyCreateWindow(tinyGetGLFWBackend(), WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "tinyMap - Web Raster Tile & Map Viewer", &rc, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        destroyFrameBuffer(fb);
        return -1;
    }

    /* Setup Map Context */
    TM_MapContext *ctx = tm_context_create(&rc, WINDOW_WIDTH, WINDOW_HEIGHT);
    g_map_ctx = ctx;
    g_window = window;

    /* Center camera on world map (or London / New York) */
    tm_viewport_center_on(&ctx->viewport, 256.0f, 256.0f);
    ctx->viewport.zoom = 1.0f;

    /* 1. Add Live Web Raster Tile Layer (Free CartoDB Voyager / OSM Tiles) */
    TM_WebTileLayer *webtiles = tm_webtile_layer_create(TM_TILE_CARTO_VOYAGER, ".cache/tinymap_tiles");
    tm_webtile_layer_set_zoom(webtiles, 1);
    tm_webtile_layer_set_opacity(webtiles, 1.0f);
    tm_context_set_webtile_layer(ctx, webtiles);

    /* 2. Add Vector Features Overlay (POI markers, routes) */
    TM_VectorMap *vmap = tm_vectormap_create(16);

    /* Route Polyline */
    TM_Vec2 route[5] = {
        { 120.0f, 150.0f },
        { 180.0f, 170.0f },
        { 260.0f, 220.0f },
        { 340.0f, 240.0f },
        { 410.0f, 310.0f }
    };
    tm_vectormap_add_line(vmap, route, 5, TM_COLOR_HIGHWAY);

    /* City POI Markers */
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 120.0f, 150.0f }, 6, TM_COLOR_MARKER);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 260.0f, 220.0f }, 6, TM_COLOR_MARKER);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 410.0f, 310.0f }, 6, TM_COLOR_MARKER);

    /* Landmark Zone Polygon */
    TM_Vec2 zone[4] = {
        { 240.0f, 180.0f },
        { 300.0f, 180.0f },
        { 300.0f, 230.0f },
        { 240.0f, 230.0f }
    };
    tm_vectormap_add_polygon(vmap, zone, 4, TM_COLOR_MARKER, TM_COLOR_BUILDING, false);

    tm_context_set_vectormap(ctx, vmap);

    printf("==================================================\n");
    printf("  tinyMap - Live Web Raster Tile & Map Viewer\n");
    printf("  Provider: CartoDB Voyager / OpenStreetMap\n");
    printf("  ------------------------------------------------\n");
    printf("  WASD / Arrow Keys : Pan Map\n");
    printf("  +/-               : Zoom In / Zoom Out\n");
    printf("  Left Click        : Center on Position\n");
    printf("  Right Click       : Zoom In\n");
    printf("  ESC               : Quit\n");
    printf("==================================================\n");

    /* Main Render Loop */
    while (!tinyWindowShouldClose(window)) {
        /* Render map layers (Web tiles -> Vectors) */
        tm_context_render(ctx);

        /* Present using tinyGraphics' windowing backend */
        tinyWindowPresent(window);
    }

    /* Cleanup */
    g_map_ctx = NULL;
    g_window = NULL;
    tm_webtile_layer_destroy(webtiles);
    tm_vectormap_destroy(vmap);
    tm_context_destroy(ctx);
    tinyDestroyWindow(window);
    destroyFrameBuffer(fb);

    return 0;
}
