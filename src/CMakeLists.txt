set(URBS_SRC ${CMAKE_CURRENT_SOURCE_DIR})

# --- urbs_core: no SDL --------------------------------------------------------------
file(GLOB URBS_CORE_SOURCES CONFIGURE_DEPENDS core/*.cpp)
list(FILTER URBS_CORE_SOURCES EXCLUDE REGEX ".*/alloc_hook\\.cpp$")
add_library(urbs_core STATIC ${URBS_CORE_SOURCES})
target_include_directories(urbs_core PUBLIC ${URBS_SRC})
target_link_libraries(urbs_core PUBLIC spdlog::spdlog TracyClient)
urbs_warnings(urbs_core)

# Global operator new hook, linked only into executables.
add_library(urbs_alloc_hook OBJECT)
if(NOT MSVC)
  target_sources(urbs_alloc_hook PRIVATE core/alloc_hook.cpp)
else()
  target_sources(urbs_alloc_hook PRIVATE core/alloc_hook_stub.cpp)
endif()
target_link_libraries(urbs_alloc_hook PRIVATE urbs_core)

# --- urbs_sim: all simulation modules, no SDL ---------------------------------------
file(GLOB_RECURSE URBS_SIM_SOURCES CONFIGURE_DEPENDS
  sim/*.cpp path/*.cpp econ/*.cpp society/*.cpp military/*.cpp script/*.cpp data/*.cpp save/*.cpp)
add_library(urbs_sim STATIC ${URBS_SIM_SOURCES})
target_link_libraries(urbs_sim PUBLIC urbs_core entt enkiTS nlohmann_json::nlohmann_json sol2_lib libzstd_static)
urbs_warnings(urbs_sim)

# --- urbs_headless ------------------------------------------------------------------
add_executable(urbs_headless app/headless_main.cpp)
target_link_libraries(urbs_headless PRIVATE urbs_sim urbs_alloc_hook)
urbs_warnings(urbs_headless)

# Guard: the sim must never depend on SDL.
foreach(lib urbs_core urbs_sim)
  get_target_property(_deps ${lib} LINK_LIBRARIES)
  foreach(d ${_deps})
    if(d MATCHES "SDL")
      message(FATAL_ERROR "${lib} must not link SDL (found ${d})")
    endif()
  endforeach()
endforeach()

if(URBS_BUILD_GAME)
  include(Shaders)
  file(GLOB_RECURSE URBS_RENDER_SOURCES CONFIGURE_DEPENDS render/*.cpp platform/*.cpp)
  add_library(urbs_render STATIC ${URBS_RENDER_SOURCES})
  target_link_libraries(urbs_render PUBLIC urbs_sim SDL3::SDL3-static imgui_lib stb_lib)
  urbs_add_shaders(urbs_render)
  urbs_warnings(urbs_render)

  file(GLOB_RECURSE URBS_UI_SOURCES CONFIGURE_DEPENDS ui/*.cpp game/*.cpp)
  add_library(urbs_ui STATIC ${URBS_UI_SOURCES})
  target_link_libraries(urbs_ui PUBLIC urbs_render)
  urbs_warnings(urbs_ui)

  add_executable(urbs MACOSX_BUNDLE app/game_main.cpp)
  target_link_libraries(urbs PRIVATE urbs_ui urbs_alloc_hook)
  set_target_properties(urbs PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Urbs"
                                        MACOSX_BUNDLE_GUI_IDENTIFIER "org.urbs.game")
  target_compile_definitions(urbs PRIVATE URBS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  urbs_warnings(urbs)
endif()

target_compile_definitions(urbs_headless PRIVATE URBS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
