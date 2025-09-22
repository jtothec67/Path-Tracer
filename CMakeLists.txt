cmake_minimum_required(VERSION ${CMAKE_VERSION})

project(PATHTRACER)

include_directories(
	src
	contrib/include
	contrib/cl-windows-x84_64/include
)

link_directories(
	contrib/cl-windows-x84_64/lib
)

set(IMGUI_SOURCES
    contrib/include/IMGUI/imgui.cpp
    contrib/include/IMGUI/imgui_draw.cpp
    contrib/include/IMGUI/imgui_tables.cpp
    contrib/include/IMGUI/imgui_widgets.cpp
    contrib/include/IMGUI/imgui_impl_sdl2.cpp
    contrib/include/IMGUI/imgui_impl_opengl3.cpp
)

add_executable(PathTracer
    src/PathTracer/main.cpp

    src/PathTracer/Film.h
    src/PathTracer/Film.cpp

    src/PathTracer/Window.h
    src/PathTracer/Window.cpp
)


# Add ImGui files to the exe
target_sources(PathTracer PRIVATE ${IMGUI_SOURCES})

# Put ImGui sources under a VS filter "ThirdParty/imgui"
source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "ThirdParty/imgui" FILES ${IMGUI_SOURCES})

target_link_libraries(PathTracer SDL2 OpenGL32 glew32)