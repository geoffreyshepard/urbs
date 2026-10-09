#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>
#include <unordered_map>

#include "render/atlas.hpp"

namespace urbs::game {

// Crisp pixel-art images for ImGui: selected atlas sprites upscaled (nearest) into one texture,
// because the ImGui backend samples linearly.
class UiIcons {
 public:
  bool init(SDL_GPUDevice* dev, const render::Atlas& atlas);
  void shutdown(SDL_GPUDevice* dev);
  // Draws the named sprite at `size` points; returns false if missing.
  bool image(const std::string& name, float size) const;

 private:
  struct Entry {
    ImVec2 uv0, uv1;
  };
  SDL_GPUTexture* tex_ = nullptr;
  std::unordered_map<std::string, Entry> entries_;
};

}  // namespace urbs::game
