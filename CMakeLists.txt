cmake_minimum_required(VERSION 3.20)

project(red_mist_demo VERSION 0.1.0 LANGUAGES CXX)

# raylib's macOS backend contains Objective-C sources. Explicitly enable that
# language so CMake never attempts to pass .m files through a non-ObjC C mode.
if(APPLE)
    enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(red_mist_demo
    src/main.cpp
    src/game/bullet_system.cpp
    src/game/enemy.cpp
    src/game/boss.cpp
    src/game/stages/stage1_enemies.cpp
    src/game/stages/stage1_boss.cpp
    src/game/stages/stage1_content.cpp
    src/game/settings.cpp
    src/game/stage_system.cpp
    src/game/stage_catalog.cpp
    src/game/asset_manager.cpp
    src/game/sprite_renderer.cpp
    src/game/game.cpp
)

target_include_directories(red_mist_demo PRIVATE src)
target_link_libraries(red_mist_demo PRIVATE raylib)
# 仅 Debug 配置编译判定可视化与可切换的玩家无敌模式。
target_compile_definitions(red_mist_demo PRIVATE
    $<$<CONFIG:Debug>:RED_MIST_DEBUG_FEATURES>
)

# Rebuild the six-face strip only when its source artwork needs changing.
add_executable(generate_skybox EXCLUDE_FROM_ALL tools/generate_skybox.cpp)
target_link_libraries(generate_skybox PRIVATE raylib)

add_executable(generate_sprite_assets EXCLUDE_FROM_ALL
    tools/generate_sprite_assets.cpp)
target_link_libraries(generate_sprite_assets PRIVATE raylib)

add_executable(sprite_benchmark EXCLUDE_FROM_ALL
    tools/sprite_benchmark.cpp
    src/game/asset_manager.cpp
    src/game/sprite_renderer.cpp
    src/game/bullet_system.cpp
    src/game/enemy.cpp
    src/game/boss.cpp
    src/game/stages/stage1_enemies.cpp
    src/game/stages/stage1_boss.cpp
    src/game/stages/stage1_content.cpp
)
target_include_directories(sprite_benchmark PRIVATE src)
target_link_libraries(sprite_benchmark PRIVATE raylib)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:red_mist_demo>/assets
    VERBATIM
)
add_dependencies(red_mist_demo copy_assets)

if(MSVC)
    target_compile_options(red_mist_demo PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(sprite_benchmark PRIVATE /utf-8)
else()
    target_compile_options(red_mist_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(red_mist_tests
        tests/core_tests.cpp
        src/game/stage_catalog.cpp
        src/game/bullet_system.cpp
        src/game/enemy.cpp
        src/game/boss.cpp
    src/game/stages/stage1_enemies.cpp
    src/game/stages/stage1_boss.cpp
    src/game/stages/stage1_content.cpp
        src/game/settings.cpp
        src/game/stage_system.cpp
    )
    target_include_directories(red_mist_tests PRIVATE src)
    target_link_libraries(red_mist_tests PRIVATE raylib)
    add_test(NAME red_mist_core_tests COMMAND red_mist_tests)
endif()
