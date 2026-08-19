# tinyMap

A lightweight, dependency-light map rendering library in C built on top of [tinyGraphics](src/vendor/tinyGraphics).

`tinyMap` provides map abstraction layers (live free web raster tiles, georeferenced raster maps, 2D orthogonal grid maps, 2D isometric tile maps, vector maps with polylines/polygons/markers, and 3D terrain elevation heightmaps) with camera viewport control and coordinate transformations using public `tinyGraphics` features and OpenGL presentation.

---

## Features

- **Live Web Raster Tile Layer (Slippy Map / XYZ)**:
  - Fetches and caches free raster tiles from web providers (e.g. **CartoDB Voyager**, **CartoDB Positron**, **CartoDB Dark Matter**, **OpenStreetMap**, **OpenTopoMap**).
  - Built-in two-tier caching (in-memory LRU cache + persistent local disk cache `.cache/tinymap_tiles/`).
  - Automatically loads and renders visible tiles based on the current camera viewport and zoom level.
  - Full support for **WGS84 Latitude/Longitude** $\leftrightarrow$ **Web Mercator World Coordinates** transformations.
- **Georeferenced Raster Files & Loaders**:
  - Support for **PNG**, **JPEG**, **BMP**, **TGA**, **PSD**, **GIF**, **PNM** (via embedded `stb_image`).
  - Support for **PPM** (P3 ASCII and P6 binary image formats).
  - Support for **ESRI ASCII Grid / DEM** (`.asc`, `.grid`) elevation datasets.
  - Configurable world-space bounding boxes (`TM_Bounds`), opacity blending, and high-performance viewport blitting.
- **2D Tile Maps**: Support for orthogonal and isometric grid maps with customizable color palettes.
- **Vector Maps**: Render markers/POIs, polylines (roads, rivers, boundaries), and polygons (buildings, zones).
- **3D Terrain Heightmaps**: Elevation grid generation, automatic contour/elevation color ramps, wireframe and solid mesh rendering.
- **Viewport & Camera Controls**: Coordinate conversions between world coordinates and screen space.
- **High-level Map Context**: Coordinates background, grid overlays, web tiles, raster layers, tile layers, vectors, and 3D terrain into a unified frameBuffer.
- **Static & Shared Libraries**: Builds `libtinymap.a` and `libtinymap.so`.
- **OpenGL Display**: Uses `tinyGraphics`'s updated `tinyWindow` backend abstraction.
- **Event-driven Navigation**: Uses `tinyGraphics`'s event queue so users can pan, zoom, and center the map from keyboard and mouse input.

---

## Directory Structure

```
tinyMap/
├── Makefile                # Build system (libraries, example, tests)
├── include/
│   └── tinymap.h           # Public API header
├── src/
│   ├── tinymap.c           # Map context and master rendering pipeline
│   ├── tm_webtile.c        # Live web raster tile fetcher, disk/memory cache, layer
│   ├── tm_rastermap.c      # Raster map layer, image/PPM/BMP/ASC loaders, rendering
│   ├── tm_viewport.c       # Viewport, camera, world-to-screen transforms
│   ├── tm_tilemap.c        # 2D orthogonal and isometric tilemap rendering
│   ├── tm_vectormap.c      # Markers, polylines, and polygon rendering
│   ├── tm_heightmap.c      # 3D terrain heightmap and elevation shading
│   └── vendor/
│       ├── stb/            # Single-header image decoding
│       └── tinyGraphics/   # Core rendering engine dependency
├── examples/
│   └── demo_opengl.c       # OpenGL/GLFW map viewer with live web tiles & vectors
└── tests/
    └── test_tinymap.c      # Automated test suite
```

---

## Building

### Prerequisites

- `gcc` or `clang`
- `make`
- `glfw3` and `OpenGL` (for running the OpenGL viewer)
- `curl` (for fetching web raster tiles)

### Makefile Targets

| Target | Description |
|---|---|
| `make` / `make all` | Builds `libtinymap.a`, `libtinymap.so`, and `demo_opengl`. |
| `make static` | Compiles the static library `build/lib/libtinymap.a`. |
| `make shared` | Compiles the shared library `build/lib/libtinymap.so`. |
| `make examples` | Compiles the OpenGL demo in `build/bin/demo_opengl`. |
| `make test` | Compiles and executes the automated unit test suite. |
| `make run` / `make run_opengl` | Launches the OpenGL map viewer. |
| `make clean` | Cleans up all build artifacts in `build/`. |
| `make help` | Displays available Makefile targets. |

---

## Quick Example (C API with Web Tiles & Vector Layers)

```c
#include "tinymap.h"

static TM_MapContext *g_map_ctx;

static void handle_event(Event e) {
    tm_context_handle_event(g_map_ctx, e);
}

int main(void) {
    int width = 800, height = 600;

    /* 1. Initialize tinyGraphics renderContext & frameBuffer */
    renderContext rc = {
        .frame_buffer = createFrameBuffer(width, height),
        .render_mode = FILLED,
        .origin = (Index){ 0, 0, 0 },
        .scene_context = NULL,
        .projection = ORTHOGRAPHIC
    };

    /* 2. Create tinyMap Context and register event navigation */
    TM_MapContext *map_ctx = tm_context_create(&rc, width, height);
    g_map_ctx = map_ctx;
    registerEventHandler(&rc, handle_event);

    /* 3. Add Live Web Raster Tile Layer (CartoDB / OSM) */
    TM_WebTileLayer *webtiles = tm_webtile_layer_create(TM_TILE_CARTO_VOYAGER, ".cache/tinymap_tiles");
    tm_context_set_webtile_layer(map_ctx, webtiles);

    /* 4. Add Vector Features (Roads, Markers) */
    TM_VectorMap *vmap = tm_vectormap_create(16);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 256.0f, 256.0f }, 6, TM_COLOR_MARKER);
    tm_context_set_vectormap(map_ctx, vmap);

    /* 5. Open a tinyGraphics window and render interactively */
    tinyWindow *window = tinyCreateWindow(tinyGetGLFWBackend(), width, height,
                                          "tinyMap Web Tiles", &rc, NULL);
    while (!tinyWindowShouldClose(window)) {
        tm_context_render(map_ctx);
        tinyWindowPresent(window);
    }

    /* 6. Cleanup */
    tinyDestroyWindow(window);
    tm_webtile_layer_destroy(webtiles);
    tm_vectormap_destroy(vmap);
    g_map_ctx = NULL;
    tm_context_destroy(map_ctx);
    destroyFrameBuffer(rc.frame_buffer);

    return 0;
}
```

With the default GLFW backend, use the arrow keys or WASD to pan, `+`/`-` (or `=`/`_`) to zoom, left-click to center on a world position, and right-click to zoom in. Press `Escape` in the full demo to close the window.
