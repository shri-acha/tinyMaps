#include "tinymap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#define DEFAULT_CACHE_DIR ".cache/tinymap_tiles"
#define MAX_CACHE_ENTRIES 64

/* Helper to replace {z}, {x}, {y} in URL template */
static void format_tile_url(char *out, size_t out_size, const char *tmpl, int z, int x, int y) {
    char z_str[16], x_str[16], y_str[16];
    snprintf(z_str, sizeof(z_str), "%d", z);
    snprintf(x_str, sizeof(x_str), "%d", x);
    snprintf(y_str, sizeof(y_str), "%d", y);

    out[0] = '\0';
    const char *p = tmpl;
    char *dest = out;
    size_t remaining = out_size - 1;

    while (*p && remaining > 0) {
        if (*p == '{') {
            if (strncmp(p, "{z}", 3) == 0) {
                size_t len = strlen(z_str);
                if (len <= remaining) {
                    memcpy(dest, z_str, len);
                    dest += len;
                    remaining -= len;
                }
                p += 3;
                continue;
            } else if (strncmp(p, "{x}", 3) == 0) {
                size_t len = strlen(x_str);
                if (len <= remaining) {
                    memcpy(dest, x_str, len);
                    dest += len;
                    remaining -= len;
                }
                p += 3;
                continue;
            } else if (strncmp(p, "{y}", 3) == 0) {
                size_t len = strlen(y_str);
                if (len <= remaining) {
                    memcpy(dest, y_str, len);
                    dest += len;
                    remaining -= len;
                }
                p += 3;
                continue;
            }
        }
        *dest++ = *p++;
        remaining--;
    }
    *dest = '\0';
}

static void ensure_dir_exists(const char *dir_path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "mkdir -p \"%s\" 2>/dev/null", dir_path);
    (void)system(tmp);
}

TM_RasterMap* tm_webtile_fetch(const char *url_template, int z, int x, int y, const char *cache_dir) {
    if (!url_template) return NULL;
    const char *cdir = (cache_dir && cache_dir[0]) ? cache_dir : DEFAULT_CACHE_DIR;

    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%d/%d", cdir, z, x);
    ensure_dir_exists(dir_path);

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%d/%d/%d.png", cdir, z, x, y);

    TM_Bounds bounds = tm_tile_bounds(z, x, y);

    /* Check if already cached on disk */
    if (access(file_path, F_OK) == 0) {
        TM_RasterMap *r = tm_raster_load_image(file_path, bounds);
        if (r) return r;
    }

    /* Format HTTP URL */
    char url[512];
    format_tile_url(url, sizeof(url), url_template, z, x, y);

    /* Download tile using curl */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s -f -m 5 -A \"tinyMap/1.0\" \"%s\" -o \"%s\"", url, file_path);
    int ret = system(cmd);
    if (ret != 0 || access(file_path, F_OK) != 0) {
        /* Failed or network unavailable */
        return NULL;
    }

    return tm_raster_load_image(file_path, bounds);
}

TM_WebTileLayer* tm_webtile_layer_create(const char *url_template, const char *cache_dir) {
    TM_WebTileLayer *layer = (TM_WebTileLayer*)malloc(sizeof(TM_WebTileLayer));
    if (!layer) return NULL;

    strncpy(layer->url_template, url_template ? url_template : TM_TILE_CARTO_VOYAGER, sizeof(layer->url_template) - 1);
    layer->url_template[sizeof(layer->url_template) - 1] = '\0';

    strncpy(layer->cache_dir, cache_dir ? cache_dir : DEFAULT_CACHE_DIR, sizeof(layer->cache_dir) - 1);
    layer->cache_dir[sizeof(layer->cache_dir) - 1] = '\0';

    layer->fixed_zoom = -1; /* Auto zoom */
    layer->opacity = 1.0f;
    layer->tile_cache = NULL;
    layer->cache_count = 0;
    layer->max_cache_size = MAX_CACHE_ENTRIES;

    return layer;
}

void tm_webtile_layer_destroy(TM_WebTileLayer *layer) {
    if (!layer) return;

    TM_CachedTile *curr = layer->tile_cache;
    while (curr) {
        TM_CachedTile *next = curr->next;
        if (curr->raster) tm_raster_destroy(curr->raster);
        free(curr);
        curr = next;
    }
    free(layer);
}

void tm_webtile_layer_set_zoom(TM_WebTileLayer *layer, int zoom) {
    if (!layer) return;
    layer->fixed_zoom = zoom;
}

void tm_webtile_layer_set_opacity(TM_WebTileLayer *layer, float opacity) {
    if (!layer) return;
    layer->opacity = opacity;
}

static TM_RasterMap* get_or_fetch_cached(TM_WebTileLayer *layer, int z, int x, int y) {
    /* Check memory cache */
    TM_CachedTile *curr = layer->tile_cache;
    while (curr) {
        if (curr->z == z && curr->x == x && curr->y == y) {
            return curr->raster;
        }
        curr = curr->next;
    }

    /* Fetch from disk cache or network */
    TM_RasterMap *r = tm_webtile_fetch(layer->url_template, z, x, y, layer->cache_dir);
    if (!r) return NULL;

    /* Add to memory cache */
    TM_CachedTile *entry = (TM_CachedTile*)malloc(sizeof(TM_CachedTile));
    if (entry) {
        entry->z = z;
        entry->x = x;
        entry->y = y;
        entry->raster = r;
        entry->next = layer->tile_cache;
        layer->tile_cache = entry;
        layer->cache_count++;

        /* Evict old cache entries if exceeding max cache */
        if (layer->cache_count > layer->max_cache_size) {
            TM_CachedTile *prev = NULL;
            TM_CachedTile *tail = layer->tile_cache;
            while (tail->next) {
                prev = tail;
                tail = tail->next;
            }
            if (prev && tail) {
                prev->next = NULL;
                if (tail->raster) tm_raster_destroy(tail->raster);
                free(tail);
                layer->cache_count--;
            }
        }
    }

    return r;
}

void tm_webtile_layer_render(renderContext *rc, TM_WebTileLayer *layer, const TM_Viewport *vp) {
    if (!rc || !layer || !vp || layer->opacity <= 0.0f) return;

    /* Determine zoom level */
    int z = 0;
    if (layer->fixed_zoom >= 0) {
        z = layer->fixed_zoom;
    } else {
        /* Approximate zoom level from viewport scale */
        float log_z = log2f(vp->zoom > 0.001f ? vp->zoom : 1.0f);
        z = (int)roundf(log_z);
        if (z < 0) z = 0;
        if (z > 18) z = 18;
    }

    int max_tile = (1 << z) - 1;

    /* Determine visible world space bounds */
    Point2 tl_screen = { 0, 0 };
    Point2 br_screen = { vp->screen_w, vp->screen_h };
    TM_Vec2 tl_world = tm_screen_to_world(vp, tl_screen);
    TM_Vec2 br_world = tm_screen_to_world(vp, br_screen);

    float min_wx = tl_world.x < br_world.x ? tl_world.x : br_world.x;
    float max_wx = tl_world.x > br_world.x ? tl_world.x : br_world.x;
    float min_wy = tl_world.y < br_world.y ? tl_world.y : br_world.y;
    float max_wy = tl_world.y > br_world.y ? tl_world.y : br_world.y;

    int min_tx = (int)floorf(min_wx / 256.0f);
    int max_tx = (int)floorf(max_wx / 256.0f);
    int min_ty = (int)floorf(min_wy / 256.0f);
    int max_ty = (int)floorf(max_wy / 256.0f);

    /* Clamp to tile space */
    if (min_tx < 0) min_tx = 0;
    if (max_tx > max_tile) max_tx = max_tile;
    if (min_ty < 0) min_ty = 0;
    if (max_ty > max_tile) max_ty = max_tile;

    /* Limit max tiles per frame to prevent overload */
    int tile_count_x = max_tx - min_tx + 1;
    int tile_count_y = max_ty - min_ty + 1;
    if (tile_count_x * tile_count_y > 64) {
        if (tile_count_x > 8) max_tx = min_tx + 7;
        if (tile_count_y > 8) max_ty = min_ty + 7;
    }

    for (int ty = min_ty; ty <= max_ty; ty++) {
        for (int tx = min_tx; tx <= max_tx; tx++) {
            TM_RasterMap *tile = get_or_fetch_cached(layer, z, tx, ty);
            if (tile) {
                tile->opacity = layer->opacity;
                tm_raster_render(rc, tile, vp);
            }
        }
    }
}
