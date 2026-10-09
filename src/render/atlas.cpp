#include "render/atlas.hpp"

#include <stb_image.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "core/log.hpp"
#include "render/gpu.hpp"

namespace urbs::render {

bool Atlas::load(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const std::string& png_path, const std::string& json_path,
                 std::vector<SDL_GPUTransferBuffer*>& to_release) {
  int w = 0, h = 0, comp = 0;
  unsigned char* px = stbi_load(png_path.c_str(), &w, &h, &comp, 4);
  if (!px) {
    URBS_LOG_ERROR("atlas: cannot load {}", png_path);
    return false;
  }
  w_ = static_cast<u32>(w);
  h_ = static_cast<u32>(h);
  pixels_.assign(px, px + static_cast<size_t>(w * h * 4));
  stbi_image_free(px);

  std::ifstream f(json_path);
  if (!f) {
    URBS_LOG_ERROR("atlas: cannot open {}", json_path);
    return false;
  }
  nlohmann::json j = nlohmann::json::parse(f);
  for (auto& [name, s] : j.at("sprites").items()) {
    Sprite sp;
    sp.u = s.at("x").get<u16>();
    sp.v = s.at("y").get<u16>();
    sp.w = s.at("w").get<u16>();
    sp.h = s.at("h").get<u16>();
    sp.ax = s.value("ax", i16{0});
    sp.ay = s.value("ay", i16{0});
    sp.valid = true;
    sprites_[name] = sp;
  }
  fallback_ = find("fx/white");
  fallback_.valid = false;
  tex_ = create_texture_rgba(dev, w_, h_, pixels_.data(), pass, to_release);
  URBS_LOG_INFO("atlas: {} sprites, {}x{}", sprites_.size(), w_, h_);
  return tex_ != nullptr;
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
