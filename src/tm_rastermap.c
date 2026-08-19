#include "tinymap.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TM_Vec2 tm_geo_to_world(TM_GeoCoord geo, int zoom) {
    double lat = geo.lat;
    double lon = geo.lon;
    if (lat > 85.05112878) lat = 85.05112878;
    if (lat < -85.05112878) lat = -85.05112878;

    double n = pow(2.0, zoom);
    double x = ((lon + 180.0) / 360.0) * n * 256.0;
    double lat_rad = lat * M_PI / 180.0;
    double y = (1.0 - asinh(tan(lat_rad)) / M_PI) / 2.0 * n * 256.0;

    return (TM_Vec2){ (float)x, (float)y };
}

TM_GeoCoord tm_world_to_geo(TM_Vec2 world, int zoom) {
    double n = pow(2.0, zoom);
    double lon = (world.x / (n * 256.0)) * 360.0 - 180.0;
    double y_norm = 1.0 - 2.0 * (world.y / (n * 256.0));
    double lat_rad = atan(sinh(y_norm * M_PI));
    double lat = lat_rad * 180.0 / M_PI;

    return (TM_GeoCoord){ lat, lon };
}

TM_Bounds tm_tile_bounds(int z, int x, int y) {
    (void)z;
    TM_Bounds b;
    b.min_x = (float)(x * 256);
    b.max_x = (float)((x + 1) * 256);
    b.min_y = (float)(y * 256);
    b.max_y = (float)((y + 1) * 256);
    return b;
}

TM_RasterMap* tm_raster_create(int width, int height, TM_Bounds bounds) {
    if (width <= 0 || height <= 0) return NULL;

    TM_RasterMap *raster = (TM_RasterMap*)malloc(sizeof(TM_RasterMap));
    if (!raster) return NULL;

    raster->width = width;
    raster->height = height;
    raster->bounds = bounds;
    raster->opacity = 1.0f;
    raster->pixels = (Color*)calloc(width * height, sizeof(Color));
    if (!raster->pixels) {
        free(raster);
        return NULL;
    }

    return raster;
}

void tm_raster_destroy(TM_RasterMap *raster) {
    if (!raster) return;
    if (raster->pixels) free(raster->pixels);
    free(raster);
}

void tm_raster_set_pixel(TM_RasterMap *raster, int x, int y, Color color) {
    if (!raster || !raster->pixels) return;
    if (x < 0 || x >= raster->width || y < 0 || y >= raster->height) return;
    raster->pixels[y * raster->width + x] = color;
}

Color tm_raster_get_pixel(const TM_RasterMap *raster, int x, int y) {
    Color black = { .literal = 0 };
    if (!raster || !raster->pixels) return black;
    if (x < 0 || x >= raster->width || y < 0 || y >= raster->height) return black;
    return raster->pixels[y * raster->width + x];
}

void tm_raster_set_opacity(TM_RasterMap *raster, float opacity) {
    if (!raster) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    raster->opacity = opacity;
}

void tm_raster_set_bounds(TM_RasterMap *raster, TM_Bounds bounds) {
    if (!raster) return;
    raster->bounds = bounds;
}

void tm_raster_fill(TM_RasterMap *raster, Color color) {
    if (!raster || !raster->pixels) return;
    int total = raster->width * raster->height;
    for (int i = 0; i < total; i++) {
        raster->pixels[i] = color;
    }
}

TM_RasterMap* tm_raster_load_image(const char *filepath, TM_Bounds bounds) {
    if (!filepath) return NULL;
    int w = 0, h = 0, channels = 0;
    unsigned char *data = stbi_load(filepath, &w, &h, &channels, 3);
    if (!data) return NULL;

    TM_RasterMap *raster = tm_raster_create(w, h, bounds);
    if (!raster) {
        stbi_image_free(data);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 3;
            uint8_t r = data[idx + 0];
            uint8_t g = data[idx + 1];
            uint8_t b = data[idx + 2];
            raster->pixels[y * w + x] = tm_color_rgb(r, g, b);
        }
    }

    stbi_image_free(data);
    return raster;
}

TM_RasterMap* tm_raster_load_image_mem(const unsigned char *buffer, int len, TM_Bounds bounds) {
    if (!buffer || len <= 0) return NULL;
    int w = 0, h = 0, channels = 0;
    unsigned char *data = stbi_load_from_memory(buffer, len, &w, &h, &channels, 3);
    if (!data) return NULL;

    TM_RasterMap *raster = tm_raster_create(w, h, bounds);
    if (!raster) {
        stbi_image_free(data);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 3;
            uint8_t r = data[idx + 0];
            uint8_t g = data[idx + 1];
            uint8_t b = data[idx + 2];
            raster->pixels[y * w + x] = tm_color_rgb(r, g, b);
        }
    }

    stbi_image_free(data);
    return raster;
}

static void skip_ppm_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (isspace(c)) continue;
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n');
        } else {
            ungetc(c, f);
            break;
        }
    }
}

TM_RasterMap* tm_raster_load_ppm(const char *filepath, TM_Bounds bounds) {
    if (!filepath) return NULL;
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    char magic[3] = {0};
    if (fscanf(f, "%2s", magic) != 1) {
        fclose(f);
        return NULL;
    }

    bool is_binary = false;
    if (strcmp(magic, "P6") == 0) {
        is_binary = true;
    } else if (strcmp(magic, "P3") == 0) {
        is_binary = false;
    } else {
        fclose(f);
        return NULL;
    }

    skip_ppm_comments(f);
    int width = 0, height = 0, maxval = 0;
    if (fscanf(f, "%d", &width) != 1) { fclose(f); return NULL; }
    skip_ppm_comments(f);
    if (fscanf(f, "%d", &height) != 1) { fclose(f); return NULL; }
    skip_ppm_comments(f);
    if (fscanf(f, "%d", &maxval) != 1) { fclose(f); return NULL; }

    fgetc(f);

    if (width <= 0 || height <= 0 || maxval <= 0) {
        fclose(f);
        return NULL;
    }

    TM_RasterMap *raster = tm_raster_create(width, height, bounds);
    if (!raster) {
        fclose(f);
        return NULL;
    }

    if (is_binary) {
        uint8_t *row_buf = (uint8_t*)malloc(width * 3);
        if (!row_buf) {
            tm_raster_destroy(raster);
            fclose(f);
            return NULL;
        }

        for (int y = 0; y < height; y++) {
            if (fread(row_buf, 3, width, f) != (size_t)width) break;
            for (int x = 0; x < width; x++) {
                uint8_t r = row_buf[x * 3 + 0];
                uint8_t g = row_buf[x * 3 + 1];
                uint8_t b = row_buf[x * 3 + 2];
                if (maxval != 255) {
                    r = (uint8_t)((r * 255) / maxval);
                    g = (uint8_t)((g * 255) / maxval);
                    b = (uint8_t)((b * 255) / maxval);
                }
                raster->pixels[y * width + x] = tm_color_rgb(r, g, b);
            }
        }
        free(row_buf);
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int r = 0, g = 0, b = 0;
                if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) break;
                if (maxval != 255) {
                    r = (r * 255) / maxval;
                    g = (g * 255) / maxval;
                    b = (b * 255) / maxval;
                }
                raster->pixels[y * width + x] = tm_color_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
        }
    }

    fclose(f);
    return raster;
}

int tm_raster_save_ppm(const TM_RasterMap *raster, const char *filepath) {
    if (!raster || !raster->pixels || !filepath) return -1;

    FILE *f = fopen(filepath, "wb");
    if (!f) return -1;

    fprintf(f, "P6\n%d %d\n255\n", raster->width, raster->height);

    uint8_t *row_buf = (uint8_t*)malloc(raster->width * 3);
    if (!row_buf) {
        fclose(f);
        return -1;
    }

    for (int y = 0; y < raster->height; y++) {
        for (int x = 0; x < raster->width; x++) {
            Color c = raster->pixels[y * raster->width + x];
            uint8_t r, g, b;
            tm_color_to_rgb(c, &r, &g, &b);
            row_buf[x * 3 + 0] = r;
            row_buf[x * 3 + 1] = g;
            row_buf[x * 3 + 2] = b;
        }
        fwrite(row_buf, 3, raster->width, f);
    }

    free(row_buf);
    fclose(f);
    return 0;
}

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pels_per_meter;
    int32_t  y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
} BMPInfoHeader;
#pragma pack(pop)

TM_RasterMap* tm_raster_load_bmp(const char *filepath, TM_Bounds bounds) {
    if (!filepath) return NULL;
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    BMPHeader header;
    BMPInfoHeader info;

    if (fread(&header, sizeof(BMPHeader), 1, f) != 1 ||
        fread(&info, sizeof(BMPInfoHeader), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    if (header.type != 0x4D42) {
        fclose(f);
        return NULL;
    }

    if (info.bit_count != 24 && info.bit_count != 32) {
        fclose(f);
        return NULL;
    }

    int width = info.width;
    int height = info.height < 0 ? -info.height : info.height;
    bool top_down = info.height < 0;

    if (width <= 0 || height <= 0) {
        fclose(f);
        return NULL;
    }

    TM_RasterMap *raster = tm_raster_create(width, height, bounds);
    if (!raster) {
        fclose(f);
        return NULL;
    }

    fseek(f, header.offset, SEEK_SET);

    int bytes_per_pixel = info.bit_count / 8;
    int row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
    uint8_t *row_buf = (uint8_t*)malloc(row_size);
    if (!row_buf) {
        tm_raster_destroy(raster);
        fclose(f);
        return NULL;
    }

    for (int y = 0; y < height; y++) {
        int target_y = top_down ? y : (height - 1 - y);
        if (fread(row_buf, 1, row_size, f) != (size_t)row_size) break;

        for (int x = 0; x < width; x++) {
            uint8_t b = row_buf[x * bytes_per_pixel + 0];
            uint8_t g = row_buf[x * bytes_per_pixel + 1];
            uint8_t r = row_buf[x * bytes_per_pixel + 2];
            raster->pixels[target_y * width + x] = tm_color_rgb(r, g, b);
        }
    }

    free(row_buf);
    fclose(f);
    return raster;
}

int tm_raster_save_bmp(const TM_RasterMap *raster, const char *filepath) {
    if (!raster || !raster->pixels || !filepath) return -1;

    FILE *f = fopen(filepath, "wb");
    if (!f) return -1;

    int width = raster->width;
    int height = raster->height;
    int row_size = ((width * 3 + 3) / 4) * 4;
    int image_size = row_size * height;

    BMPHeader header;
    header.type = 0x4D42;
    header.size = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + image_size;
    header.reserved1 = 0;
    header.reserved2 = 0;
    header.offset = sizeof(BMPHeader) + sizeof(BMPInfoHeader);

    BMPInfoHeader info;
    memset(&info, 0, sizeof(BMPInfoHeader));
    info.size = sizeof(BMPInfoHeader);
    info.width = width;
    info.height = height;
    info.planes = 1;
    info.bit_count = 24;
    info.compression = 0;
    info.image_size = image_size;

    fwrite(&header, sizeof(BMPHeader), 1, f);
    fwrite(&info, sizeof(BMPInfoHeader), 1, f);

    uint8_t *row_buf = (uint8_t*)calloc(row_size, 1);
    if (!row_buf) {
        fclose(f);
        return -1;
    }

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            Color c = raster->pixels[y * width + x];
            uint8_t r, g, b;
            tm_color_to_rgb(c, &r, &g, &b);
            row_buf[x * 3 + 0] = b;
            row_buf[x * 3 + 1] = g;
            row_buf[x * 3 + 2] = r;
        }
        fwrite(row_buf, 1, row_size, f);
    }

    free(row_buf);
    fclose(f);
    return 0;
}

TM_RasterMap* tm_raster_load_asc(const char *filepath, float min_elev, float max_elev) {
    if (!filepath) return NULL;
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    int ncols = 0, nrows = 0;
    float xllcorner = 0.0f, yllcorner = 0.0f, cellsize = 1.0f, nodata_val = -9999.0f;

    char key[64];
    for (int i = 0; i < 6; i++) {
        if (fscanf(f, "%63s", key) != 1) break;
        if (strcasecmp(key, "ncols") == 0) {
            if (fscanf(f, "%d", &ncols) != 1) break;
        } else if (strcasecmp(key, "nrows") == 0) {
            if (fscanf(f, "%d", &nrows) != 1) break;
        } else if (strcasecmp(key, "xllcorner") == 0 || strcasecmp(key, "xllcenter") == 0) {
            if (fscanf(f, "%f", &xllcorner) != 1) break;
        } else if (strcasecmp(key, "yllcorner") == 0 || strcasecmp(key, "yllcenter") == 0) {
            if (fscanf(f, "%f", &yllcorner) != 1) break;
        } else if (strcasecmp(key, "cellsize") == 0) {
            if (fscanf(f, "%f", &cellsize) != 1) break;
        } else if (strcasecmp(key, "NODATA_value") == 0) {
            if (fscanf(f, "%f", &nodata_val) != 1) break;
        }
    }

    if (ncols <= 0 || nrows <= 0) {
        fclose(f);
        return NULL;
    }

    TM_Bounds bounds;
    bounds.min_x = xllcorner;
    bounds.max_x = xllcorner + (float)ncols * cellsize;
    bounds.min_y = yllcorner;
    bounds.max_y = yllcorner + (float)nrows * cellsize;

    TM_RasterMap *raster = tm_raster_create(ncols, nrows, bounds);
    if (!raster) {
        fclose(f);
        return NULL;
    }

    float *elev_data = (float*)malloc(ncols * nrows * sizeof(float));
    if (!elev_data) {
        tm_raster_destroy(raster);
        fclose(f);
        return NULL;
    }

    float actual_min = 1e9f, actual_max = -1e9f;
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncols; c++) {
            float val = 0.0f;
            if (fscanf(f, "%f", &val) != 1) val = 0.0f;
            elev_data[r * ncols + c] = val;
            if (fabsf(val - nodata_val) > 0.01f) {
                if (val < actual_min) actual_min = val;
                if (val > actual_max) actual_max = val;
            }
        }
    }
    fclose(f);

    float use_min = (min_elev < max_elev) ? min_elev : actual_min;
    float use_max = (min_elev < max_elev) ? max_elev : actual_max;

    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncols; c++) {
            float val = elev_data[r * ncols + c];
            if (fabsf(val - nodata_val) < 0.01f) {
                raster->pixels[r * ncols + c] = TM_COLOR_WATER;
            } else {
                raster->pixels[r * ncols + c] = tm_heightmap_get_elevation_color(val, use_min, use_max);
            }
        }
    }

    free(elev_data);
    return raster;
}

TM_HeightMap* tm_heightmap_from_asc(const char *filepath) {
    if (!filepath) return NULL;
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    int ncols = 0, nrows = 0;
    float xllcorner = 0.0f, yllcorner = 0.0f, cellsize = 1.0f, nodata_val = -9999.0f;

    char key[64];
    for (int i = 0; i < 6; i++) {
        if (fscanf(f, "%63s", key) != 1) break;
        if (strcasecmp(key, "ncols") == 0) {
            if (fscanf(f, "%d", &ncols) != 1) break;
        } else if (strcasecmp(key, "nrows") == 0) {
            if (fscanf(f, "%d", &nrows) != 1) break;
        } else if (strcasecmp(key, "xllcorner") == 0 || strcasecmp(key, "xllcenter") == 0) {
            if (fscanf(f, "%f", &xllcorner) != 1) break;
        } else if (strcasecmp(key, "yllcorner") == 0 || strcasecmp(key, "yllcenter") == 0) {
            if (fscanf(f, "%f", &yllcorner) != 1) break;
        } else if (strcasecmp(key, "cellsize") == 0) {
            if (fscanf(f, "%f", &cellsize) != 1) break;
        } else if (strcasecmp(key, "NODATA_value") == 0) {
            if (fscanf(f, "%f", &nodata_val) != 1) break;
        }
    }

    if (ncols <= 0 || nrows <= 0) {
        fclose(f);
        return NULL;
    }

    TM_HeightMap *hm = tm_heightmap_create(ncols, nrows, cellsize);
    if (!hm) {
        fclose(f);
        return NULL;
    }

    for (int r = 0; r < nrows; r++) {
        for (int c = 0; c < ncols; c++) {
            float val = 0.0f;
            if (fscanf(f, "%f", &val) != 1) val = 0.0f;
            if (fabsf(val - nodata_val) < 0.01f) val = 0.0f;
            tm_heightmap_set(hm, c, r, val);
        }
    }

    fclose(f);
    return hm;
}

static inline Color blend_color(Color dst, Color src, float alpha) {
    if (alpha >= 1.0f) return src;
    if (alpha <= 0.0f) return dst;

    uint8_t r_dst, g_dst, b_dst;
    uint8_t r_src, g_src, b_src;
    tm_color_to_rgb(dst, &r_dst, &g_dst, &b_dst);
    tm_color_to_rgb(src, &r_src, &g_src, &b_src);

    uint8_t r_out = (uint8_t)(r_dst * (1.0f - alpha) + r_src * alpha);
    uint8_t g_out = (uint8_t)(g_dst * (1.0f - alpha) + g_src * alpha);
    uint8_t b_out = (uint8_t)(b_dst * (1.0f - alpha) + b_src * alpha);

    return tm_color_rgb(r_out, g_out, b_out);
}

void tm_raster_render(renderContext *rc, const TM_RasterMap *raster, const TM_Viewport *vp) {
    if (!rc || !rc->frame_buffer || !raster || !raster->pixels || raster->opacity <= 0.0f) return;

    frameBuffer *fb = rc->frame_buffer;
    float range_x = raster->bounds.max_x - raster->bounds.min_x;
    float range_y = raster->bounds.max_y - raster->bounds.min_y;
    if (range_x <= 0.0f || range_y <= 0.0f) return;

    Point2 p_tl = tm_world_to_screen(vp, (TM_Vec2){ raster->bounds.min_x, raster->bounds.min_y });
    Point2 p_br = tm_world_to_screen(vp, (TM_Vec2){ raster->bounds.max_x, raster->bounds.max_y });

    int start_x = p_tl.x < p_br.x ? p_tl.x : p_br.x;
    int end_x   = p_tl.x > p_br.x ? p_tl.x : p_br.x;
    int start_y = p_tl.y < p_br.y ? p_tl.y : p_br.y;
    int end_y   = p_tl.y > p_br.y ? p_tl.y : p_br.y;

    if (start_x < 0) start_x = 0;
    if (end_x >= fb->width) end_x = fb->width - 1;
    if (start_y < 0) start_y = 0;
    if (end_y >= fb->height) end_y = fb->height - 1;

    if (start_x > end_x || start_y > end_y) return;

    float inv_range_x = 1.0f / range_x;
    float inv_range_y = 1.0f / range_y;

    for (int sy = start_y; sy <= end_y; sy++) {
        for (int sx = start_x; sx <= end_x; sx++) {
            TM_Vec2 wpos = tm_screen_to_world(vp, (Point2){ sx, sy });

            float u = (wpos.x - raster->bounds.min_x) * inv_range_x;
            float v = (wpos.y - raster->bounds.min_y) * inv_range_y;

            if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
                int rx = (int)(u * (float)(raster->width - 1));
                int ry = (int)(v * (float)(raster->height - 1));

                if (rx < 0) rx = 0;
                if (rx >= raster->width) rx = raster->width - 1;
                if (ry < 0) ry = 0;
                if (ry >= raster->height) ry = raster->height - 1;

                Color src_col = raster->pixels[ry * raster->width + rx];
                int fb_idx = sy * fb->width + sx;

                if (raster->opacity >= 0.99f) {
                    fb->buffer[fb_idx].color = src_col;
                } else {
                    fb->buffer[fb_idx].color = blend_color(fb->buffer[fb_idx].color, src_col, raster->opacity);
                }
            }
        }
    }
}
