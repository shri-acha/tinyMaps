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

typedef struct {
    float min_x;
    float min_y;
    float max_x;
    float max_y;
} TM_Bounds;

typedef struct {
    double lat;
    double lon;
} TM_GeoCoord;

static inline Color tm_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t r5 = (r >> 3) & 0x1F;
    uint16_t g6 = (g >> 2) & 0x3F;
    uint16_t b5 = (b >> 3) & 0x1F;
    Color c;
    c.literal = (r5 << 11) | (g6 << 5) | b5;
    return c;
}

static inline void tm_color_to_rgb(Color c, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint16_t r5 = (c.literal >> 11) & 0x1F;
    uint16_t g6 = (c.literal >> 5) & 0x3F;
    uint16_t b5 = c.literal & 0x1F;
    if (r) *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    if (g) *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    if (b) *b = (uint8_t)((b5 << 3) | (b5 >> 2));
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

TM_Vec2 tm_geo_to_world(TM_GeoCoord geo, int zoom);
TM_GeoCoord tm_world_to_geo(TM_Vec2 world, int zoom);
TM_Bounds tm_tile_bounds(int z, int x, int y);

typedef struct {
    TM_Vec2 center;
    float   zoom;
    int     screen_w;
    int     screen_h;
} TM_Viewport;

TM_Viewport tm_viewport_create(int screen_w, int screen_h);
void        tm_viewport_pan(TM_Viewport *vp, float dx, float dy);
void        tm_viewport_zoom(TM_Viewport *vp, float factor);
void        tm_viewport_center_on(TM_Viewport *vp, float x, float y);
void        tm_viewport_handle_event(TM_Viewport *vp, Event e);

Point2  tm_world_to_screen(const TM_Viewport *vp, TM_Vec2 world_pos);
TM_Vec2 tm_screen_to_world(const TM_Viewport *vp, Point2 screen_pos);

#define TM_KEY_LEFT   263
#define TM_KEY_RIGHT  262
#define TM_KEY_UP     265
#define TM_KEY_DOWN   264

typedef struct {
    int       width;
    int       height;
    Color    *pixels;
    TM_Bounds bounds;
    float     opacity;
} TM_RasterMap;

TM_RasterMap* tm_raster_create(int width, int height, TM_Bounds bounds);
void          tm_raster_destroy(TM_RasterMap *raster);
void          tm_raster_set_pixel(TM_RasterMap *raster, int x, int y, Color color);
Color         tm_raster_get_pixel(const TM_RasterMap *raster, int x, int y);
void          tm_raster_set_opacity(TM_RasterMap *raster, float opacity);
void          tm_raster_set_bounds(TM_RasterMap *raster, TM_Bounds bounds);
void          tm_raster_fill(TM_RasterMap *raster, Color color);

TM_RasterMap* tm_raster_load_image(const char *filepath, TM_Bounds bounds);
TM_RasterMap* tm_raster_load_image_mem(const unsigned char *buffer, int len, TM_Bounds bounds);
TM_RasterMap* tm_raster_load_ppm(const char *filepath, TM_Bounds bounds);
int           tm_raster_save_ppm(const TM_RasterMap *raster, const char *filepath);
TM_RasterMap* tm_raster_load_bmp(const char *filepath, TM_Bounds bounds);
int           tm_raster_save_bmp(const TM_RasterMap *raster, const char *filepath);
TM_RasterMap* tm_raster_load_asc(const char *filepath, float min_elev, float max_elev);

void          tm_raster_render(renderContext *rc, const TM_RasterMap *raster, const TM_Viewport *vp);

#define TM_TILE_OSM          "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
#define TM_TILE_CARTO_VOYAGER "https://a.basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png"
#define TM_TILE_CARTO_POSITRON "https://a.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png"
#define TM_TILE_CARTO_DARK   "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png"
#define TM_TILE_OPENTOPOMAP  "https://tile.opentopomap.org/{z}/{x}/{y}.png"

typedef struct TM_CachedTile {
    int               z, x, y;
    TM_RasterMap     *raster;
    struct TM_CachedTile *next;
} TM_CachedTile;

typedef struct {
    char          url_template[256];
    char          cache_dir[256];
    int           fixed_zoom;
    float         opacity;
    TM_CachedTile *tile_cache;
    int           cache_count;
    int           max_cache_size;
} TM_WebTileLayer;

TM_RasterMap*    tm_webtile_fetch(const char *url_template, int z, int x, int y, const char *cache_dir);
TM_WebTileLayer* tm_webtile_layer_create(const char *url_template, const char *cache_dir);
void             tm_webtile_layer_destroy(TM_WebTileLayer *layer);
void             tm_webtile_layer_set_zoom(TM_WebTileLayer *layer, int zoom);
void             tm_webtile_layer_set_opacity(TM_WebTileLayer *layer, float opacity);
void             tm_webtile_layer_render(renderContext *rc, TM_WebTileLayer *layer, const TM_Viewport *vp);

typedef enum {
    TM_MAP_ORTHOGONAL = 0,
    TM_MAP_ISOMETRIC  = 1
} TM_MapProjection;

typedef struct {
    int              cols;
    int              rows;
    int              tile_size;
    TM_MapProjection projection;
    int             *tiles;
    Color           *palette;
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
TM_HeightMap* tm_heightmap_from_asc(const char *filepath);
void          tm_heightmap_render_wireframe(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale);
void          tm_heightmap_render_solid(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale);

typedef struct {
    renderContext   *rc;
    TM_Viewport      viewport;
    TM_WebTileLayer *webtile_layer;
    TM_RasterMap    *rastermap;
    TM_TileMap      *tilemap;
    TM_VectorMap    *vectormap;
    TM_HeightMap    *heightmap;
    Color            background_color;
    bool             show_grid;
    int              grid_spacing;
} TM_MapContext;

TM_MapContext* tm_context_create(renderContext *rc, int screen_w, int screen_h);
void           tm_context_destroy(TM_MapContext *ctx);
void           tm_context_set_webtile_layer(TM_MapContext *ctx, TM_WebTileLayer *layer);
void           tm_context_set_rastermap(TM_MapContext *ctx, TM_RasterMap *rastermap);
void           tm_context_set_tilemap(TM_MapContext *ctx, TM_TileMap *tilemap);
void           tm_context_set_vectormap(TM_MapContext *ctx, TM_VectorMap *vectormap);
void           tm_context_set_heightmap(TM_MapContext *ctx, TM_HeightMap *heightmap);
void           tm_context_handle_event(TM_MapContext *ctx, Event e);
void           tm_context_render(TM_MapContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TINYMAP_H */
