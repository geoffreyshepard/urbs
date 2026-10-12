#include "render/atlas.hpp"

#include <stb_image.h>

#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/log.hpp"
#include "render/gpu.hpp"
#include "render/mod_sprites.hpp"

namespace urbs::render {

namespace {
constexpr u32 kMipLevels = 3;  // base, 1/2, 1/4: enough for the zoomed-out levels above far zoom

SDL_GPUTexture* create_array(SDL_GPUDevice* dev, u32 w, u32 h, u32 pages) {
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;  // colour target: mip generation
  ti.width = w;
  ti.height = h;
  ti.layer_count_or_depth = pages;
  ti.num_levels = kMipLevels;
  SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev, &ti);
  if (!tex) URBS_LOG_ERROR("atlas: texture creation failed: {}", SDL_GetError());
  return tex;
}

void upload_page(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, SDL_GPUTexture* tex, u32 layer, const u8* px, u32 w, u32 h,
                 std::vector<SDL_GPUTransferBuffer*>& to_release) {
  const size_t page_bytes = static_cast<size_t>(w) * h * 4;
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xi.size = static_cast<u32>(page_bytes);
  SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev, &xi);
  std::memcpy(SDL_MapGPUTransferBuffer(dev, xb, false), px, page_bytes);
  SDL_UnmapGPUTransferBuffer(dev, xb);
  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = xb;
  SDL_GPUTextureRegion dst{};
  dst.texture = tex;
  dst.layer = layer;
  dst.w = w;
  dst.h = h;
  dst.d = 1;
  SDL_UploadToGPUTexture(pass, &src, &dst, false);
  to_release.push_back(xb);
}
}  // namespace

bool Atlas::load(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const std::string& dir,
                 std::vector<SDL_GPUTransferBuffer*>& to_release) {
  const std::string json_path = dir + "main.json";
  std::ifstream f(json_path);
  if (!f) {
    URBS_LOG_ERROR("atlas: cannot open {}", json_path);
    return false;
  }
  nlohmann::json j = nlohmann::json::parse(f);
  w_ = j.at("page_width").get<u32>();
  h_ = j.at("page_height").get<u32>();
  pages_ = j.at("pages").get<u32>();
  scale_ = j.value("scale", 1.0f);
  if (pages_ == 0 || static_cast<u64>(pages_) * h_ > 65536) {
    URBS_LOG_ERROR("atlas: {} pages of {} rows do not fit a u16 v", pages_, h_);
    return false;
  }
  const size_t page_bytes = static_cast<size_t>(w_) * h_ * 4;
  pixels_.assign(page_bytes * pages_, 0);
  for (u32 p = 0; p < pages_; ++p) {
    const std::string png = dir + "main_" + std::to_string(p) + ".png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(png.c_str(), &w, &h, &comp, 4);
    if (!px || static_cast<u32>(w) != w_ || static_cast<u32>(h) != h_) {
      URBS_LOG_ERROR("atlas: cannot load {} ({}x{})", png, w, h);
      if (px) stbi_image_free(px);
      return false;
    }
    std::memcpy(pixels_.data() + page_bytes * p, px, page_bytes);
    stbi_image_free(px);
  }

  for (auto& [name, s] : j.at("sprites").items()) {
    Sprite sp;
    sp.u = s.at("x").get<u16>();
    sp.v = static_cast<u16>(s.at("page").get<u32>() * h_ + s.at("y").get<u32>());
    sp.w = s.at("w").get<u16>();
    sp.h = s.at("h").get<u16>();
    sp.ax = s.value("ax", i16{0});
    sp.ay = s.value("ay", i16{0});
    const f32 sc = s.value("s", scale_);
    sp.iso_per_texel = sc > 0 ? 1.0f / sc : 1.0f;
    sp.valid = true;
    sprites_[name] = sp;
  }
  fallback_ = find("fx/white");
  fallback_.valid = false;

  base_pages_ = pages_;
  base_sprites_ = sprites_;
  tex_ = create_array(dev, w_, h_, pages_);
  if (!tex_) return false;
  for (u32 p = 0; p < pages_; ++p) upload_page(dev, pass, tex_, p, pixels_.data() + page_bytes * p, w_, h_, to_release);
  URBS_LOG_INFO("atlas: {} sprites, {} page(s) of {}x{}, scale {}", sprites_.size(), pages_, w_, h_, scale_);
  return true;
}

void Atlas::set_mod_sprites(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const ModSpritePack& pack,
                            std::vector<SDL_GPUTransferBuffer*>& to_release) {
  ++revision_;
  sprites_ = base_sprites_;
  const u32 mod_pages = pack.page_w == w_ && pack.page_h == h_ ? pack.pages : 0;
  if (mod_pages != pack.pages)
    URBS_LOG_ERROR("atlas: mod sprite pages are {}x{}, not {}x{}", pack.page_w, pack.page_h, w_, h_);
  else
    merge_mod_sprites(sprites_, pack);
  if (mod_pages == 0 && pages_ == base_pages_) return;  // no extra pages before or after: the texture stays
  SDL_GPUTexture* tex = create_array(dev, w_, h_, base_pages_ + mod_pages);
  if (!tex) {  // keep the old texture, drawn with the base sprites only
    sprites_ = base_sprites_;
    return;
  }
  const size_t page_bytes = static_cast<size_t>(w_) * h_ * 4;
  pixels_.resize(page_bytes * base_pages_);
  pixels_.insert(pixels_.end(), pack.pixels.begin(),
                 pack.pixels.begin() + static_cast<std::ptrdiff_t>(page_bytes * mod_pages));
  for (u32 p = 0; p < base_pages_; ++p) {  // base pages: copied on the GPU, mip 0 (the mips are regenerated)
    SDL_GPUTextureLocation from{};
    from.texture = tex_;
    from.layer = p;
    SDL_GPUTextureLocation to{};
    to.texture = tex;
    to.layer = p;
    SDL_CopyGPUTextureToTexture(pass, &from, &to, w_, h_, 1, false);
  }
  for (u32 p = 0; p < mod_pages; ++p)
    upload_page(dev, pass, tex, base_pages_ + p, pixels_.data() + page_bytes * (base_pages_ + p), w_, h_, to_release);
  SDL_ReleaseGPUTexture(dev, tex_);  // freed once the copies above are done
  tex_ = tex;
  pages_ = base_pages_ + mod_pages;
}

void Atlas::generate_mips(SDL_GPUCommandBuffer* cmd) const {
  if (tex_) SDL_GenerateMipmapsForGPUTexture(cmd, tex_);
}

void Atlas::release(SDL_GPUDevice* dev) {
  if (tex_) SDL_ReleaseGPUTexture(dev, tex_);
  tex_ = nullptr;
}

Sprite Atlas::find(const std::string& name) const {
  auto it = sprites_.find(name);
  return it == sprites_.end() ? Sprite{} : it->second;
}

Sprite Atlas::get(const std::string& name) const {
  auto it = sprites_.find(name);
  if (it != sprites_.end()) return it->second;
  URBS_LOG_WARN("atlas: missing sprite '{}'", name);
  return fallback_;
}

u32 Atlas::average_color(const Sprite& s) const {
  u64 r = 0, g = 0, b = 0, n = 0;
  for (u32 y = s.v; y < static_cast<u32>(s.v + s.h); ++y)
    for (u32 x = s.u; x < static_cast<u32>(s.u + s.w); ++x) {
      const u8* p = &pixels_[(static_cast<size_t>(y) * w_ + x) * 4];
      if (p[3] < 128) continue;
      r += p[0];
      g += p[1];
      b += p[2];
      ++n;
    }
  if (n == 0) return rgba(255, 0, 255);
  return rgba(static_cast<u8>(r / n), static_cast<u8>(g / n), static_cast<u8>(b / n));
}

}  // namespace urbs::render
