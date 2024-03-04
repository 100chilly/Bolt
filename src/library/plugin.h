#ifndef _BOLT_LIBRARY_PLUGIN_H_
#define _BOLT_LIBRARY_PLUGIN_H_

#include <stddef.h>
#include <stdint.h>

struct RenderBatch2D;

/// Struct containing "vtable" callback information for RenderBatch2D's list of vertices.
/// Unless stated otherwise, functions will be called with three params: the index, the specified
/// userdata, and an output pointer, which must be able to index the returned number of items.
struct Vertex2DFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Returns the vertex X and Y, in screen coordinates.
    void (*xy)(size_t index, void* userdata, int32_t* out);
    
    /// Returns the X and Y of the texture image associated with this vertex, in pixel coordinates.
    void (*atlas_xy)(size_t index, void* userdata, int32_t* out);

    /// Returns the W and H of the texture image associated with this vertex, in pixel coordinates.
    void (*atlas_wh)(size_t index, void* userdata, int32_t* out);

    /// Returns the U and V of this vertex in pixel coordinates, normalised from 0.0 to 1.0 within
    /// the sub-image specified by atlas xy and wh.
    void (*uv)(size_t index, void* userdata, double* out);

    /// Returns the RGBA colour of this vertex, each one normalised from 0.0 to 1.0.
    void (*colour)(size_t index, void* userdata, double* out);
};

/// Struct containing "vtable" callback information for textures.
/// Note that in the context of Bolt plugins, textures are always two-dimensional.
struct TextureFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Returns the ID for the associated texture object.
    size_t (*id)(void* userdata);

    /// Returns the size of this texture atlas in pixels.
    void (*size)(void* userdata, size_t* out);

    /// Compares a section of this texture to some RGBA bytes using `memcmp`. Returns true if the
    /// section matches exactly, otherwise false.
    ///
    /// Note that changing the in-game "texture compression" setting will change the contents of
    /// the texture for some images and therefore change the result of this comparison.
    uint8_t (*compare)(void* userdata, size_t x, size_t y, size_t len, const unsigned char* data);
};

/// Struct containing "vtable" callback information for surfaces.
struct SurfaceFunctions {
    /// Userdata which will be passed to the functions contained in this struct.
    void* userdata;

    /// Equivalent to glClearColor(r, g, b, a) & glClear().
    void (*clear)(void* userdata, double r, double g, double b, double a);

    /// Draws a rectangle from the surface, indicated by sx,sy,sw,sh, to a rectangle on the backbuffer,
    /// indicated by dx,dy,dw,dh. All values are in pixels.
    void (*draw_to_screen)(void* userdata, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
};

struct RenderBatch2D {
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t index_count;
    uint32_t vertices_per_icon;
    uint8_t is_minimap;
    struct Vertex2DFunctions vertex_functions;
    struct TextureFunctions texture_functions;
};

struct RenderMinimapEvent {
    double angle;
    double scale;
    double x;
    double y;
};

struct SwapBuffersEvent {};

/// Init the plugin library. Call _bolt_plugin_close at the end of execution, and don't double-init.
/// Must be provided an init function and destroy function for surfaces.
void _bolt_plugin_init(void (*)(struct SurfaceFunctions*, unsigned int, unsigned int), void (*)(void*));

/// Returns true if the plugin library is initialised (i.e. init has been called more recently than
/// close), otherwise false.
uint8_t _bolt_plugin_is_inited();

/// Close the plugin library.
void _bolt_plugin_close();

/// Creates a new instance of a plugin with its own Lua environment (lua_setfenv).
/// The `lua` param will be loaded and executed in a fresh environment, then event callbacks will be
/// sent to it until it is destroyed by the plugin being stopped.
///
/// Returns a unique plugin ID on success or 0 on failure.
uint64_t _bolt_plugin_add(const char* lua);

/// Stops a plugin via its plugin ID, returned from `_bolt_plugin_add`.
void _bolt_plugin_stop(uint64_t);

/// Sends a SwapBuffers event to all plugins.
void _bolt_plugin_handle_swapbuffers(struct SwapBuffersEvent*);

/// Sends a RenderBatch2D to all plugins.
void _bolt_plugin_handle_2d(struct RenderBatch2D*);

/// Sends a RenderMinimap to all plugins.
void _bolt_plugin_handle_minimap(struct RenderMinimapEvent*);

#endif
