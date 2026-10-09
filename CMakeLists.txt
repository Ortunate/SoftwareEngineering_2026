# 本文件负责配置游戏引擎、程序、素材工具与测试目标。
cmake_minimum_required(VERSION 3.20)

# 根工程显式启用 C，供原生桥接与 raylib 共用完整的语言配置。
project(red_mist_demo VERSION 0.1.0 LANGUAGES C CXX)

# macOS 原生输入桥接和 raylib 后端包含 Objective-C 源码，显式启用对应语言。
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

# 游戏实现只编译一次，所有目标共用同一份实现。
add_library(red_mist_engine STATIC
    src/app/game.cpp
    src/app/input.cpp
    src/config/settings.cpp
    src/gameplay/actors/boss.cpp
    src/gameplay/actors/enemy.cpp
    src/gameplay/actors/player.cpp
    src/gameplay/bullets/system.cpp
    src/gameplay/combat/combat.cpp
    src/gameplay/effects/actor_effects.cpp
    src/gameplay/items/drop_system.cpp
    src/platform/window_input.cpp
    src/render/menu_render.cpp
    src/render/game_render.cpp
    src/render/sprite_renderer.cpp
    src/resources/asset_manager.cpp
    src/stage/catalog.cpp
    src/stage/content.cpp
    src/stage/enemy_spec.cpp
    src/stage/system.cpp
    src/stages/stage1/boss.cpp
    src/stages/stage1/content.cpp
    src/stages/stage1/dialogue.cpp
    src/stages/stage1/enemies.cpp
    src/stages/stage1/manifest.cpp
    src/stages/stage1/timeline.cpp
)
target_include_directories(red_mist_engine PUBLIC src)
target_link_libraries(red_mist_engine PUBLIC raylib)
if(APPLE)
    target_sources(red_mist_engine PRIVATE src/platform/window_input_macos.m)
    target_link_libraries(red_mist_engine PRIVATE "-framework AppKit")
elseif(WIN32)
    target_link_libraries(red_mist_engine PRIVATE imm32)
endif()
# Game 的条件成员需要所有使用方采用相同的类布局。
target_compile_definitions(red_mist_engine PUBLIC
    $<$<CONFIG:Debug>:RED_MIST_DEBUG_FEATURES>
)

add_executable(red_mist_demo src/main.cpp)
target_link_libraries(red_mist_demo PRIVATE red_mist_engine)
# Windows 每次构建均嵌入 UTF-8 清单，避免重新编译后中文音乐路径再次闪退。
if(MSVC)
    target_sources(red_mist_demo PRIVATE src/windows_utf8.manifest)
endif()

# Rebuild the six-face strip only when its source artwork needs changing.
add_executable(generate_skybox EXCLUDE_FROM_ALL tools/generate_skybox.cpp)
target_link_libraries(generate_skybox PRIVATE raylib)

add_executable(generate_sprite_assets EXCLUDE_FROM_ALL
    tools/generate_sprite_assets.cpp)
target_link_libraries(generate_sprite_assets PRIVATE raylib)

add_executable(sprite_benchmark EXCLUDE_FROM_ALL tools/sprite_benchmark.cpp)
target_link_libraries(sprite_benchmark PRIVATE red_mist_engine)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            $<TARGET_FILE_DIR:red_mist_demo>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime
            $<TARGET_FILE_DIR:red_mist_demo>/assets
    VERBATIM
)
add_dependencies(red_mist_demo copy_assets)

if(MSVC)
    target_compile_options(red_mist_engine PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(red_mist_demo PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(red_mist_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(red_mist_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(red_mist_tests
        tests/core_tests.cpp
        tests/actor_effect_tests.cpp
        tests/drop_tests.cpp
        tests/gameplay_tests.cpp
        tests/player_tests.cpp
        tests/settings_tests.cpp
        tests/stage_tests.cpp)
    target_link_libraries(red_mist_tests PRIVATE red_mist_engine)
    # 离屏生命周期测试仍需要图形会话，手动运行，避免普通无窗口测试依赖桌面服务。
    add_executable(red_mist_lifecycle_tests EXCLUDE_FROM_ALL tests/lifecycle_tests.cpp)
    target_link_libraries(red_mist_lifecycle_tests PRIVATE red_mist_engine)
    if(MSVC)
        target_sources(red_mist_lifecycle_tests PRIVATE src/windows_utf8.manifest)
        target_compile_options(red_mist_lifecycle_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(red_mist_lifecycle_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
    add_dependencies(red_mist_lifecycle_tests copy_assets)
    if(MSVC)
        target_sources(red_mist_tests PRIVATE src/windows_utf8.manifest)
    endif()
    # assert 在 Release 中仍需执行，否则测试主体会被 NDEBUG 跳过。
    if(MSVC)
        target_compile_options(red_mist_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(red_mist_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME red_mist_core_tests COMMAND red_mist_tests)
endif()

# 隐藏窗口交互测试需 OpenGL/音频环境，按需构建，不影响无图形环境的核心测试。
if(BUILD_TESTING)
    add_executable(red_mist_settings_ui_tests EXCLUDE_FROM_ALL tests/settings_ui_tests.cpp)
    target_link_libraries(red_mist_settings_ui_tests PRIVATE red_mist_engine)
    add_dependencies(red_mist_settings_ui_tests copy_assets)
    if(MSVC)
        target_compile_options(red_mist_settings_ui_tests PRIVATE /utf-8)
        target_sources(red_mist_settings_ui_tests PRIVATE src/windows_utf8.manifest)
    endif()
endif()

# 原生输入测试在隐藏窗口中验证系统消息路径；无桌面环境不加入默认 ctest。
if(BUILD_TESTING AND (APPLE OR WIN32))
    add_executable(red_mist_window_input_tests EXCLUDE_FROM_ALL tests/window_input_tests.cpp)
    target_link_libraries(red_mist_window_input_tests PRIVATE red_mist_engine)
    if(APPLE)
        target_sources(red_mist_window_input_tests PRIVATE tests/window_input_macos.m)
    else()
        target_sources(red_mist_window_input_tests PRIVATE tests/window_input_win32.cpp)
    endif()
    if(MSVC)
        target_compile_options(red_mist_window_input_tests PRIVATE /utf-8)
        target_sources(red_mist_window_input_tests PRIVATE src/windows_utf8.manifest)
    endif()
endif()
