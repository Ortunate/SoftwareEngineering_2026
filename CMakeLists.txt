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
    src/game/settings.cpp
    src/game/stage_system.cpp
    src/game/game.cpp
)

target_include_directories(red_mist_demo PRIVATE src)
target_link_libraries(red_mist_demo PRIVATE raylib)

if(MSVC)
    target_compile_options(red_mist_demo PRIVATE /W4 /permissive-)
else()
    target_compile_options(red_mist_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(red_mist_tests
        tests/core_tests.cpp
        src/game/bullet_system.cpp
        src/game/settings.cpp
    )
    target_include_directories(red_mist_tests PRIVATE src)
    target_link_libraries(red_mist_tests PRIVATE raylib)
    add_test(NAME red_mist_core_tests COMMAND red_mist_tests)
endif()
