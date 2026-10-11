#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "core/types.hpp"

namespace urbs::render {

// Per-instance sprite data. Matches shaders/sprite.vert. 32 bytes.
struct SpriteInstance {
  f32 x, y, z;    // iso px (top-left), depth (0 near .. 1 far)
  f32 w, h;       // iso px size
  u16 u, v, uw, vh;  // atlas texel rect (v runs through the stacked pages, see render::Sprite)
  u32 rgba;       // tint, little-endian RGBA8 (0xAABBGGRR)
};
static_assert(sizeof(SpriteInstance) == 32);

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
  return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
}
inline constexpr u32 kWhite = 0xFFFFFFFFu;

struct CameraUniform {
  f32 origin[2];
  f32 scale[2];
  f32 screen[2];
  f32 tex_inv[2];
  f32 page[4];  // x: rows per atlas page
};

SDL_GPUShader* load_shader(SDL_GPUDevice* dev, SDL_GPUShaderStage stage, const unsigned char* spv, u32 spv_size,
                           const unsigned char* msl, u32 msl_size, u32 samplers, u32 uniform_buffers);

SDL_GPUTexture* create_texture_rgba(SDL_GPUDevice* dev, u32 w, u32 h, const void* pixels, SDL_GPUCopyPass* pass,
                                    std::vector<SDL_GPUTransferBuffer*>& to_release);

// A GPU vertex buffer that grows; data is uploaded through a cycled transfer buffer each time.
struct DynamicBuffer {
  SDL_GPUBuffer* buf = nullptr;
  SDL_GPUTransferBuffer* xfer = nullptr;
  u32 capacity = 0;  // bytes
  void ensure(SDL_GPUDevice* dev, u32 bytes);
  void upload(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const void* data, u32 bytes);
  void release(SDL_GPUDevice* dev);
};

}  // namespace urbs::render
