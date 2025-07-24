cmake_minimum_required(VERSION 3.20)

project(CS2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS_UNKNOWNCHEATS "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_UNKNOWNCHEATS "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_STATIC_LINKER_FLAGS_UNKNOWNCHEATS "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")

# Build configuration options
add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

file(GLOB_RECURSE SOURCE_FILES_CS2 "src/*.cpp")
file(GLOB_RECURSE HEADER_FILES_CS2 "src/*.h" "src/*.hpp")

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES_CS2}
    ${HEADER_FILES_CS2}
)

#add thirdparty libs
add_subdirectory(3rdparty)

# Enable precompiled header
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    $<$<CONFIG:Debug>:_DEBUG _UC _CONSOLE>
    $<$<CONFIG:Release>:NDEBUG _UC _CONSOLE>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Gdi32
    User32
    gdiplus
    wininet
    nlohmann_json
    fmt
    quill
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/offsets/offsets.json"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/offsets.json"
)
