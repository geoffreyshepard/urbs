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
  // Big images (province map, logo): drawn at an explicit size. Returns false if missing.
  bool picture(const std::string& name, ImVec2 size) const;
  [[nodiscard]] ImTextureRef big_texture() const;
  [[nodiscard]] bool big_uv(const std::string& name, ImVec2& uv0, ImVec2& uv1) const;

 private:
  struct Entry {
    ImVec2 uv0, uv1;
  };
  SDL_GPUTexture* tex_ = nullptr;
  SDL_GPUTexture* big_ = nullptr;
  std::unordered_map<std::string, Entry> entries_, big_entries_;
};

}  // namespace urbs::game
