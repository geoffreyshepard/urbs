#include "game/ui_icons.hpp"

#include <algorithm>
#include <vector>

#include "render/gpu.hpp"

namespace urbs::game {

bool UiIcons::init(SDL_GPUDevice* dev, const render::Atlas& atlas) {
  // Which sprites to include: advisors, gods, goods icons.
  std::vector<std::string> names;
  for (const char* a : {"engineer", "quaestor", "haruspex", "legate", "aedile"}) names.push_back(std::string("ui/advisor_") + a);
  for (const char* g : {"jupiter", "juno", "mars", "venus", "ceres", "neptune", "mercury", "minerva", "vulcan", "vesta", "isis",
                        "serapis", "divus"})
    names.push_back(std::string("ui/god_") + g);
  for (const char* g : {"wheat", "vegetables", "fish", "meat", "bread", "olive_oil", "wine", "pottery", "clothing", "wood",
                        "planks", "bricks", "stone", "tools", "weapons", "armor", "denarii", "furniture", "glassware",
                        "jewelry", "garum", "salt", "marble", "iron", "silver", "clay", "flour", "olives", "grapes"})
    names.push_back(std::string("goods/") + g + "_16");
  for (const char* h : {"population", "happiness", "culture", "prosperity", "security", "favor", "build", "rome", "advisors",
                        "finance", "goods", "trade", "labor", "religion", "military", "research", "alerts", "help", "menu",
                        "pause", "play", "fast", "faster", "road", "demolish", "overlay"})
    names.push_back(std::string("ui/icon_") + h);
  constexpr int kScale = 6, kCell = 32 * kScale, kCols = 8;
  const int rows = static_cast<int>((names.size() + kCols - 1) / kCols);
  const u32 W = kCols * kCell, H = static_cast<u32>(rows) * kCell;
  std::vector<u8> px(static_cast<size_t>(W) * H * 4, 0);
  const auto& src = atlas.pixels();
  for (size_t k = 0; k < names.size(); ++k) {
    const render::Sprite s = atlas.find(names[k]);
    if (!s.valid) continue;
    const int cx = static_cast<int>(k % kCols) * kCell, cy = static_cast<int>(k / kCols) * kCell;
    for (int y = 0; y < s.h * kScale && y < kCell; ++y)
      for (int x = 0; x < s.w * kScale && x < kCell; ++x) {
        const size_t si = (static_cast<size_t>(s.v + y / kScale) * atlas.width() + s.u + x / kScale) * 4;
        const size_t di = (static_cast<size_t>(cy + y) * W + cx + x) * 4;
        for (int c = 0; c < 4; ++c) px[di + c] = src[si + c];
      }
    entries_[names[k]] = Entry{ImVec2(static_cast<float>(cx) / W, static_cast<float>(cy) / H),
                               ImVec2(static_cast<float>(cx + s.w * kScale) / W, static_cast<float>(cy + s.h * kScale) / H)};
  }
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xf;
  tex_ = render::create_texture_rgba(dev, W, H, px.data(), cp, xf);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev);
  for (auto* x : xf) SDL_ReleaseGPUTransferBuffer(dev, x);
  // Big pictures, upscaled x3 into their own texture (stacked vertically).
  {
    const char* bigs[] = {"ui/province_map", "ui/logo"};
    constexpr int kS = 3;
    u32 BW = 0, BH = 0;
    for (const char* n : bigs) {
      const render::Sprite s = atlas.find(n);
      if (!s.valid) continue;
      BW = std::max<u32>(BW, s.w * kS);
      BH += s.h * kS;
    }
    if (BW > 0) {
      std::vector<u8> bp(static_cast<size_t>(BW) * BH * 4, 0);
      u32 oy = 0;
      for (const char* n : bigs) {
        const render::Sprite s = atlas.find(n);
        if (!s.valid) continue;
        for (u32 y = 0; y < static_cast<u32>(s.h) * kS; ++y)
          for (u32 x = 0; x < static_cast<u32>(s.w) * kS; ++x) {
            const size_t si = (static_cast<size_t>(s.v + y / kS) * atlas.width() + s.u + x / kS) * 4;
            const size_t di = (static_cast<size_t>(oy + y) * BW + x) * 4;
            for (int c = 0; c < 4; ++c) bp[di + c] = src[si + c];
          }
        big_entries_[n] = Entry{ImVec2(0, static_cast<float>(oy) / BH),
                                ImVec2(static_cast<float>(s.w * kS) / BW, static_cast<float>(oy + s.h * kS) / BH)};
        oy += s.h * kS;
      }
      SDL_GPUCommandBuffer* c2 = SDL_AcquireGPUCommandBuffer(dev);
      SDL_GPUCopyPass* p2 = SDL_BeginGPUCopyPass(c2);
      std::vector<SDL_GPUTransferBuffer*> x2;
      big_ = render::create_texture_rgba(dev, BW, BH, bp.data(), p2, x2);
      SDL_EndGPUCopyPass(p2);
      SDL_SubmitGPUCommandBuffer(c2);
      SDL_WaitForGPUIdle(dev);
      for (auto* x : x2) SDL_ReleaseGPUTransferBuffer(dev, x);
    }
  }
  return tex_ != nullptr;
}

bool UiIcons::picture(const std::string& name, ImVec2 size) const {
  auto it = big_entries_.find(name);
  if (it == big_entries_.end() || !big_) return false;
  ImGui::Image(big_texture(), size, it->second.uv0, it->second.uv1);
  return true;
}

ImTextureRef UiIcons::big_texture() const { return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(big_))); }

bool UiIcons::big_uv(const std::string& name, ImVec2& uv0, ImVec2& uv1) const {
  auto it = big_entries_.find(name);
  if (it == big_entries_.end()) return false;
  uv0 = it->second.uv0;
  uv1 = it->second.uv1;
  return true;
}

void UiIcons::shutdown(SDL_GPUDevice* dev) {
  if (tex_) SDL_ReleaseGPUTexture(dev, tex_);
  if (big_) SDL_ReleaseGPUTexture(dev, big_);
  tex_ = big_ = nullptr;
}

bool UiIcons::image(const std::string& name, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !tex_) return false;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))), ImVec2(size, size), it->second.uv0,
               it->second.uv1);
  return true;
}

}  // namespace urbs::game
