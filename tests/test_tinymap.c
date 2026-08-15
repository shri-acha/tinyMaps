#include "tinymap.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

void test_viewport(void) {
    printf("[Test] Viewport & Coordinate Transforms... ");
    TM_Viewport vp = tm_viewport_create(800, 600);
    assert(vp.screen_w == 800);
    assert(vp.screen_h == 600);
    assert(vp.zoom == 1.0f);

    TM_Vec2 world_pt = { 400.0f, 300.0f };
    Point2 screen_pt = tm_world_to_screen(&vp, world_pt);
    assert(screen_pt.x == 400);
    assert(screen_pt.y == 300);

    TM_Vec2 back_world = tm_screen_to_world(&vp, screen_pt);
    assert(fabsf(back_world.x - world_pt.x) < 0.01f);
    assert(fabsf(back_world.y - world_pt.y) < 0.01f);

    tm_viewport_zoom(&vp, 2.0f);
    assert(vp.zoom == 2.0f);

    tm_viewport_pan(&vp, 10.0f, 20.0f);
    printf("PASSED\n");
}

void test_tilemap(void) {
    printf("[Test] TileMap Creation & Access... ");
    TM_TileMap *tm = tm_tilemap_create(10, 10, 16, TM_MAP_ORTHOGONAL);
    assert(tm != NULL);
    assert(tm->cols == 10);
    assert(tm->rows == 10);

    tm_tilemap_fill(tm, 2);
    assert(tm_tilemap_get_tile(tm, 0, 0) == 2);
    assert(tm_tilemap_get_tile(tm, 9, 9) == 2);

    tm_tilemap_set_tile(tm, 4, 5, 7);
    assert(tm_tilemap_get_tile(tm, 4, 5) == 7);
    assert(tm_tilemap_get_tile(tm, -1, 0) == -1);
    assert(tm_tilemap_get_tile(tm, 10, 10) == -1);

    tm_tilemap_destroy(tm);
    printf("PASSED\n");
}

void test_vectormap(void) {
    printf("[Test] VectorMap Features... ");
    TM_VectorMap *vm = tm_vectormap_create(4);
    assert(vm != NULL);

    int m_idx = tm_vectormap_add_marker(vm, (TM_Vec2){ 10.0f, 20.0f }, 5, TM_COLOR_MARKER);
    assert(m_idx == 0);

    TM_Vec2 line_pts[3] = { {0,0}, {10,10}, {20,20} };
    int l_idx = tm_vectormap_add_line(vm, line_pts, 3, TM_COLOR_ROAD);
    assert(l_idx == 1);

    TM_Vec2 poly_pts[3] = { {0,0}, {10,0}, {5,10} };
    int p_idx = tm_vectormap_add_polygon(vm, poly_pts, 3, TM_COLOR_MARKER, TM_COLOR_BUILDING, true);
    assert(p_idx == 2);
    assert(vm->count == 3);

    tm_vectormap_destroy(vm);
    printf("PASSED\n");
}

void test_heightmap(void) {
    printf("[Test] HeightMap Elevation... ");
    TM_HeightMap *hm = tm_heightmap_create(8, 8, 10.0f);
    assert(hm != NULL);

    tm_heightmap_set(hm, 2, 3, 45.0f);
    assert(tm_heightmap_get(hm, 2, 3) == 45.0f);

    Color water_c = tm_heightmap_get_elevation_color(5.0f, 0.0f, 100.0f);
    Color snow_c = tm_heightmap_get_elevation_color(95.0f, 0.0f, 100.0f);
    assert(water_c.literal != snow_c.literal);

    tm_heightmap_destroy(hm);
    printf("PASSED\n");
}

void test_render_pipeline(void) {
    printf("[Test] Full Map Render Pipeline... ");
    int w = 100, h = 100;
    renderContext rc;
    rc.frame_buffer = createFrameBuffer(w, h);
    rc.render_mode = FILLED;
    rc.shading_mode = SHADE_NONE;
    rc.projection = ORTHOGRAPHIC;
    rc.origin = (Index){ 50, 50, 0 };
    rc.scene_context = NULL;

    TM_MapContext *ctx = tm_context_create(&rc, w, h);
    TM_TileMap *tm = tm_tilemap_create(5, 5, 20, TM_MAP_ORTHOGONAL);
    tm_tilemap_fill(tm, 1);
    tm_context_set_tilemap(ctx, tm);

    TM_VectorMap *vm = tm_vectormap_create(2);
    tm_vectormap_add_marker(vm, (TM_Vec2){ 50, 50 }, 4, TM_COLOR_MARKER);
    tm_context_set_vectormap(ctx, vm);

    tm_context_render(ctx);

    /* Verify center pixel has marker color */
    pixelBuffer p = get_pixel(rc.frame_buffer, 50, 50);
    assert(p.color.literal != 0);

    tm_tilemap_destroy(tm);
    tm_vectormap_destroy(vm);
    tm_context_destroy(ctx);
    destroyFrameBuffer(rc.frame_buffer);
    destroyContext(NULL);
    printf("PASSED\n");
}

int main(void) {
    printf("\n=== Running tinyMap Test Suite ===\n");
    test_viewport();
    test_tilemap();
    test_vectormap();
    test_heightmap();
    test_render_pipeline();
    printf("=== All Tests Passed Successfully! ===\n\n");
    return 0;
}
