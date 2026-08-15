# tinyMap

A lightweight, dependency-light map rendering library in C built on top of [tinyGraphics](src/vendor/tinyGraphics).

`tinyMap` provides map abstraction layers (2D orthogonal grid maps, 2D isometric tile maps, vector maps with polylines/polygons/markers, and 3D terrain elevation heightmaps) with camera viewport control and coordinate transformations using public `tinyGraphics` features and OpenGL presentation.

---

## Features

- **2D Tile Maps**: Support for orthogonal and isometric grid maps with customizable color palettes.
- **Vector Maps**: Render markers/POIs, polylines (roads, rivers, boundaries), and polygons (buildings, zones).
- **3D Terrain Heightmaps**: Elevation grid generation, automatic contour/elevation color ramps, wireframe and solid mesh rendering.
- **Viewport & Camera Controls**: Coordinate conversions between world coordinates and screen space.
- **High-level Map Context**: Coordinates background, grid overlays, tile layers, vectors, and 3D terrain into a unified frameBuffer.
- **Static & Shared Libraries**: Builds `libtinymap.a` and `libtinymap.so`.
- **OpenGL Display**: Uses `tinyGraphics`'s vendor `gl_ext.h` texture presentation macros (`TINY_GL_INIT_TEXTURE`, `TINY_GL_PRESENT`).

---

## Directory Structure

```
tinyMap/
├── Makefile                # Build system (libraries, example, tests)
├── include/
│   └── tinymap.h           # Public API header
├── src/
│   ├── tinymap.c           # Map context and master rendering pipeline
│   ├── tm_viewport.c       # Viewport, camera, world-to-screen transforms
│   ├── tm_tilemap.c        # 2D orthogonal and isometric tilemap rendering
│   ├── tm_vectormap.c      # Markers, polylines, and polygon rendering
│   ├── tm_heightmap.c      # 3D terrain heightmap and elevation shading
│   └── vendor/
│       └── tinyGraphics/   # Core rendering engine dependency
├── examples/
│   └── demo_opengl.c       # OpenGL/GLFW map viewer using vendor gl_ext.h
└── tests/
    └── test_tinymap.c      # Automated test suite
```

---

## Building

### Prerequisites

- `gcc` or `clang`
- `make`
- `glfw3` and `OpenGL` (for running the OpenGL viewer)

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

## Quick Example (C API)

```c
#include "tinymap.h"

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

    /* 2. Create tinyMap Context */
    TM_MapContext *map_ctx = tm_context_create(&rc, width, height);

    /* 3. Create a 2D TileMap (40x30 tiles, 20px each) */
    TM_TileMap *tilemap = tm_tilemap_create(40, 30, 20, TM_MAP_ORTHOGONAL);
    tm_tilemap_fill(tilemap, 3); /* Fill with grass */
    tm_context_set_tilemap(map_ctx, tilemap);

    /* 4. Add Vector Features (Roads, Markers) */
    TM_VectorMap *vmap = tm_vectormap_create(16);
    tm_vectormap_add_marker(vmap, (TM_Vec2){ 100.0f, 150.0f }, 5, TM_COLOR_MARKER);
    tm_context_set_vectormap(map_ctx, vmap);

    /* 5. Render Scene */
    tm_context_render(map_ctx);

    /* 6. Cleanup */
    tm_tilemap_destroy(tilemap);
    tm_vectormap_destroy(vmap);
    tm_context_destroy(map_ctx);
    destroyFrameBuffer(rc.frame_buffer);

    return 0;
}
```
