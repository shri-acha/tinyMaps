#include "tinymap.h"
#include <stdlib.h>
#include <string.h>

static const Color DEFAULT_PALETTE[] = {
    { .literal = 0x0000 },                         /* 0: Empty / Black */
    { .literal = (5 << 11) | (20 << 5) | 28 },     /* 1: Water */
    { .literal = (30 << 11) | (52 << 5) | 7 },     /* 2: Sand */
    { .literal = (4 << 11) | (43 << 5) | 12 },     /* 3: Grass */
    { .literal = (3 << 11) | (33 << 5) | 9 },      /* 4: Forest */
    { .literal = (15 << 11) | (35 << 5) | 17 },    /* 5: Road */
    { .literal = (23 << 11) | (48 << 5) | 24 },    /* 6: Building */
    { .literal = (31 << 11) | (63 << 5) | 31 }     /* 7: Snow/White */
};

TM_TileMap* tm_tilemap_create(int cols, int rows, int tile_size, TM_MapProjection proj) {
    if (cols <= 0 || rows <= 0 || tile_size <= 0) return NULL;
    
    TM_TileMap *map = (TM_TileMap*)malloc(sizeof(TM_TileMap));
    if (!map) return NULL;
    
    map->cols = cols;
    map->rows = rows;
    map->tile_size = tile_size;
    map->projection = proj;
    map->tiles = (int*)calloc(cols * rows, sizeof(int));
    
    int default_pal_size = sizeof(DEFAULT_PALETTE) / sizeof(DEFAULT_PALETTE[0]);
    map->palette = (Color*)malloc(default_pal_size * sizeof(Color));
    memcpy(map->palette, DEFAULT_PALETTE, default_pal_size * sizeof(Color));
    map->palette_size = default_pal_size;
    
    return map;
}

void tm_tilemap_destroy(TM_TileMap *map) {
    if (!map) return;
    if (map->tiles) free(map->tiles);
    if (map->palette) free(map->palette);
    free(map);
}

void tm_tilemap_set_palette(TM_TileMap *map, const Color *palette, int size) {
    if (!map || !palette || size <= 0) return;
    Color *new_pal = (Color*)realloc(map->palette, size * sizeof(Color));
    if (new_pal) {
        map->palette = new_pal;
        memcpy(map->palette, palette, size * sizeof(Color));
        map->palette_size = size;
    }
}

void tm_tilemap_set_tile(TM_TileMap *map, int col, int row, int tile_id) {
    if (!map || !map->tiles) return;
    if (col < 0 || col >= map->cols || row < 0 || row >= map->rows) return;
    map->tiles[row * map->cols + col] = tile_id;
}

int tm_tilemap_get_tile(const TM_TileMap *map, int col, int row) {
    if (!map || !map->tiles) return -1;
    if (col < 0 || col >= map->cols || row < 0 || row >= map->rows) return -1;
    return map->tiles[row * map->cols + col];
}

void tm_tilemap_fill(TM_TileMap *map, int tile_id) {
    if (!map || !map->tiles) return;
    for (int i = 0; i < map->cols * map->rows; i++) {
        map->tiles[i] = tile_id;
    }
}

/* Helper to render filled orthogonal quad using 2 triangles */
static void render_ortho_tile(renderContext *rc, Point2 p0, Point2 p1, Point2 p2, Point2 p3, Color color) {
    Point2 *tri1[3] = { &p0, &p1, &p2 };
    Point2 *tri2[3] = { &p0, &p2, &p3 };
    renderTriangle2D(rc, tri1, color);
    renderTriangle2D(rc, tri2, color);
}

void tm_tilemap_render(renderContext *rc, const TM_TileMap *map, const TM_Viewport *vp) {
    if (!rc || !map || !map->tiles) return;

    int ts = map->tile_size;

    for (int r = 0; r < map->rows; r++) {
        for (int c = 0; c < map->cols; c++) {
            int tile_id = map->tiles[r * map->cols + c];
            if (tile_id <= 0 || tile_id >= map->palette_size) continue;
            Color tile_color = map->palette[tile_id];

            if (map->projection == TM_MAP_ORTHOGONAL) {
                TM_Vec2 w0 = { (float)(c * ts), (float)(r * ts) };
                TM_Vec2 w1 = { (float)((c + 1) * ts), (float)(r * ts) };
                TM_Vec2 w2 = { (float)((c + 1) * ts), (float)((r + 1) * ts) };
                TM_Vec2 w3 = { (float)(c * ts), (float)((r + 1) * ts) };

                Point2 p0 = tm_world_to_screen(vp, w0);
                Point2 p1 = tm_world_to_screen(vp, w1);
                Point2 p2 = tm_world_to_screen(vp, w2);
                Point2 p3 = tm_world_to_screen(vp, w3);

                /* Quick viewport culling */
                int min_x = p0.x < p1.x ? (p0.x < p2.x ? p0.x : p2.x) : (p1.x < p2.x ? p1.x : p2.x);
                int max_x = p0.x > p1.x ? (p0.x > p2.x ? p0.x : p2.x) : (p1.x > p2.x ? p1.x : p2.x);
                int min_y = p0.y < p1.y ? (p0.y < p2.y ? p0.y : p2.y) : (p1.y < p2.y ? p1.y : p2.y);
                int max_y = p0.y > p1.y ? (p0.y > p2.y ? p0.y : p2.y) : (p1.y > p2.y ? p1.y : p2.y);

                if (vp && (max_x < 0 || min_x >= vp->screen_w || max_y < 0 || min_y >= vp->screen_h)) {
                    continue;
                }

                render_ortho_tile(rc, p0, p1, p2, p3, tile_color);
            } else {
                /* Isometric tile */
                float iso_w = (float)ts;
                float iso_h = (float)ts / 2.0f;
                
                float center_x = (float)(c - r) * (iso_w / 2.0f);
                float center_y = (float)(c + r) * (iso_h / 2.0f);

                TM_Vec2 w_top   = { center_x, center_y - iso_h / 2.0f };
                TM_Vec2 w_right = { center_x + iso_w / 2.0f, center_y };
                TM_Vec2 w_bot   = { center_x, center_y + iso_h / 2.0f };
                TM_Vec2 w_left  = { center_x - iso_w / 2.0f, center_y };

                Point2 p_top   = tm_world_to_screen(vp, w_top);
                Point2 p_right = tm_world_to_screen(vp, w_right);
                Point2 p_bot   = tm_world_to_screen(vp, w_bot);
                Point2 p_left  = tm_world_to_screen(vp, w_left);

                Point2 *tri1[3] = { &p_top, &p_right, &p_bot };
                Point2 *tri2[3] = { &p_top, &p_bot, &p_left };
                renderTriangle2D(rc, tri1, tile_color);
                renderTriangle2D(rc, tri2, tile_color);
            }
        }
    }
}
