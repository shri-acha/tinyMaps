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

void test_viewport_events(void) {
    printf("[Test] Event-driven Viewport Navigation... ");
    TM_Viewport vp = tm_viewport_create(800, 600);
    float start_center_x = vp.center.x;
    float start_center_y = vp.center.y;

    Event key_ev;
    key_ev.ev_typ = KEYBOARD;
    key_ev.ke.keycode = TM_KEY_RIGHT;
    key_ev.ke.state = DOWN;
    tm_viewport_handle_event(&vp, key_ev);
    assert(vp.center.x > start_center_x);

    key_ev.ke.keycode = TM_KEY_UP;
    tm_viewport_handle_event(&vp, key_ev);
    assert(vp.center.y < start_center_y);

    float zoom_before = vp.zoom;
    key_ev.ke.keycode = '+';
    tm_viewport_handle_event(&vp, key_ev);
    assert(vp.zoom > zoom_before);

    TM_Viewport vp_before_mouse = vp;
    Event mouse_ev;
    mouse_ev.ev_typ = MOUSE;
    mouse_ev.me.btn = LEFT;
    mouse_ev.me.state = DOWN;
    mouse_ev.me.pos = (Point2){ .x = 100, .y = 120 };

    TM_Vec2 expected = tm_screen_to_world(&vp_before_mouse, mouse_ev.me.pos);
    tm_viewport_handle_event(&vp, mouse_ev);

    assert(fabsf(vp.center.x - expected.x) < 0.01f);
    assert(fabsf(vp.center.y - expected.y) < 0.01f);

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
    printf("PASSED\n");
}

void test_raster_basic(void) {
    printf("[Test] RasterMap Creation & Manipulation... ");
    TM_Bounds bounds = { 0.0f, 0.0f, 100.0f, 100.0f };
    TM_RasterMap *raster = tm_raster_create(10, 10, bounds);
    assert(raster != NULL);
    assert(raster->width == 10);
    assert(raster->height == 10);
    assert(raster->opacity == 1.0f);

    tm_raster_fill(raster, TM_COLOR_GRASS);
    assert(tm_raster_get_pixel(raster, 0, 0).literal == TM_COLOR_GRASS.literal);
    assert(tm_raster_get_pixel(raster, 9, 9).literal == TM_COLOR_GRASS.literal);

    tm_raster_set_pixel(raster, 4, 4, TM_COLOR_WATER);
    assert(tm_raster_get_pixel(raster, 4, 4).literal == TM_COLOR_WATER.literal);

    tm_raster_set_opacity(raster, 0.5f);
    assert(fabsf(raster->opacity - 0.5f) < 0.001f);

    tm_raster_destroy(raster);
    printf("PASSED\n");
}

void test_raster_ppm_io(void) {
    printf("[Test] Raster PPM (P6/P3) File I/O... ");
    TM_Bounds bounds = { 0.0f, 0.0f, 20.0f, 20.0f };
    TM_RasterMap *orig = tm_raster_create(4, 4, bounds);
    assert(orig != NULL);

    tm_raster_set_pixel(orig, 0, 0, tm_color_rgb(255, 0, 0));
    tm_raster_set_pixel(orig, 1, 0, tm_color_rgb(0, 255, 0));
    tm_raster_set_pixel(orig, 2, 0, tm_color_rgb(0, 0, 255));
    tm_raster_set_pixel(orig, 3, 0, tm_color_rgb(255, 255, 255));

    const char *tmp_ppm = "/tmp/test_tinymap.ppm";
    int save_res = tm_raster_save_ppm(orig, tmp_ppm);
    assert(save_res == 0);

    TM_RasterMap *loaded = tm_raster_load_ppm(tmp_ppm, bounds);
    assert(loaded != NULL);
    assert(loaded->width == 4);
    assert(loaded->height == 4);

    Color c0 = tm_raster_get_pixel(loaded, 0, 0);
    Color c1 = tm_raster_get_pixel(loaded, 1, 0);
    Color c2 = tm_raster_get_pixel(loaded, 2, 0);
    assert(c0.literal == orig->pixels[0].literal);
    assert(c1.literal == orig->pixels[1].literal);
    assert(c2.literal == orig->pixels[2].literal);

    tm_raster_destroy(orig);
    tm_raster_destroy(loaded);
    remove(tmp_ppm);
    printf("PASSED\n");
}

void test_raster_bmp_io(void) {
    printf("[Test] Raster BMP File I/O... ");
    TM_Bounds bounds = { 10.0f, 20.0f, 110.0f, 120.0f };
    TM_RasterMap *orig = tm_raster_create(8, 8, bounds);
    assert(orig != NULL);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            tm_raster_set_pixel(orig, x, y, tm_color_rgb((uint8_t)(x * 30), (uint8_t)(y * 30), 128));
        }
    }

    const char *tmp_bmp = "/tmp/test_tinymap.bmp";
    int save_res = tm_raster_save_bmp(orig, tmp_bmp);
    assert(save_res == 0);

    TM_RasterMap *loaded = tm_raster_load_bmp(tmp_bmp, bounds);
    assert(loaded != NULL);
    assert(loaded->width == 8);
    assert(loaded->height == 8);

    Color c_orig = tm_raster_get_pixel(orig, 3, 5);
    Color c_load = tm_raster_get_pixel(loaded, 3, 5);
    assert(c_orig.literal == c_load.literal);

    tm_raster_destroy(orig);
    tm_raster_destroy(loaded);
    remove(tmp_bmp);
    printf("PASSED\n");
}

void test_raster_asc_io(void) {
    printf("[Test] Raster ESRI ASC / DEM Grid I/O... ");
    const char *tmp_asc = "/tmp/test_tinymap.asc";
    FILE *f = fopen(tmp_asc, "w");
    assert(f != NULL);

    fprintf(f, "ncols         4\n");
    fprintf(f, "nrows         3\n");
    fprintf(f, "xllcorner     100.0\n");
    fprintf(f, "yllcorner     200.0\n");
    fprintf(f, "cellsize      10.0\n");
    fprintf(f, "NODATA_value  -9999\n");
    fprintf(f, "10.0 20.0 30.0 40.0\n");
    fprintf(f, "50.0 60.0 70.0 80.0\n");
    fprintf(f, "90.0 100.0 -9999 120.0\n");
    fclose(f);

    TM_RasterMap *raster = tm_raster_load_asc(tmp_asc, 0.0f, 150.0f);
    assert(raster != NULL);
    assert(raster->width == 4);
    assert(raster->height == 3);
    assert(fabsf(raster->bounds.min_x - 100.0f) < 0.01f);
    assert(fabsf(raster->bounds.max_x - 140.0f) < 0.01f);
    assert(fabsf(raster->bounds.min_y - 200.0f) < 0.01f);
    assert(fabsf(raster->bounds.max_y - 230.0f) < 0.01f);

    TM_HeightMap *hm = tm_heightmap_from_asc(tmp_asc);
    assert(hm != NULL);
    assert(hm->cols == 4);
    assert(hm->rows == 3);
    assert(fabsf(tm_heightmap_get(hm, 1, 1) - 60.0f) < 0.01f);

    tm_raster_destroy(raster);
    tm_heightmap_destroy(hm);
    remove(tmp_asc);
    printf("PASSED\n");
}

void test_raster_render(void) {
    printf("[Test] Raster Map Rendering... ");
    int w = 60, h = 60;
    renderContext rc;
    rc.frame_buffer = createFrameBuffer(w, h);
    rc.render_mode = FILLED;
    rc.shading_mode = SHADE_NONE;
    rc.projection = ORTHOGRAPHIC;
    rc.origin = (Index){ 0, 0, 0 };
    rc.scene_context = NULL;

    TM_Viewport vp = tm_viewport_create(w, h);
    TM_Bounds bounds = { 10.0f, 10.0f, 50.0f, 50.0f };
    TM_RasterMap *raster = tm_raster_create(4, 4, bounds);
    tm_raster_fill(raster, tm_color_rgb(200, 50, 100));

    tm_raster_render(&rc, raster, &vp);

    Point2 center_pt = { 30, 30 };
    pixelBuffer p = get_pixel(rc.frame_buffer, center_pt.x, center_pt.y);
    assert(p.color.literal != 0);

    tm_raster_destroy(raster);
    destroyFrameBuffer(rc.frame_buffer);
    printf("PASSED\n");
}

void test_geo_coords(void) {
    printf("[Test] Geographic & Tile Coordinate Math... ");
    TM_GeoCoord london = { 51.5074, -0.1278 };
    int zoom = 10;
    TM_Vec2 world_pos = tm_geo_to_world(london, zoom);
    assert(world_pos.x > 0.0f);
    assert(world_pos.y > 0.0f);

    TM_GeoCoord back_geo = tm_world_to_geo(world_pos, zoom);
    assert(fabs(back_geo.lat - london.lat) < 0.01);
    assert(fabs(back_geo.lon - london.lon) < 0.01);

    TM_Bounds b0 = tm_tile_bounds(0, 0, 0);
    assert(b0.min_x == 0.0f);
    assert(b0.max_x == 256.0f);
    assert(b0.min_y == 0.0f);
    assert(b0.max_y == 256.0f);

    TM_Bounds b1 = tm_tile_bounds(1, 0, 0);
    assert(b1.min_x == 0.0f);
    assert(b1.max_x == 128.0f);
    assert(b1.min_y == 0.0f);
    assert(b1.max_y == 128.0f);

    printf("PASSED\n");
}

void test_webtile_fetch_and_layer(void) {
    printf("[Test] Web Raster Tile Layer & Caching... ");
    const char *test_cache = "/tmp/tinymap_test_tiles";
    TM_WebTileLayer *layer = tm_webtile_layer_create(TM_TILE_CARTO_VOYAGER, test_cache);
    assert(layer != NULL);

    tm_webtile_layer_set_zoom(layer, 0);
    tm_webtile_layer_set_opacity(layer, 0.9f);

    /* Fetch world tile (0, 0, 0) */
    TM_RasterMap *tile = tm_webtile_fetch(TM_TILE_CARTO_VOYAGER, 0, 0, 0, test_cache);
    if (tile) {
        assert(tile->width == 256);
        assert(tile->height == 256);
        tm_raster_destroy(tile);
    }

    /* Test layer render into framebuffer */
    int w = 256, h = 256;
    renderContext rc;
    rc.frame_buffer = createFrameBuffer(w, h);
    rc.render_mode = FILLED;
    rc.shading_mode = SHADE_NONE;
    rc.projection = ORTHOGRAPHIC;
    rc.origin = (Index){ 0, 0, 0 };
    rc.scene_context = NULL;

    TM_Viewport vp = tm_viewport_create(w, h);
    tm_viewport_center_on(&vp, 128.0f, 128.0f);

    tm_webtile_layer_render(&rc, layer, &vp);

    tm_webtile_layer_destroy(layer);
    destroyFrameBuffer(rc.frame_buffer);

    printf("PASSED\n");
}

static void add_cached_tile_for_test(TM_WebTileLayer *layer, int z, int x, int y, Color color) {
    TM_Bounds bounds = tm_tile_bounds(z, x, y);
    TM_RasterMap *raster = tm_raster_create(4, 4, bounds);
    assert(raster != NULL);
    tm_raster_fill(raster, color);

    TM_CachedTile *entry = (TM_CachedTile*)calloc(1, sizeof(TM_CachedTile));
    assert(entry != NULL);
    entry->z = z;
    entry->x = x;
    entry->y = y;
    entry->raster = raster;
    entry->next = layer->tile_cache;
    layer->tile_cache = entry;
    layer->cache_count++;
}

static void clear_frame_buffer(frameBuffer *fb) {
    for (int i = 0; i < fb->width * fb->height; i++) {
        fb->buffer[i].color.literal = 0;
    }
}

static void assert_pixel_is_red(frameBuffer *fb, int x, int y) {
    pixelBuffer p = get_pixel(fb, x, y);
    uint8_t r = 0, g = 0, b = 0;
    tm_color_to_rgb(p.color, &r, &g, &b);
    assert(r > 200);
    assert(g < 80);
    assert(b < 80);
}

static void assert_pixel_is_green(frameBuffer *fb, int x, int y) {
    pixelBuffer p = get_pixel(fb, x, y);
    uint8_t r = 0, g = 0, b = 0;
    tm_color_to_rgb(p.color, &r, &g, &b);
    assert(r < 80);
    assert(g > 200);
    assert(b < 80);
}

static void assert_pixel_is_black(frameBuffer *fb, int x, int y) {
    pixelBuffer p = get_pixel(fb, x, y);
    assert(p.color.literal == 0);
}

void test_webtile_auto_depth_and_wrap(void) {
    printf("[Test] Web Tile Auto Depth & Infinite Horizontal Wrapping... ");

    TM_WebTileLayer *layer = tm_webtile_layer_create(TM_TILE_CARTO_VOYAGER, "/tmp/tinymap_test_tiles_depth");
    assert(layer != NULL);
    assert(layer->fixed_zoom == -1);
    assert(layer->wrap_x == true);

    int w = 64, h = 64;
    renderContext rc;
    rc.frame_buffer = createFrameBuffer(w, h);
    rc.render_mode = FILLED;
    rc.shading_mode = SHADE_NONE;
    rc.projection = ORTHOGRAPHIC;
    rc.origin = (Index){ 0, 0, 0 };
    rc.scene_context = NULL;

    TM_Viewport vp = tm_viewport_create(w, h);

    /* Seed only the tiles needed by the renderer; no network is involved. */
    add_cached_tile_for_test(layer, 0, 0, 0, tm_color_rgb(255, 0, 0));
    add_cached_tile_for_test(layer, 1, 0, 0, tm_color_rgb(0, 255, 0));

    /* Viewport zoom 1 selects depth 0. */
    tm_viewport_center_on(&vp, 64.0f, 64.0f);
    vp.zoom = 1.0f;
    clear_frame_buffer(rc.frame_buffer);
    tm_webtile_layer_render(&rc, layer, &vp);
    assert_pixel_is_red(rc.frame_buffer, 32, 32);

    /* Viewport zoom 2 selects depth 1 and re-renders using the next level. */
    vp.zoom = 2.0f;
    clear_frame_buffer(rc.frame_buffer);
    tm_webtile_layer_render(&rc, layer, &vp);
    assert_pixel_is_green(rc.frame_buffer, 32, 32);

    /* Pan east past the world edge: the single depth-0 tile repeats. */
    vp.zoom = 1.0f;
    tm_viewport_center_on(&vp, 320.0f, 64.0f);
    clear_frame_buffer(rc.frame_buffer);
    tm_webtile_layer_render(&rc, layer, &vp);
    assert_pixel_is_red(rc.frame_buffer, 32, 32);

    /* Disabling horizontal wrapping restores the original edge-clamped view. */
    tm_webtile_layer_set_wrap_x(layer, false);
    assert(layer->wrap_x == false);
    clear_frame_buffer(rc.frame_buffer);
    tm_webtile_layer_render(&rc, layer, &vp);
    assert_pixel_is_black(rc.frame_buffer, 32, 32);

    tm_webtile_layer_destroy(layer);
    destroyFrameBuffer(rc.frame_buffer);
    printf("PASSED\n");
}

int main(void) {
    printf("\n=== Running tinyMap Test Suite ===\n");
    test_viewport();
    test_viewport_events();
    test_tilemap();
    test_vectormap();
    test_heightmap();
    test_render_pipeline();
    test_raster_basic();
    test_raster_ppm_io();
    test_raster_bmp_io();
    test_raster_asc_io();
    test_raster_render();
    test_geo_coords();
    test_webtile_fetch_and_layer();
    test_webtile_auto_depth_and_wrap();
    printf("=== All Tests Passed Successfully! ===\n\n");
    return 0;
}
