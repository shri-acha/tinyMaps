#ifndef TINYMAP_H
#define TINYMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "graphics.h"
#include "types.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float x;
    float y;
} TM_Vec2;

typedef struct {
    float x;
    float y;
    float z;
} TM_Vec3;

static inline Color tm_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t r5 = (r >> 3) & 0x1F;
    uint16_t g6 = (g >> 2) & 0x3F;
    uint16_t b5 = (b >> 3) & 0x1F;
    Color c;
    c.literal = (r5 << 11) | (g6 << 5) | b5;
    return c;
}

#define TM_COLOR_WATER       tm_color_rgb(41, 128, 185)
#define TM_COLOR_DEEP_WATER  tm_color_rgb(26, 82, 118)
#define TM_COLOR_SAND        tm_color_rgb(244, 208, 63)
#define TM_COLOR_GRASS       tm_color_rgb(39, 174, 96)
#define TM_COLOR_FOREST      tm_color_rgb(30, 132, 73)
#define TM_COLOR_ROAD        tm_color_rgb(127, 140, 141)
#define TM_COLOR_HIGHWAY     tm_color_rgb(230, 126, 34)
#define TM_COLOR_BUILDING    tm_color_rgb(189, 195, 199)
#define TM_COLOR_MARKER      tm_color_rgb(231, 76, 60)
#define TM_COLOR_SNOW        tm_color_rgb(245, 245, 245)
#define TM_COLOR_ROCK        tm_color_rgb(108, 122, 137)
#define TM_COLOR_GRID        tm_color_rgb(60, 64, 75)


typedef struct {
    TM_Vec2 center;      /* Center coordinates in world space */
    float   zoom;        /* Zoom level / scale factor (1.0 = normal) */
    int     screen_w;    /* Screen / Framebuffer width */
    int     screen_h;    /* Screen / Framebuffer height */
} TM_Viewport;

TM_Viewport tm_viewport_create(int screen_w, int screen_h);
void        tm_viewport_pan(TM_Viewport *vp, float dx, float dy);
void        tm_viewport_zoom(TM_Viewport *vp, float factor);
void        tm_viewport_center_on(TM_Viewport *vp, float x, float y);
void        tm_viewport_handle_event(TM_Viewport *vp, Event e);

Point2  tm_world_to_screen(const TM_Viewport *vp, TM_Vec2 world_pos);
TM_Vec2 tm_screen_to_world(const TM_Viewport *vp, Point2 screen_pos);

/*
 * GLFW-compatible key tokens used by the event handler.  They are defined here
 * so tinyMap itself can stay independent of GLFW while still interpreting the
 * events emitted by tinyGraphics' GLFW backend.
 */
#define TM_KEY_LEFT   263
#define TM_KEY_RIGHT  262
#define TM_KEY_UP     265
#define TM_KEY_DOWN   264

typedef enum {
    TM_MAP_ORTHOGONAL = 0,
    TM_MAP_ISOMETRIC  = 1
} TM_MapProjection;

typedef struct {
    int              cols;
    int              rows;
    int              tile_size;       /* Pixel size per tile in world space */
    TM_MapProjection projection;
    int             *tiles;           /* Array of tile type IDs (cols * rows) */
    Color           *palette;         /* Array of colors mapped to tile IDs */
    int              palette_size;
} TM_TileMap;

TM_TileMap* tm_tilemap_create(int cols, int rows, int tile_size, TM_MapProjection proj);
void        tm_tilemap_destroy(TM_TileMap *map);
void        tm_tilemap_set_palette(TM_TileMap *map, const Color *palette, int size);
void        tm_tilemap_set_tile(TM_TileMap *map, int col, int row, int tile_id);
int         tm_tilemap_get_tile(const TM_TileMap *map, int col, int row);
void        tm_tilemap_fill(TM_TileMap *map, int tile_id);
void        tm_tilemap_render(renderContext *rc, const TM_TileMap *map, const TM_Viewport *vp);


typedef enum {
    TM_FEATURE_POINT = 0,
    TM_FEATURE_LINE  = 1,
    TM_FEATURE_POLY  = 2
} TM_FeatureType;

typedef struct {
    TM_FeatureType type;
    Color          color;
    Color          fill_color;
    bool           filled;
    int            point_count;
    TM_Vec2       *points;
    int            radius;       
} TM_Feature;

typedef struct {
    TM_Feature *features;
    int         count;
    int         capacity;
} TM_VectorMap;

TM_VectorMap* tm_vectormap_create(int initial_capacity);
void          tm_vectormap_destroy(TM_VectorMap *vmap);
int           tm_vectormap_add_marker(TM_VectorMap *vmap, TM_Vec2 pos, int radius, Color color);
int           tm_vectormap_add_line(TM_VectorMap *vmap, const TM_Vec2 *points, int count, Color color);
int           tm_vectormap_add_polygon(TM_VectorMap *vmap, const TM_Vec2 *points, int count, Color stroke, Color fill, bool filled);
void          tm_vectormap_render(renderContext *rc, const TM_VectorMap *vmap, const TM_Viewport *vp);

typedef struct {
    int    cols;
    int    rows;
    float  cell_size;
    float *heights;     
    float  min_height;
    float  max_height;
} TM_HeightMap;

TM_HeightMap* tm_heightmap_create(int cols, int rows, float cell_size);
void          tm_heightmap_destroy(TM_HeightMap *hm);
void          tm_heightmap_set(TM_HeightMap *hm, int col, int row, float height);
float         tm_heightmap_get(const TM_HeightMap *hm, int col, int row);
Color         tm_heightmap_get_elevation_color(float height, float min_h, float max_h);
void          tm_heightmap_render_wireframe(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale);
void          tm_heightmap_render_solid(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale);

typedef struct {
    renderContext *rc;
    TM_Viewport    viewport;
    TM_TileMap    *tilemap;
    TM_VectorMap  *vectormap;
    TM_HeightMap  *heightmap;
    Color          background_color;
    bool           show_grid;
    int            grid_spacing;
} TM_MapContext;

TM_MapContext* tm_context_create(renderContext *rc, int screen_w, int screen_h);
void           tm_context_destroy(TM_MapContext *ctx);
void           tm_context_set_tilemap(TM_MapContext *ctx, TM_TileMap *tilemap);
void           tm_context_set_vectormap(TM_MapContext *ctx, TM_VectorMap *vectormap);
void           tm_context_set_heightmap(TM_MapContext *ctx, TM_HeightMap *heightmap);
void           tm_context_handle_event(TM_MapContext *ctx, Event e);
void           tm_context_render(TM_MapContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TINYMAP_H */
