#include "game/ui_icons.hpp"

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
  return tex_ != nullptr;
}

void UiIcons::shutdown(SDL_GPUDevice* dev) {
  if (tex_) SDL_ReleaseGPUTexture(dev, tex_);
  tex_ = nullptr;
}

bool UiIcons::image(const std::string& name, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !tex_) return false;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))), ImVec2(size, size), it->second.uv0,
               it->second.uv1);
  return true;
}

}  // namespace urbs::game
