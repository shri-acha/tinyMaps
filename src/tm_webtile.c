#include "tinymap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#define DEFAULT_CACHE_DIR ".cache/tinymap_tiles"
#define MAX_CACHE_ENTRIES 64
#define MAX_VISIBLE_TILES_X 64
#define MAX_VISIBLE_TILES_Y 64
#define MAX_PENDING_TILE_REQUESTS 512

typedef struct TM_TileRequest {
    int z;
    int x;
    int y;
    struct TM_TileRequest *next;
} TM_TileRequest;

typedef struct TM_WebTileWorker {
    TM_WebTileLayer *layer;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    TM_TileRequest *head;
    TM_TileRequest *tail;
    int pending_count;
    bool thread_started;
    bool shutdown;
} TM_WebTileWorker;

static void *webtile_worker_main(void *arg);

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int wrap_tile_x(long long tile_x, int tile_count) {
    long long wrapped = tile_x % (long long)tile_count;
    if (wrapped < 0) wrapped += (long long)tile_count;
    return (int)wrapped;
}

static int tile_depth_from_viewport_zoom(float viewport_zoom) {
    if (viewport_zoom <= 0.0f) return 0;

    float zoom_log = log2f(viewport_zoom);
    int depth = (int)lroundf(zoom_log);
    return clamp_int(depth, 0, 19);
}

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

static TM_RasterMap* cache_lookup_locked(TM_WebTileLayer *layer, int z, int x, int y) {
    TM_CachedTile *curr = layer->tile_cache;
    while (curr) {
        if (curr->z == z && curr->x == x && curr->y == y) {
            return curr->raster;
        }
        curr = curr->next;
    }
    return NULL;
}

static bool request_is_queued_locked(TM_WebTileWorker *worker, int z, int x, int y) {
    TM_TileRequest *req = worker->head;
    while (req) {
        if (req->z == z && req->x == x && req->y == y) {
            return true;
        }
        req = req->next;
    }
    return false;
}

static void cache_insert_locked(TM_WebTileLayer *layer, int z, int x, int y, TM_RasterMap *raster) {
    if (!raster) return;

    /* The worker is the only writer, but guard against a duplicate arriving
     * after shutdown or from a future request. */
    if (cache_lookup_locked(layer, z, x, y)) {
        tm_raster_destroy(raster);
        return;
    }

    TM_CachedTile *entry = (TM_CachedTile*)malloc(sizeof(TM_CachedTile));
    if (!entry) {
        tm_raster_destroy(raster);
        return;
    }

    entry->z = z;
    entry->x = x;
    entry->y = y;
    entry->raster = raster;
    entry->next = layer->tile_cache;
    layer->tile_cache = entry;
    layer->cache_count++;

    /* Evict the oldest entry when the in-memory cache grows too large. */
    if (layer->cache_count > layer->max_cache_size) {
        TM_CachedTile *prev = NULL;
        TM_CachedTile *tail = layer->tile_cache;
        while (tail && tail->next) {
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

static void enqueue_tile_request_locked(TM_WebTileLayer *layer, int z, int x, int y) {
    TM_WebTileWorker *worker = layer->worker;
    if (!worker) return;

    if (cache_lookup_locked(layer, z, x, y)) return;
    if (request_is_queued_locked(worker, z, x, y)) return;
    if (worker->pending_count >= MAX_PENDING_TILE_REQUESTS) return;

    TM_TileRequest *req = (TM_TileRequest*)calloc(1, sizeof(TM_TileRequest));
    if (!req) return;

    req->z = z;
    req->x = x;
    req->y = y;
    req->next = NULL;

    if (worker->tail) {
        worker->tail->next = req;
    } else {
        worker->head = req;
    }
    worker->tail = req;
    worker->pending_count++;

    pthread_cond_signal(&worker->cond);
}

static void *webtile_worker_main(void *arg) {
    TM_WebTileWorker *worker = (TM_WebTileWorker*)arg;
    TM_WebTileLayer *layer = worker->layer;

    for (;;) {
        pthread_mutex_lock(&worker->mutex);

        while (!worker->shutdown && !worker->head) {
            pthread_cond_wait(&worker->cond, &worker->mutex);
        }

        if (worker->shutdown) {
            pthread_mutex_unlock(&worker->mutex);
            break;
        }

        TM_TileRequest *req = worker->head;
        worker->head = req->next;
        if (!worker->head) {
            worker->tail = NULL;
        }
        worker->pending_count--;
        pthread_mutex_unlock(&worker->mutex);

        /* Downloading and image decoding happen off the render thread. */
        TM_RasterMap *raster = tm_webtile_fetch(layer->url_template, req->z, req->x, req->y, layer->cache_dir);

        pthread_mutex_lock(&worker->mutex);
        if (worker->shutdown) {
            if (raster) tm_raster_destroy(raster);
            pthread_mutex_unlock(&worker->mutex);
            free(req);
            break;
        }
        cache_insert_locked(layer, req->z, req->x, req->y, raster);
        pthread_mutex_unlock(&worker->mutex);

        free(req);
    }

    return NULL;
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
    layer->wrap_x = true;
    layer->tile_cache = NULL;
    layer->cache_count = 0;
    layer->max_cache_size = MAX_CACHE_ENTRIES;
    layer->worker = NULL;

    TM_WebTileWorker *worker = (TM_WebTileWorker*)calloc(1, sizeof(TM_WebTileWorker));
    if (!worker) {
        free(layer);
        return NULL;
    }

    worker->layer = layer;
    worker->head = NULL;
    worker->tail = NULL;
    worker->pending_count = 0;
    worker->shutdown = false;
    worker->thread_started = false;

    if (pthread_mutex_init(&worker->mutex, NULL) != 0) {
        free(worker);
        free(layer);
        return NULL;
    }
    if (pthread_cond_init(&worker->cond, NULL) != 0) {
        pthread_mutex_destroy(&worker->mutex);
        free(worker);
        free(layer);
        return NULL;
    }

    layer->worker = worker;

    if (pthread_create(&worker->thread, NULL, webtile_worker_main, worker) != 0) {
        pthread_cond_destroy(&worker->cond);
        pthread_mutex_destroy(&worker->mutex);
        layer->worker = NULL;
        free(worker);
        free(layer);
        return NULL;
    }

    worker->thread_started = true;

    return layer;
}

void tm_webtile_layer_destroy(TM_WebTileLayer *layer) {
    if (!layer) return;

    TM_WebTileWorker *worker = layer->worker;
    if (worker) {
        pthread_mutex_lock(&worker->mutex);
        worker->shutdown = true;
        pthread_cond_broadcast(&worker->cond);
        pthread_mutex_unlock(&worker->mutex);

        if (worker->thread_started) {
            pthread_join(worker->thread, NULL);
        }

        TM_TileRequest *req = worker->head;
        while (req) {
            TM_TileRequest *next = req->next;
            free(req);
            req = next;
        }

        pthread_cond_destroy(&worker->cond);
        pthread_mutex_destroy(&worker->mutex);
        free(worker);
        layer->worker = NULL;
    }

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

void tm_webtile_layer_set_wrap_x(TM_WebTileLayer *layer, bool wrap_x) {
    if (!layer) return;
    layer->wrap_x = wrap_x;
}

void tm_webtile_layer_render(renderContext *rc, TM_WebTileLayer *layer, const TM_Viewport *vp) {
    if (!rc || !layer || !vp || layer->opacity <= 0.0f) return;

    /* Determine zoom level */
    int z = 0;
    if (layer->fixed_zoom >= 0) {
        z = layer->fixed_zoom;
    } else {
        /* In auto mode, each depth level is selected from the current viewport zoom. */
        z = tile_depth_from_viewport_zoom(vp->zoom);
    }
    z = clamp_int(z, 0, 19);

    int tile_count = 1 << z;
    float tile_world_size = 256.0f / (float)tile_count;

    /* Determine visible world space bounds */
    Point2 tl_screen = { 0, 0 };
    Point2 br_screen = { vp->screen_w, vp->screen_h };
    TM_Vec2 tl_world = tm_screen_to_world(vp, tl_screen);
    TM_Vec2 br_world = tm_screen_to_world(vp, br_screen);

    float min_wx = tl_world.x < br_world.x ? tl_world.x : br_world.x;
    float max_wx = tl_world.x > br_world.x ? tl_world.x : br_world.x;
    float min_wy = tl_world.y < br_world.y ? tl_world.y : br_world.y;
    float max_wy = tl_world.y > br_world.y ? tl_world.y : br_world.y;

    /* Vertical tile range is clamped to the Mercator world. */
    int min_ty = (int)floorf(min_wy / tile_world_size);
    int max_ty = (int)floorf(max_wy / tile_world_size);
    if (min_ty < 0) min_ty = 0;
    if (max_ty > tile_count - 1) max_ty = tile_count - 1;
    if (min_ty > max_ty) return;

    /*
     * Horizontal tile range is intentionally not clamped.  Wrapping it around
     * the world later gives us an endless east/west map.
     */
    long long min_tx = (long long)floorf(min_wx / tile_world_size);
    long long max_tx = (long long)floorf(max_wx / tile_world_size);
    if (min_tx > max_tx) return;

    /* Keep tile fetching bounded even when the camera is zoomed far out. */
    if (max_tx - min_tx + 1 > MAX_VISIBLE_TILES_X) {
        max_tx = min_tx + MAX_VISIBLE_TILES_X - 1;
    }
    if (max_ty - min_ty + 1 > MAX_VISIBLE_TILES_Y) {
        max_ty = min_ty + MAX_VISIBLE_TILES_Y - 1;
    }

    TM_WebTileWorker *worker = layer->worker;
    if (worker) {
        pthread_mutex_lock(&worker->mutex);
    }

    for (int ty = min_ty; ty <= max_ty; ty++) {
        for (long long tx = min_tx; tx <= max_tx; tx++) {
            int source_tx;
            if (layer->wrap_x) {
                source_tx = wrap_tile_x(tx, tile_count);
            } else {
                if (tx < 0 || tx >= tile_count) continue;
                source_tx = (int)tx;
            }

            TM_RasterMap *tile = cache_lookup_locked(layer, z, source_tx, ty);
            if (!tile) {
                enqueue_tile_request_locked(layer, z, source_tx, ty);
                continue;
            }

            /*
             * The cached raster carries the canonical tile bounds.  For
             * wrapped horizontal copies we temporarily render it at the
             * shifted world position for this repetition.
             */
            TM_Bounds original_bounds = tile->bounds;
            float original_opacity = tile->opacity;

            tile->bounds.min_x = (float)tx * tile_world_size;
            tile->bounds.max_x = tile->bounds.min_x + tile_world_size;
            tile->bounds.min_y = (float)ty * tile_world_size;
            tile->bounds.max_y = tile->bounds.min_y + tile_world_size;
            tile->opacity = layer->opacity;

            tm_raster_render(rc, tile, vp);

            tile->bounds = original_bounds;
            tile->opacity = original_opacity;
        }
    }

    if (worker) {
        pthread_mutex_unlock(&worker->mutex);
    }
}
