# 本文件负责配置游戏引擎、程序、素材工具与测试目标。
cmake_minimum_required(VERSION 3.20)

# 根工程显式启用 C，供原生桥接与 raylib 共用完整的语言配置。
project(koumakyou VERSION 0.1.0 LANGUAGES C CXX)

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

# 通用 UI 可独立链接和测试，不依赖 Game、关卡、配置或素材管理器。
add_library(koumakyou_ui STATIC src/ui/components.cpp src/ui/command_bar.cpp)
target_include_directories(koumakyou_ui PUBLIC src)
target_link_libraries(koumakyou_ui PUBLIC raylib)

# 游戏实现只编译一次，所有目标共用同一份实现。
add_library(koumakyou_engine STATIC
    src/app/game.cpp
    src/app/postgame.cpp
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
    src/gameplay/data/native_combat.cpp
    src/gameplay/bullets/system.cpp
    src/gameplay/combat/combat.cpp
    src/gameplay/combat/damage_rules.cpp
    src/gameplay/effects/actor_effects.cpp
    src/gameplay/effects/external_effects.cpp
    src/gameplay/effects/player_bomb.cpp
    src/gameplay/effects/bomb_volume.cpp
    src/gameplay/items/drop_system.cpp
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

add_executable(koumakyou src/main.cpp)
target_link_libraries(koumakyou PRIVATE koumakyou_engine)
# Windows 每次构建均嵌入 UTF-8 清单，避免重新编译后中文音乐路径再次闪退。
if(MSVC)
    target_sources(koumakyou PRIVATE src/windows_utf8.manifest)
endif()

# Rebuild the six-face strip only when its source artwork needs changing.
add_executable(generate_skybox EXCLUDE_FROM_ALL tools/generate_skybox.cpp)
target_link_libraries(generate_skybox PRIVATE raylib)

add_executable(sprite_benchmark EXCLUDE_FROM_ALL tools/sprite_benchmark.cpp)
target_link_libraries(sprite_benchmark PRIVATE koumakyou_engine)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            $<TARGET_FILE_DIR:koumakyou>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime
            $<TARGET_FILE_DIR:koumakyou>/assets
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
        target_sources(koumakyou_lifecycle_tests PRIVATE src/windows_utf8.manifest)
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
        target_sources(koumakyou_loading_tests PRIVATE src/windows_utf8.manifest)
        target_compile_options(koumakyou_loading_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(koumakyou_loading_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    # 重叠测试用已知 Alpha 颜色读取 GPU 像素，按需运行，不依赖本机原作安装或音频。
    add_executable(koumakyou_sprite_overlap_tests EXCLUDE_FROM_ALL tests/sprite_overlap_tests.cpp)
    target_link_libraries(koumakyou_sprite_overlap_tests PRIVATE koumakyou_engine)
    if(MSVC)
        target_sources(koumakyou_sprite_overlap_tests PRIVATE src/windows_utf8.manifest)
        target_compile_options(koumakyou_sprite_overlap_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(koumakyou_sprite_overlap_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MSVC)
        target_sources(koumakyou_tests PRIVATE src/windows_utf8.manifest)
    endif()
    # assert 在 Release 中仍需执行，否则测试主体会被 NDEBUG 跳过。
    if(MSVC)
        target_compile_options(koumakyou_tests PRIVATE /UNDEBUG /utf-8)
    else()
        target_compile_options(koumakyou_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME koumakyou_core_tests COMMAND koumakyou_tests)
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
        target_sources(koumakyou_settings_ui_tests PRIVATE src/windows_utf8.manifest)
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
        target_sources(koumakyou_window_input_tests PRIVATE src/windows_utf8.manifest)
    endif()
endif()
