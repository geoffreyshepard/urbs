# Every dependency is pinned to an exact version. Bump deliberately, one per commit.

# --- Pure-sim dependencies (no SDL) -------------------------------------------------

CPMAddPackage(NAME EnTT GITHUB_REPOSITORY skypjack/entt GIT_TAG v3.16.0 DOWNLOAD_ONLY YES)
add_library(entt INTERFACE)
target_include_directories(entt SYSTEM INTERFACE ${EnTT_SOURCE_DIR}/src)

CPMAddPackage(NAME enkiTS GITHUB_REPOSITORY dougbinks/enkiTS GIT_TAG v1.12
  OPTIONS "ENKITS_BUILD_EXAMPLES OFF" "ENKITS_BUILD_C_INTERFACE OFF" "ENKITS_INSTALL OFF")

CPMAddPackage(NAME nlohmann_json GITHUB_REPOSITORY nlohmann/json GIT_TAG v3.12.0
  OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF")

CPMAddPackage(NAME spdlog GITHUB_REPOSITORY gabime/spdlog GIT_TAG v1.17.0
  OPTIONS "SPDLOG_BUILD_EXAMPLE OFF" "SPDLOG_INSTALL OFF")

CPMAddPackage(NAME doctest GITHUB_REPOSITORY doctest/doctest GIT_TAG v2.5.3 DOWNLOAD_ONLY YES)
add_library(doctest_lib INTERFACE)
target_include_directories(doctest_lib SYSTEM INTERFACE ${doctest_SOURCE_DIR})

CPMAddPackage(NAME zstd GITHUB_REPOSITORY facebook/zstd GIT_TAG v1.5.7 SOURCE_SUBDIR build/cmake
  OPTIONS "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_SHARED OFF" "ZSTD_BUILD_STATIC ON"
          "ZSTD_BUILD_TESTS OFF" "ZSTD_LEGACY_SUPPORT OFF")

CPMAddPackage(NAME lua GITHUB_REPOSITORY lua/lua GIT_TAG v5.4.9 DOWNLOAD_ONLY YES)
file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/(lua|luac|onelua|ltests)\\.c$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static SYSTEM PUBLIC ${lua_SOURCE_DIR})
if(UNIX AND NOT APPLE)
  target_compile_definitions(lua_static PRIVATE LUA_USE_LINUX)
elseif(APPLE)
  target_compile_definitions(lua_static PRIVATE LUA_USE_MACOSX)
endif()
set_target_properties(lua_static PROPERTIES LINKER_LANGUAGE C)

CPMAddPackage(NAME sol2 GITHUB_REPOSITORY ThePhD/sol2 GIT_TAG v3.5.0 DOWNLOAD_ONLY YES)
add_library(sol2_lib INTERFACE)
target_include_directories(sol2_lib SYSTEM INTERFACE ${sol2_SOURCE_DIR}/include)
target_link_libraries(sol2_lib INTERFACE lua_static)
target_compile_definitions(sol2_lib INTERFACE SOL_ALL_SAFETIES_ON=1)

CPMAddPackage(NAME stb GITHUB_REPOSITORY nothings/stb GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20 DOWNLOAD_ONLY YES)
add_library(stb_lib INTERFACE)
target_include_directories(stb_lib SYSTEM INTERFACE ${stb_SOURCE_DIR})

CPMAddPackage(NAME tracy GITHUB_REPOSITORY wolfpld/tracy GIT_TAG v0.14.1
  OPTIONS "TRACY_ENABLE ${URBS_TRACY}" "TRACY_ON_DEMAND ON" "TRACY_STATIC ON")

# --- Presentation dependencies (SDL) ----------------------------------------------

if(URBS_BUILD_GAME)
  CPMAddPackage(NAME SDL3 GITHUB_REPOSITORY libsdl-org/SDL GIT_TAG release-3.4.18
    OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_EXAMPLES OFF"
            "SDL_TESTS OFF" "SDL_INSTALL OFF")

  CPMAddPackage(NAME imgui GITHUB_REPOSITORY ocornut/imgui GIT_TAG v1.92.9 DOWNLOAD_ONLY YES)
  add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
  target_include_directories(imgui_lib SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui_lib PUBLIC SDL3::SDL3-static)
endif()
