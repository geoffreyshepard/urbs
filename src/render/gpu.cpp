#include "render/gpu.hpp"

#include <cstring>

#include "core/log.hpp"

namespace urbs::render {

SDL_GPUShader* load_shader(SDL_GPUDevice* dev, SDL_GPUShaderStage stage, const unsigned char* spv, u32 spv_size,
                           const unsigned char* msl, u32 msl_size, u32 samplers, u32 uniform_buffers) {
  SDL_GPUShaderCreateInfo ci{};
  ci.stage = stage;
  ci.num_samplers = samplers;
  ci.num_uniform_buffers = uniform_buffers;
  const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(dev);
  if (formats & SDL_GPU_SHADERFORMAT_MSL) {
    ci.format = SDL_GPU_SHADERFORMAT_MSL;
    ci.code = msl;
    ci.code_size = msl_size;  // MSL source is passed as text
    ci.entrypoint = "main0";
  } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.code = spv;
    ci.code_size = spv_size;
    ci.entrypoint = "main";
  } else {
    URBS_LOG_ERROR("no supported shader format");
    return nullptr;
  }
  SDL_GPUShader* s = SDL_CreateGPUShader(dev, &ci);
  if (!s) URBS_LOG_ERROR("SDL_CreateGPUShader failed: {}", SDL_GetError());
  return s;
}

SDL_GPUTexture* create_texture_rgba(SDL_GPUDevice* dev, u32 w, u32 h, const void* pixels, SDL_GPUCopyPass* pass,
                                    std::vector<SDL_GPUTransferBuffer*>& to_release) {
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = w;
  ti.height = h;
  ti.layer_count_or_depth = 1;
  ti.num_levels = 1;
  SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev, &ti);
  if (!tex || !pixels) return tex;
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xi.size = w * h * 4;
  SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev, &xi);
  void* map = SDL_MapGPUTransferBuffer(dev, xb, false);
  std::memcpy(map, pixels, xi.size);
  SDL_UnmapGPUTransferBuffer(dev, xb);
  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = xb;
  SDL_GPUTextureRegion dst{};
  dst.texture = tex;
  dst.w = w;
  dst.h = h;
  dst.d = 1;
  SDL_UploadToGPUTexture(pass, &src, &dst, false);
  to_release.push_back(xb);
  return tex;
}

void DynamicBuffer::ensure(SDL_GPUDevice* dev, u32 bytes) {
  if (bytes <= capacity && buf) return;
  release(dev);
  capacity = 64 * 1024;
  while (capacity < bytes) capacity *= 2;
  SDL_GPUBufferCreateInfo bi{};
  bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  bi.size = capacity;
  buf = SDL_CreateGPUBuffer(dev, &bi);
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xi.size = capacity;
  xfer = SDL_CreateGPUTransferBuffer(dev, &xi);
}

void DynamicBuffer::upload(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const void* data, u32 bytes) {
  if (bytes == 0) return;
  ensure(dev, bytes);
  void* map = SDL_MapGPUTransferBuffer(dev, xfer, true);
  std::memcpy(map, data, bytes);
  SDL_UnmapGPUTransferBuffer(dev, xfer);
  SDL_GPUTransferBufferLocation src{xfer, 0};
  SDL_GPUBufferRegion dst{buf, 0, bytes};
  SDL_UploadToGPUBuffer(pass, &src, &dst, true);
}

void DynamicBuffer::release(SDL_GPUDevice* dev) {
  if (buf) SDL_ReleaseGPUBuffer(dev, buf);
  if (xfer) SDL_ReleaseGPUTransferBuffer(dev, xfer);
  buf = nullptr;
  xfer = nullptr;
  capacity = 0;
}

}  // namespace urbs::render
