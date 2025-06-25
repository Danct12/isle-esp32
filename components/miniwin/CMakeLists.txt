idf_component_register(
    SRCS
        # Core
        "miniwin/src/windows/windows.cpp"

        # DDraw
        "miniwin/src/ddraw/ddpalette.cpp"
        "miniwin/src/ddraw/ddraw.cpp"
        "miniwin/src/ddraw/ddsurface.cpp"
        "miniwin/src/ddraw/framebuffer.cpp"

        # D3DRM
        "miniwin/src/d3drm/d3drm.cpp"
        "miniwin/src/d3drm/d3drmdevice.cpp"
        "miniwin/src/d3drm/d3drmframe.cpp"
        "miniwin/src/d3drm/d3drmlight.cpp"
        "miniwin/src/d3drm/d3drmmesh.cpp"
        "miniwin/src/d3drm/d3drmtexture.cpp"
        "miniwin/src/d3drm/d3drmviewport.cpp"
        "miniwin/src/internal/meshutils.cpp"

        # D3DRM backends
        #"src/d3drm/backends/sdl3gpu/renderer.cpp"
        #"src/d3drm/backends/sdl3gpu/shaders/generated/ShaderIndex.cpp"
        "miniwin/src/d3drm/backends/software/renderer.cpp"
    INCLUDE_DIRS "miniwin/include"
    PRIV_INCLUDE_DIRS "miniwin/src/internal"
    REQUIRES georgik__sdl
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-reorder
    -Wno-maybe-uninitialized
    -Wno-parentheses
    -Wno-format
    -Wno-switch
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC MINIWIN)
