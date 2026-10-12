#include "game/ui_picture.hpp"

#include <stb_image.h>

#include <cstring>
#include <vector>

#include "core/types.hpp"

namespace urbs::game {

bool UiPicture::load(SDL_GPUDevice* dev, const std::string& path) {
  release(dev);
  int w = 0, h = 0, comp = 0;
  unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 4);
  if (!px) return false;
  // Mip levels while both sides stay even (a 2560x1440 map gets five: down to 160x90).
  u32 levels = 1;
  while (levels < 6 && (w >> (levels - 1)) % 2 == 0 && (h >> (levels - 1)) % 2 == 0 && (h >> levels) >= 64) ++levels;
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = static_cast<u32>(w);
  ti.height = static_cast<u32>(h);
  ti.layer_count_or_depth = 1;
  ti.num_levels = levels;
  tex_ = SDL_CreateGPUTexture(dev, &ti);
  if (!tex_) {
    stbi_image_free(px);
    return false;
  }
  std::vector<u8> level(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(px);
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xfers;
  u32 lw = static_cast<u32>(w), lh = static_cast<u32>(h);
  for (u32 l = 0; l < levels; ++l) {
    if (l > 0) {  // 2x2 box average of the previous level
      const u32 nw = lw / 2, nh = lh / 2;
      std::vector<u8> next(static_cast<size_t>(nw) * nh * 4);
      for (u32 y = 0; y < nh; ++y)
        for (u32 x = 0; x < nw; ++x)
          for (u32 c = 0; c < 4; ++c) {
            const size_t a = (static_cast<size_t>(y * 2) * lw + x * 2) * 4 + c, b = a + static_cast<size_t>(lw) * 4;
            next[(static_cast<size_t>(y) * nw + x) * 4 + c] =
                static_cast<u8>((level[a] + level[a + 4] + level[b] + level[b + 4] + 2) / 4);
          }
      level = std::move(next);
      lw = nw;
      lh = nh;
    }
    SDL_GPUTransferBufferCreateInfo xi{};
    xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xi.size = lw * lh * 4;
    SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev, &xi);
    std::memcpy(SDL_MapGPUTransferBuffer(dev, xb, false), level.data(), xi.size);
    SDL_UnmapGPUTransferBuffer(dev, xb);
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = xb;
    SDL_GPUTextureRegion dst{};
    dst.texture = tex_;
    dst.mip_level = l;
    dst.w = lw;
    dst.h = lh;
    dst.d = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    xfers.push_back(xb);
  }
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev);
  for (auto* x : xfers) SDL_ReleaseGPUTransferBuffer(dev, x);
  w_ = w;
  h_ = h;
  return true;
}

void UiPicture::release(SDL_GPUDevice* dev) {
  if (tex_) SDL_ReleaseGPUTexture(dev, tex_);
  tex_ = nullptr;
  w_ = h_ = 0;
}

ImTextureRef UiPicture::texture() const {
  return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_)));
}

}  // namespace urbs::game
