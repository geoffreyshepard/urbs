# Shader toolchain: one GLSL source per stage (shaders/*.vert, *.frag)
#   glslang -> SPIR-V (Vulkan backend)
#   SPIRV-Cross -> MSL (Metal backend), using SPIR-V binding numbers as MSL indices
# Both blobs are embedded as byte arrays in a generated header per shader.
#
# Binding conventions (match SDL_gpu.h):
#   vertex:   set 0 = textures/storage, set 1 = uniform buffers
#   fragment: set 2 = textures/storage, set 3 = uniform buffers
# Rule: never mix uniform buffers and storage buffers in the same stage (MSL index collision).
# Per-instance data goes through instance-rate vertex buffers.

CPMAddPackage(NAME glslang GITHUB_REPOSITORY KhronosGroup/glslang GIT_TAG vulkan-sdk-1.4.363.0
  OPTIONS "ENABLE_OPT OFF" "GLSLANG_TESTS OFF" "ENABLE_GLSLANG_BINARIES ON" "ENABLE_HLSL OFF"
          "ENABLE_SPVREMAPPER OFF" "GLSLANG_ENABLE_INSTALL OFF" "BUILD_EXTERNAL OFF" "ENABLE_CTEST OFF")
CPMAddPackage(NAME spirv_cross GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross GIT_TAG vulkan-sdk-1.4.363.0
  OPTIONS "SPIRV_CROSS_CLI ON" "SPIRV_CROSS_ENABLE_TESTS OFF" "SPIRV_CROSS_SKIP_INSTALL ON"
          "SPIRV_CROSS_ENABLE_C_API OFF"
          )

function(urbs_add_shaders target)
  file(GLOB _shaders CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)
  set(_outdir ${CMAKE_BINARY_DIR}/generated/shaders)
  file(MAKE_DIRECTORY ${_outdir})
  set(_headers "")
  foreach(src ${_shaders})
    get_filename_component(_name ${src} NAME)
    string(REPLACE "." "_" _ident ${_name})
    set(_spv ${_outdir}/${_name}.spv)
    set(_msl ${_outdir}/${_name}.msl)
    set(_hdr ${_outdir}/${_ident}.h)
    add_custom_command(
      OUTPUT ${_hdr}
      COMMAND $<TARGET_FILE:glslang-standalone> -V --quiet -o ${_spv} ${src}
      COMMAND $<TARGET_FILE:spirv-cross> ${_spv} --msl --msl-version 20100 --msl-decoration-binding --output ${_msl}
      COMMAND ${CMAKE_COMMAND} -DSPV=${_spv} -DMSL=${_msl} -DOUT=${_hdr} -DIDENT=${_ident}
              -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake
      DEPENDS ${src} glslang-standalone spirv-cross ${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake
      COMMENT "Compiling shader ${_name}")
    list(APPEND _headers ${_hdr})
  endforeach()
  add_custom_target(${target}_shaders DEPENDS ${_headers})
  add_dependencies(${target} ${target}_shaders)
  target_include_directories(${target} PUBLIC ${CMAKE_BINARY_DIR}/generated)
endfunction()
