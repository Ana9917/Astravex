cmake_minimum_required(VERSION 3.21)
project(SatelliteOrbitSimulator LANGUAGES CXX)

find_package(SDL3 3.2 CONFIG REQUIRED)

add_executable(SatelliteOrbit WIN32
    Engine.cpp
    functions.cpp
    shapes.cpp
)
set_target_properties(SatelliteOrbit PROPERTIES
    OUTPUT_NAME "Astravex"
)
# Dear ImGui source files
target_sources(SatelliteOrbit PRIVATE
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_demo.cpp

    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(SatelliteOrbit PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends"
)

target_compile_features(SatelliteOrbit PRIVATE cxx_std_17)
target_link_libraries(SatelliteOrbit PRIVATE SDL3::SDL3)

if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET SatelliteOrbit POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>"
            "$<TARGET_FILE_DIR:SatelliteOrbit>"
        VERBATIM
    )
endif()

add_custom_target(run
    COMMAND $<TARGET_FILE:SatelliteOrbit>
    DEPENDS SatelliteOrbit
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
