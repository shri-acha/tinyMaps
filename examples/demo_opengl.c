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
                                          "tinyMap - OpenGL Viewer", &rc, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        destroyFrameBuffer(fb);
        return -1;
    }

    /* Setup Map Context */
    TM_MapContext *ctx = tm_context_create(&rc, WINDOW_WIDTH, WINDOW_HEIGHT);
    g_map_ctx = ctx;
    g_window = window;

    /* 1. Create TileMap (40x30 tiles, 20px each) */
    TM_TileMap *tilemap = tm_tilemap_create(40, 30, 20, TM_MAP_ORTHOGONAL);
    tm_tilemap_fill(tilemap, 3); /* Grass */

    /* Add a winding river and sand shores */
    for (int r = 0; r < 30; r++) {
        int c = 16 + (int)(sinf(r * 0.3f) * 5.0f);
        tm_tilemap_set_tile(tilemap, c - 1, r, 2); /* Sand */
        tm_tilemap_set_tile(tilemap, c,     r, 1); /* Water */
        tm_tilemap_set_tile(tilemap, c + 1, r, 1); /* Water */
        tm_tilemap_set_tile(tilemap, c + 2, r, 2); /* Sand */
    }

    /* Add paved roads */
    for (int c = 0; c < 40; c++) {
        tm_tilemap_set_tile(tilemap, c, 14, 5); /* Road */
    }
    for (int r = 0; r < 30; r++) {
        tm_tilemap_set_tile(tilemap, 28, r, 5); /* Cross road */
    }

    tm_context_set_tilemap(ctx, tilemap);

    /* 2. Add Vector Features (Highway, Markers, Buildings) */
    TM_VectorMap *vmap = tm_vectormap_create(16);

    /* Highway polyline */
    TM_Vec2 highway[6] = {
        { 40.0f, 50.0f },
        { 180.0f, 120.0f },
        { 380.0f, 150.0f },
        { 550.0f, 320.0f },
        { 700.0f, 450.0f },
        { 760.0f, 550.0f }
    };
    tm_vectormap_add_line(vmap, highway, 6, TM_COLOR_HIGHWAY);

    /* City POI Markers */
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 180.0f, 120.0f }, 5, TM_COLOR_MARKER);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 550.0f, 320.0f }, 5, TM_COLOR_MARKER);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 560.0f, 280.0f }, 4, TM_COLOR_MARKER);

    /* Building Polygons */
    TM_Vec2 bldg1[4] = {
        { 600.0f, 180.0f },
        { 680.0f, 180.0f },
        { 680.0f, 240.0f },
        { 600.0f, 240.0f }
    };
    tm_vectormap_add_polygon(vmap, bldg1, 4, TM_COLOR_MARKER, TM_COLOR_BUILDING, true);

    TM_Vec2 bldg2[4] = {
        { 620.0f, 80.0f },
        { 720.0f, 80.0f },
        { 720.0f, 140.0f },
        { 620.0f, 140.0f }
    };
    tm_vectormap_add_polygon(vmap, bldg2, 4, TM_COLOR_MARKER, TM_COLOR_BUILDING, true);

    tm_context_set_vectormap(ctx, vmap);

    printf("=========================================\n");
    printf("  tinyMap OpenGL Viewer\n");
    printf("  WASD / Arrow Keys : Pan\n");
    printf("  +/-               : Zoom\n");
    printf("  Left Click        : Center on position\n");
    printf("  Right Click       : Zoom in\n");
    printf("  ESC               : Quit\n");
    printf("=========================================\n");

    /* Main Render Loop */
    while (!tinyWindowShouldClose(window)) {
        /* Render map to frameBuffer */
        tm_context_render(ctx);

        /* Present using tinyGraphics' updated windowing backend. */
        tinyWindowPresent(window);
    }

    /* Cleanup */
    g_map_ctx = NULL;
    g_window = NULL;
    tm_tilemap_destroy(tilemap);
    tm_vectormap_destroy(vmap);
    tm_context_destroy(ctx);
    tinyDestroyWindow(window);
    destroyFrameBuffer(fb);

    return 0;
}
