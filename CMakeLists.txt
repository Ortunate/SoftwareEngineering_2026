# 本文件负责配置游戏引擎、程序、素材工具与测试目标。
cmake_minimum_required(VERSION 3.20)

# 根工程显式启用 C，供原生桥接与 raylib 共用完整的语言配置。
project(koumakyou VERSION 0.1.0 LANGUAGES C CXX)

# MinGW 通过资源脚本嵌入进程清单，不能只把 .manifest 列为源码。
if(WIN32 AND NOT MSVC)
    enable_language(RC)
endif()

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

# 第三方公开头文件按系统头传递，避免 raymath.h 的 C 风格初始化在每个 C++ 使用方重复报警。
# 仅改变 raylib 头文件的诊断边界；项目源码仍使用原有警告选项，兼容 CMake 3.20。
get_target_property(koumakyou_raylib_include_dirs raylib INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(raylib SYSTEM INTERFACE ${koumakyou_raylib_include_dirs})

# 通用 UI 可独立链接和测试，不依赖 Game、关卡、配置或素材管理器。
add_library(koumakyou_ui STATIC src/ui/components.cpp src/ui/command_bar.cpp src/ui/name_entry.cpp src/ui/difficulty_card.cpp)
target_include_directories(koumakyou_ui PUBLIC src)
target_link_libraries(koumakyou_ui PUBLIC raylib)

# 游戏实现只编译一次，所有目标共用同一份实现。
add_library(koumakyou_engine STATIC
    src/app/game.cpp
    src/app/postgame.cpp
    src/app/replay.cpp
    src/app/replay_controls.cpp
    src/replay/replay.cpp
    src/app/input.cpp
    src/app/start_setup.cpp
    src/app/spell_practice.cpp
    src/app/spell_practice_menu.cpp
    src/render/spell_practice_render.cpp
    src/config/settings.cpp
    src/config/high_scores.cpp
    src/narrative/ending.cpp
    src/gameplay/actors/boss.cpp
    src/gameplay/bosses/configured_boss.cpp
    src/gameplay/bosses/rumia_behaviors.cpp
    src/gameplay/bosses/daiyousei_behaviors.cpp
    src/gameplay/bosses/cirno_behaviors.cpp
    src/gameplay/bosses/meiling_behaviors.cpp
    src/gameplay/bosses/koakuma_behaviors.cpp
    src/gameplay/bosses/patchouli_behaviors.cpp
    src/gameplay/bosses/patchouli_extra_behaviors.cpp
    src/gameplay/bosses/flandre.cpp
    src/gameplay/bosses/flandre_behaviors.cpp
    src/gameplay/bosses/flandre_cranberry.cpp
    src/gameplay/bosses/flandre_laevatein.cpp
    src/gameplay/bosses/flandre_four_of_a_kind.cpp
    src/gameplay/bosses/flandre_kagome.cpp
    src/gameplay/bosses/flandre_maze_of_love.cpp
    src/gameplay/bosses/flandre_starbow_break.cpp
    src/gameplay/bosses/flandre_catadioptric.cpp
    src/gameplay/bosses/flandre_ripples.cpp
    src/gameplay/bosses/flandre_danmaku_heart.cpp
    src/gameplay/bosses/flandre_creeping_bloodstain.cpp
    src/gameplay/bosses/flandre_fragile_wing.cpp
    src/gameplay/bosses/flandre_encore_entrance.cpp
    src/gameplay/bosses/flandre_and_then_will_there_be_none.cpp
    src/gameplay/bosses/flandre_clock_of_the_past.cpp
    src/gameplay/bosses/sakuya_behaviors.cpp
    src/gameplay/bosses/remilia_behaviors.cpp
    src/gameplay/bosses/sakuya_eternal_meek.cpp
    src/gameplay/bosses/rumia.cpp
    src/gameplay/bosses/daiyousei.cpp
    src/gameplay/bosses/cirno.cpp
    src/gameplay/bosses/meiling.cpp
    src/gameplay/bosses/koakuma.cpp
    src/gameplay/bosses/patchouli.cpp
    src/gameplay/bosses/sakuya.cpp
    src/gameplay/bosses/remilia.cpp
    src/gameplay/actors/player.cpp
    src/gameplay/actors/aim.cpp
    src/gameplay/data/native_combat.cpp
    src/gameplay/bullets/system.cpp
    src/gameplay/combat/combat.cpp
    src/gameplay/combat/damage_rules.cpp
    src/gameplay/effects/actor_effects.cpp
    src/gameplay/effects/external_effects.cpp
    src/gameplay/effects/player_bomb.cpp
    src/gameplay/effects/bomb_volume.cpp
    src/gameplay/items/drop_system.cpp
    src/platform/file_publish.cpp
    src/platform/window_input.cpp
    src/render/menu_render.cpp
    src/render/start_setup_render.cpp
    src/render/game_render.cpp
    src/render/postgame_render.cpp
    src/render/command_bar.cpp
    src/render/gameplay_hud.cpp
    src/render/player_bomb_render.cpp
    src/render/bomb_geometry.cpp
    src/render/sprite_renderer.cpp
    src/render/animated_sprite.cpp
    src/resources/asset_manager.cpp
    src/resources/animation.cpp
    src/resources/battle_catalog.cpp
    src/resources/dds_image.cpp
    src/stage/catalog.cpp
    src/stage/content.cpp
    src/stage/system.cpp
    src/gameplay/actors/movement.cpp
    src/gameplay/actors/skill.cpp
    src/gameplay/actors/behavior.cpp
    src/stages/common/assets.cpp
    src/stages/common/content.cpp
    src/stages/stage2/behaviors.cpp
    src/stages/stage2/boss_config.cpp
    src/stages/stage2/content.cpp
    src/stages/stage2/dialogue.cpp
    src/stages/stage2/manifest.cpp
    src/stages/stage2/scenery.cpp
    src/stages/stage2/timeline.cpp
    src/stages/stage3/behaviors.cpp
    src/stages/stage3/boss_config.cpp
    src/stages/stage3/content.cpp
    src/stages/stage3/dialogue.cpp
    src/stages/stage3/manifest.cpp
    src/stages/stage3/scenery.cpp
    src/stages/stage3/timeline.cpp
    src/stages/stage4/behaviors.cpp
    src/stages/stage4/boss_config.cpp
    src/stages/stage4/content.cpp
    src/stages/stage4/dialogue.cpp
    src/stages/stage4/manifest.cpp
    src/stages/stage4/scenery.cpp
    src/stages/stage4/timeline.cpp
    src/stages/stage5/behaviors.cpp
    src/stages/stage5/boss_config.cpp
    src/stages/stage5/content.cpp
    src/stages/stage5/scenery.cpp
    src/stages/stage5/dialogue.cpp
    src/stages/stage5/manifest.cpp
    src/stages/stage5/timeline.cpp
    src/stages/stage6/behaviors.cpp
    src/stages/stage6/boss_config.cpp
    src/stages/stage6/content.cpp
    src/stages/stage6/scenery.cpp
    src/stages/stage6/timeline.cpp
    src/stages/stage6/dialogue.cpp
    src/stages/stage6/manifest.cpp
    src/stages/stage_extra/behaviors.cpp
    src/stages/stage_extra/boss_config.cpp
    src/stages/stage_extra/manifest.cpp
    src/stages/stage_extra/dialogue.cpp
    src/stages/stage_extra/timeline.cpp
    src/stages/stage_extra/scenery.cpp
    src/stages/stage_extra/content.cpp
    src/stages/stage1/behaviors.cpp
    src/stages/stage1/boss_config.cpp
    src/stages/stage1/content.cpp
    src/stages/stage1/dialogue.cpp
    src/stages/stage1/scenery.cpp
    src/stages/stage1/manifest.cpp
    src/stages/stage1/timeline.cpp
)
target_include_directories(koumakyou_engine PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(koumakyou_engine PUBLIC koumakyou_ui raylib Threads::Threads)
if(APPLE)
    target_sources(koumakyou_engine PRIVATE src/platform/window_input_macos.m)
    target_link_libraries(koumakyou_engine PRIVATE "-framework AppKit")
elseif(WIN32)
    target_link_libraries(koumakyou_engine PRIVATE imm32)
endif()
# Game 的条件成员需要所有使用方采用相同的类布局。
target_compile_definitions(koumakyou_engine PUBLIC
    $<$<CONFIG:Debug>:KOUMAKYOU_DEBUG_FEATURES>
)


target_sources(koumakyou_engine PRIVATE
 src/render/projectiles/option_renderer.cpp
 src/render/projectiles/bomb_renderer.cpp
 src/render/combat_renderer.cpp
 src/resources/projectile_assets.cpp
 src/gameplay/collision/collision.cpp
 src/render/projectiles/classic_renderer.cpp
 src/render/projectiles/collection_renderer.cpp
 src/render/projectiles/laser_renderer.cpp
 src/render/projectiles/spawn_renderer.cpp)

add_executable(koumakyou src/main.cpp)
target_link_libraries(koumakyou PRIVATE koumakyou_engine)

# Rebuild the six-face strip only when its source artwork needs changing.
add_executable(generate_skybox EXCLUDE_FROM_ALL tools/generate_skybox.cpp)
target_link_libraries(generate_skybox PRIVATE raylib)

add_executable(sprite_benchmark EXCLUDE_FROM_ALL tools/sprite_benchmark.cpp)
target_link_libraries(sprite_benchmark PRIVATE koumakyou_engine)

# 正式模型与透明队列的窗口压测仅按需运行，不进入普通 CTest。
if(UNIX)
    add_executable(render_benchmark EXCLUDE_FROM_ALL tools/render_benchmark.cpp)
    target_link_libraries(render_benchmark PRIVATE koumakyou_engine)
endif()

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            $<TARGET_FILE_DIR:koumakyou>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime
            $<TARGET_FILE_DIR:koumakyou>/assets
    COMMAND ${CMAKE_COMMAND} -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_ROOT=$<TARGET_FILE_DIR:koumakyou>/assets -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyProjectileAssets.cmake
    VERBATIM
)
add_dependencies(koumakyou copy_assets)

# 全目录图形审阅工具按需构建，直接复用原始素材缓存与 ANM 播放/绘制接口。
add_executable(native_asset_preview EXCLUDE_FROM_ALL tools/native_asset_preview.cpp)
target_link_libraries(native_asset_preview PRIVATE koumakyou_engine)
add_dependencies(native_asset_preview copy_assets)

# 立绘导入仅在变更素材时运行；使用生产 DDS/ANM 解码器，不依赖窗口或原作安装路径。
add_executable(export_portrait_sprites EXCLUDE_FROM_ALL tools/export_portrait_sprites.cpp)
target_link_libraries(export_portrait_sprites PRIVATE koumakyou_engine)
# 锁定提示验收复用真实世界/HUD 渲染；输出 PNG 供 GIF 编码，断言在 Release 中也启用。
add_executable(aim_lock_preview EXCLUDE_FROM_ALL tools/aim_lock_preview.cpp)
target_link_libraries(aim_lock_preview PRIVATE koumakyou_engine)
add_dependencies(aim_lock_preview copy_assets)
if(MSVC)
    target_compile_options(aim_lock_preview PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(aim_lock_preview PRIVATE -UNDEBUG)
endif()


if(MSVC)
    target_compile_options(koumakyou_ui PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(koumakyou_engine PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(koumakyou PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(koumakyou_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(koumakyou_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(koumakyou PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(koumakyou_postgame_tests tests/ending_tests.cpp)
    target_link_libraries(koumakyou_postgame_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_compile_options(koumakyou_postgame_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_postgame_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME koumakyou_postgame_tests COMMAND koumakyou_postgame_tests)
    # 真实结局贴图及键鼠命名回归需桌面图形会话，按需执行，不加入无窗口 CTest。
    add_executable(koumakyou_postgame_lifecycle_tests EXCLUDE_FROM_ALL tests/postgame_lifecycle_tests.cpp)
    target_link_libraries(koumakyou_postgame_lifecycle_tests PRIVATE koumakyou_engine)
    target_include_directories(koumakyou_postgame_lifecycle_tests PRIVATE "${raylib_SOURCE_DIR}/external/glfw/include")
    add_dependencies(koumakyou_postgame_lifecycle_tests copy_assets)
    if(MSVC)
        target_compile_options(koumakyou_postgame_lifecycle_tests PRIVATE /UNDEBUG /utf-8)
        target_sources(koumakyou_postgame_lifecycle_tests PRIVATE src/windows_utf8.manifest)
    else()
        target_compile_options(koumakyou_postgame_lifecycle_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
    # 模块测试仅链接 UI 与 raylib，不加载游戏、素材、音频或图形窗口。
    add_executable(koumakyou_ui_tests tests/ui_tests.cpp)
    target_link_libraries(koumakyou_ui_tests PRIVATE koumakyou_ui)
    if(MSVC)
        target_compile_options(koumakyou_ui_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_ui_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME koumakyou_ui_tests COMMAND koumakyou_ui_tests)
    # 旧 ECL 解释器只作为独立差分基准，不能重新加入生产引擎。
    add_executable(koumakyou_tests
        tests/reference_script.cpp
        tests/behavior_tests.cpp
        tests/spatial_behavior_tests.cpp
        tests/core_tests.cpp
        tests/animation_tests.cpp
        tests/external_effect_tests.cpp
        tests/attached_effect_tests.cpp
        tests/actor_effect_tests.cpp
        tests/drop_tests.cpp
        tests/gameplay_tests.cpp
        tests/player_tests.cpp
        tests/marisa_tests.cpp
        tests/native_combat_tests.cpp
        tests/player_bomb_tests.cpp
        tests/settings_tests.cpp
        tests/stage_tests.cpp)
    target_sources(koumakyou_tests PRIVATE tests/stage_extra_dialogue_tests.cpp)
    target_sources(koumakyou_tests PRIVATE tests/stage_extra_timeline_tests.cpp)
    target_sources(koumakyou_tests PRIVATE tests/stage2_tests.cpp tests/stage3_tests.cpp tests/stage3_spell_reference.cpp tests/stage4_tests.cpp tests/stage4_combat_reference.cpp tests/stage5_tests.cpp tests/stage5_combat_reference.cpp tests/stage6_tests.cpp tests/stage6_combat_reference.cpp tests/stage_extra_tests.cpp tests/stage_extra_patchouli_tests.cpp tests/stage_extra_flandre_tests.cpp tests/stage_extra_kagome_tests.cpp tests/stage_extra_maze_tests.cpp tests/stage_extra_starbow_tests.cpp tests/stage_extra_catadioptric_tests.cpp tests/stage_extra_clock_tests.cpp tests/stage_extra_survival_tests.cpp tests/stage_extra_ripples_tests.cpp tests/stage_extra_encore_entrance_tests.cpp tests/stage_extra_fragile_wing_tests.cpp tests/stage_extra_bloodstain_tests.cpp tests/stage_extra_heart_tests.cpp)
    target_link_libraries(koumakyou_tests PRIVATE koumakyou_engine)
    target_compile_definitions(koumakyou_tests PRIVATE KOUMAKYOU_ASSET_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime")
    # 离屏生命周期测试仍需要图形会话，手动运行，避免普通无窗口测试依赖桌面服务。
    add_executable(koumakyou_lifecycle_tests EXCLUDE_FROM_ALL tests/lifecycle_tests.cpp)
    target_link_libraries(koumakyou_lifecycle_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_compile_options(koumakyou_lifecycle_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_lifecycle_tests PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic)
    endif()
    add_dependencies(koumakyou_lifecycle_tests copy_assets)
    # 加载测试需要图形上下文，验证实际异步任务和绘制帧，不纳入无窗口 CTest。
    add_executable(koumakyou_loading_tests EXCLUDE_FROM_ALL tests/loading_tests.cpp)
    target_link_libraries(koumakyou_loading_tests PRIVATE koumakyou_engine)
    add_dependencies(koumakyou_loading_tests copy_assets)
    if(MSVC)
        target_compile_options(koumakyou_loading_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(koumakyou_loading_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # 重叠测试用已知 Alpha 颜色读取 GPU 像素，按需运行，不依赖本机原作安装或音频。
    add_executable(koumakyou_sprite_overlap_tests EXCLUDE_FROM_ALL tests/sprite_overlap_tests.cpp)
    target_link_libraries(koumakyou_sprite_overlap_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_compile_options(koumakyou_sprite_overlap_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(koumakyou_sprite_overlap_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # assert 在 Release 中仍需执行，否则测试主体会被 NDEBUG 跳过。
    if(MSVC)
        target_compile_options(koumakyou_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME koumakyou_core_tests COMMAND koumakyou_tests)
    add_test(NAME koumakyou_start_setup_tests COMMAND koumakyou_tests --start-setup-only)
    # 参考导入与结构审阅使用独立合成数据；仅测试期使用 Python，不成为游戏运行依赖。
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME koumakyou_reference_tool_tests
            COMMAND ${Python3_EXECUTABLE} -B -m unittest
                tests/test_native_combat_data.py
                tests/test_native_difficulty.py
                tests/test_native_rank_events.py
                tests/test_native_effects.py
                tests/test_extra_flow.py
                tests/test_extra_dialogue.py
                tests/test_stage_script_inspection.py
                tests/test_stage1_reference_import.py)
        set_tests_properties(koumakyou_reference_tool_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()

# 隐藏窗口交互测试需 OpenGL/音频环境，按需构建，不影响无图形环境的核心测试。
if(BUILD_TESTING)
    add_executable(koumakyou_settings_ui_tests EXCLUDE_FROM_ALL tests/settings_ui_tests.cpp)
    target_link_libraries(koumakyou_settings_ui_tests PRIVATE koumakyou_engine)
    add_dependencies(koumakyou_settings_ui_tests copy_assets)
    if(MSVC)
        target_compile_options(koumakyou_settings_ui_tests PRIVATE /utf-8)
    endif()
endif()

# 原生输入测试在隐藏窗口中验证系统消息路径；无桌面环境不加入默认 ctest。
if(BUILD_TESTING AND (APPLE OR WIN32))
    add_executable(koumakyou_window_input_tests EXCLUDE_FROM_ALL tests/window_input_tests.cpp)
    target_link_libraries(koumakyou_window_input_tests PRIVATE koumakyou_engine)
    if(APPLE)
        target_sources(koumakyou_window_input_tests PRIVATE tests/window_input_macos.m)
    else()
        target_sources(koumakyou_window_input_tests PRIVATE tests/window_input_win32.cpp)
    endif()
    if(MSVC)
        target_compile_options(koumakyou_window_input_tests PRIVATE /utf-8)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(projectile_direction_tests tests/projectile_direction_tests.cpp)
    target_include_directories(projectile_direction_tests PRIVATE src)
    target_link_libraries(projectile_direction_tests PRIVATE koumakyou_engine)
    add_test(NAME projectile_direction_tests COMMAND projectile_direction_tests)
    add_executable(combat_feedback_tests tests/combat_feedback_tests.cpp)
    target_link_libraries(combat_feedback_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_compile_options(combat_feedback_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(combat_feedback_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME combat_feedback_tests COMMAND combat_feedback_tests)
    # 录像的纯 CPU 格式与可播放范围单独验证，不依赖图形窗口或本机录像目录。
    add_executable(koumakyou_replay_tests tests/replay_tests.cpp)
    target_link_libraries(koumakyou_replay_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_compile_options(koumakyou_replay_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_replay_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME koumakyou_replay_tests COMMAND koumakyou_replay_tests)
endif()

if(BUILD_TESTING)
    # 需要图形上下文，仅手动运行功能集成检查，不执行性能测量。
    add_executable(projectile_runtime_tests EXCLUDE_FROM_ALL tests/projectile_runtime_tests.cpp)
    target_link_libraries(projectile_runtime_tests PRIVATE koumakyou_engine)
endif()

# 真实音频设备回归按需运行；静音验证中文文件名和 MP3 流，不依赖图形窗口。
if(BUILD_TESTING)
    add_executable(koumakyou_music_tests EXCLUDE_FROM_ALL tests/music_tests.cpp)
    target_link_libraries(koumakyou_music_tests PRIVATE koumakyou_engine)
    add_dependencies(koumakyou_music_tests copy_assets)
    if(MSVC)
        target_compile_options(koumakyou_music_tests PRIVATE /utf-8)
    endif()
endif()

# 所有本目录的 Windows 程序均使用 UTF-8，包括工具与测试；第三方库不受影响。
# MSVC 由链接器合并清单，MinGW 则必须编译 RT_MANIFEST 资源，否则中文 BGM 会静默缺失。
if(WIN32)
    get_property(koumakyou_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(koumakyou_target IN LISTS koumakyou_targets)
        get_target_property(koumakyou_target_type ${koumakyou_target} TYPE)
        if(koumakyou_target_type STREQUAL "EXECUTABLE")
            if(MSVC)
                target_sources(${koumakyou_target} PRIVATE src/windows_utf8.manifest)
            else()
                target_sources(${koumakyou_target} PRIVATE src/windows_utf8.rc)
                target_include_directories(${koumakyou_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
            endif()
        endif()
    endforeach()
endif()

# 兼容标识覆盖所有运行时代码和资源；任何源码改动触发重新配置，不允许旧录像静默使用新逻辑。
file(GLOB_RECURSE REPLAY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*" "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/*")
# 模型实体已参与碰撞，导出几何和配置也属于录像内容；编辑器工程及审阅输出不参与签名。
file(GLOB_RECURSE REPLAY_MODEL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/projectiles/*.gltf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/projectiles/*.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/projectiles/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/items/*.gltf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/items/*.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/items/*.json")
list(APPEND REPLAY_SOURCES ${REPLAY_MODEL_SOURCES})
set(REPLAY_SIGNATURE "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}-${CMAKE_CXX_FLAGS}-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}")
foreach(REPLAY_SOURCE IN LISTS REPLAY_SOURCES)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPLAY_SOURCE}")
    file(SHA256 "${REPLAY_SOURCE}" REPLAY_SOURCE_HASH)
    string(APPEND REPLAY_SIGNATURE "${REPLAY_SOURCE_HASH}")
endforeach()
string(SHA256 REPLAY_BUILD_HASH "${REPLAY_SIGNATURE}")
target_compile_definitions(koumakyou_engine PRIVATE KOUMAKYOU_REPLAY_BUILD="${REPLAY_BUILD_HASH}-$<CONFIG>")
# 模拟兼容版本不依赖观战控件和渲染代码；内容清单单独签名，构建标识仅展示。
set(REPLAY_SIM_SIGNATURE "v2-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}-${CMAKE_CXX_FLAGS}")
set(REPLAY_CONTENT_SIGNATURE "v2")
foreach(REPLAY_SOURCE IN LISTS REPLAY_SOURCES)
    file(SHA256 "${REPLAY_SOURCE}" REPLAY_SOURCE_HASH)
    if(REPLAY_SOURCE MATCHES "/(gameplay|stage|stages)/" OR REPLAY_SOURCE MATCHES "/app/(game|input|replay)[.](cpp|hpp)$" OR REPLAY_SOURCE MATCHES "/resources/(animation|battle_catalog)[.](cpp|hpp)$")
        string(APPEND REPLAY_SIM_SIGNATURE "${REPLAY_SOURCE_HASH}")
    endif()
    if(REPLAY_SOURCE MATCHES "/assets/(runtime|projectiles|items)/")
        string(APPEND REPLAY_CONTENT_SIGNATURE "${REPLAY_SOURCE_HASH}")
    endif()
endforeach()
string(SHA256 REPLAY_SIM_HASH "${REPLAY_SIM_SIGNATURE}")
string(SHA256 REPLAY_CONTENT_HASH "${REPLAY_CONTENT_SIGNATURE}")
target_compile_definitions(koumakyou_engine PRIVATE KOUMAKYOU_REPLAY_SIMULATION="${REPLAY_SIM_HASH}-$<CONFIG>" KOUMAKYOU_REPLAY_CONTENT="${REPLAY_CONTENT_HASH}")
