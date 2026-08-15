#include "tinymap.h"
#include <stdlib.h>
#include <float.h>

TM_HeightMap* tm_heightmap_create(int cols, int rows, float cell_size) {
    if (cols <= 0 || rows <= 0 || cell_size <= 0.0f) return NULL;

    TM_HeightMap *hm = (TM_HeightMap*)malloc(sizeof(TM_HeightMap));
    if (!hm) return NULL;

    hm->cols = cols;
    hm->rows = rows;
    hm->cell_size = cell_size;
    hm->heights = (float*)calloc(cols * rows, sizeof(float));
    hm->min_height = 0.0f;
    hm->max_height = 100.0f;

    return hm;
}

void tm_heightmap_destroy(TM_HeightMap *hm) {
    if (!hm) return;
    if (hm->heights) free(hm->heights);
    free(hm);
}

void tm_heightmap_set(TM_HeightMap *hm, int col, int row, float height) {
    if (!hm || !hm->heights) return;
    if (col < 0 || col >= hm->cols || row < 0 || row >= hm->rows) return;
    hm->heights[row * hm->cols + col] = height;
    if (height < hm->min_height) hm->min_height = height;
    if (height > hm->max_height) hm->max_height = height;
}

float tm_heightmap_get(const TM_HeightMap *hm, int col, int row) {
    if (!hm || !hm->heights) return 0.0f;
    if (col < 0 || col >= hm->cols || row < 0 || row >= hm->rows) return 0.0f;
    return hm->heights[row * hm->cols + col];
}

Color tm_heightmap_get_elevation_color(float height, float min_h, float max_h) {
    float range = max_h - min_h;
    if (range <= 0.0f) range = 1.0f;
    float norm = (height - min_h) / range;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    if (norm < 0.2f) {
        return TM_COLOR_WATER;
    } else if (norm < 0.35f) {
        return TM_COLOR_SAND;
    } else if (norm < 0.65f) {
        return TM_COLOR_GRASS;
    } else if (norm < 0.85f) {
        return TM_COLOR_ROCK;
    } else {
        return TM_COLOR_SNOW;
    }
}

static Point3 make_point3(const TM_HeightMap *hm, int c, int r, Point3 origin, float scale) {
    float x = origin.x + ((float)c - (float)hm->cols / 2.0f) * hm->cell_size * scale;
    float z = origin.z + ((float)r - (float)hm->rows / 2.0f) * hm->cell_size * scale;
    float y = origin.y + tm_heightmap_get(hm, c, r) * scale;
    Point3 p = { (int)x, (int)y, (int)z };
    return p;
}

void tm_heightmap_render_wireframe(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale) {
    if (!rc || !hm || !hm->heights) return;

    for (int r = 0; r < hm->rows; r++) {
        for (int c = 0; c < hm->cols; c++) {
            Point3 p_curr = make_point3(hm, c, r, origin, scale);
            float h = tm_heightmap_get(hm, c, r);
            Color col = tm_heightmap_get_elevation_color(h, hm->min_height, hm->max_height);

            if (c + 1 < hm->cols) {
                Point3 p_right = make_point3(hm, c + 1, r, origin, scale);
                renderLine3D(rc, p_curr, p_right, col);
            }
            if (r + 1 < hm->rows) {
                Point3 p_down = make_point3(hm, c, r + 1, origin, scale);
                renderLine3D(rc, p_curr, p_down, col);
            }
        }
    }
}

void tm_heightmap_render_solid(renderContext *rc, const TM_HeightMap *hm, Point3 origin, float scale) {
    if (!rc || !hm || !hm->heights) return;

    for (int r = 0; r < hm->rows - 1; r++) {
        for (int c = 0; c < hm->cols - 1; c++) {
            Point3 p00 = make_point3(hm, c,     r,     origin, scale);
            Point3 p10 = make_point3(hm, c + 1, r,     origin, scale);
            Point3 p01 = make_point3(hm, c,     r + 1, origin, scale);
            Point3 p11 = make_point3(hm, c + 1, r + 1, origin, scale);

            float avg_h1 = (tm_heightmap_get(hm, c, r) + tm_heightmap_get(hm, c + 1, r) + tm_heightmap_get(hm, c, r + 1)) / 3.0f;
            Color col1 = tm_heightmap_get_elevation_color(avg_h1, hm->min_height, hm->max_height);

            Point3 *tri1[3] = { &p00, &p10, &p01 };
            renderTriangle3D(rc, tri1, col1);

            float avg_h2 = (tm_heightmap_get(hm, c + 1, r) + tm_heightmap_get(hm, c + 1, r + 1) + tm_heightmap_get(hm, c, r + 1)) / 3.0f;
            Color col2 = tm_heightmap_get_elevation_color(avg_h2, hm->min_height, hm->max_height);

            Point3 *tri2[3] = { &p10, &p11, &p01 };
            renderTriangle3D(rc, tri2, col2);
        }
    }
}
