cmake_minimum_required(VERSION 3.20)
project(swieta2026 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(EMSCRIPTEN)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

option(SWIETA_BUILD_APP "Build SDL3 + OpenGL application" ON)
option(SWIETA_BUILD_TESTS "Build unit and automated game tests" ON)
option(SWIETA_ENABLE_COVERAGE "Enable gcov coverage instrumentation" OFF)
option(SWIETA_ENABLE_SANITIZERS "Enable ASan/UBSan on tests" OFF)

# Pure deterministic C11 core library (zero SDL/OpenGL dependency -> >=95% unit test coverage)
add_library(swieta_core STATIC
    src/core/math3d.c
    src/core/calendar.c
    src/render/voxel_mesher.c
    src/net/net_proto.c
    src/platform/touch.c
    src/audio/retro_audio.c
    src/sim/world.c
)
target_include_directories(swieta_core PUBLIC src)
if(NOT MSVC)
    target_compile_options(swieta_core PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(swieta_core PUBLIC m)
else()
    target_compile_options(swieta_core PRIVATE /W4)
endif()

if(SWIETA_ENABLE_COVERAGE AND NOT MSVC)
    target_compile_options(swieta_core PUBLIC --coverage -O0 -g)
    target_link_options(swieta_core PUBLIC --coverage)
endif()

if(SWIETA_ENABLE_SANITIZERS AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(swieta_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(swieta_core PUBLIC -fsanitize=address,undefined)
endif()

# Full SDL3 + OpenGL / WebGL2 + WebXR Application
if(SWIETA_BUILD_APP)
    include(FetchContent)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    if(EMSCRIPTEN)
        set(SDL_PTHREADS OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.20
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    add_executable(swieta2026
        src/render/gl_loader.c
        src/render/gl_renderer.c
        src/platform/sdl_main.c
    )
    target_link_libraries(swieta2026 PRIVATE swieta_core SDL3::SDL3-static)
    if(NOT MSVC)
        target_compile_options(swieta2026 PRIVATE -Wall -Wextra -Werror)
    endif()

    if(EMSCRIPTEN)
        set_target_properties(swieta2026 PROPERTIES SUFFIX ".js")
        target_link_options(swieta2026 PRIVATE
            "-sUSE_WEBGL2=1"
            "-sFULL_ES3=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=33554432"
            "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_swieta_set_split_screen','_swieta_set_unlock_all','_swieta_set_retro_res','_swieta_resize','_swieta_toggle_touch','_swieta_jump_to_day','_swieta_net_set_role','_swieta_net_on_packet','_swieta_xr_begin_frame','_swieta_xr_render_eye']"
            "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','HEAPF32','UTF8ToString','stringToUTF8']"
        )
        add_custom_command(TARGET swieta2026 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/web/index.html $<TARGET_FILE_DIR:swieta2026>/index.html
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/web/webrtc_bridge.js $<TARGET_FILE_DIR:swieta2026>/webrtc_bridge.js
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/web/webxr_bridge.js $<TARGET_FILE_DIR:swieta2026>/webxr_bridge.js
        )
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(swieta2026 PRIVATE OpenGL::GL)
    endif()
endif()

# Unit Tests & Automated Headless Game Tests
if(SWIETA_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()

    add_executable(test_unit tests/unit/test_core.c)
    target_link_libraries(test_unit PRIVATE swieta_core)
    if(NOT MSVC)
        target_compile_options(test_unit PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME unit_tests COMMAND test_unit)

    add_executable(test_game tests/game/test_game_playthrough.c)
    target_link_libraries(test_game PRIVATE swieta_core)
    if(NOT MSVC)
        target_compile_options(test_game PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME automated_game_tests COMMAND test_game)
endif()
