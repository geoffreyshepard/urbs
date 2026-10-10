#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>
#include <unordered_map>

#include "data/defs.hpp"
#include "render/atlas.hpp"

namespace urbs::game {

// Crisp pixel-art images for ImGui: selected atlas sprites upscaled (nearest) into one texture,
// because the ImGui backend samples linearly.
class UiIcons {
 public:
  bool init(SDL_GPUDevice* dev, const render::Atlas& atlas, const data::Defs& defs);
  void shutdown(SDL_GPUDevice* dev);
  // Draws the named sprite at `size` points; returns false if missing.
  bool image(const std::string& name, float size) const;
  // Big images (province map, logo): drawn at an explicit size. Returns false if missing.
  bool picture(const std::string& name, ImVec2 size) const;
  [[nodiscard]] ImTextureRef big_texture() const;
  // Build-menu pictures, one per building def (keyed by def id) plus the tools "tool:dirt", "tool:paved" and
  // "tool:demolish": each sprite (or a small field / line of its pieces) fitted into a square cell.
  // `region` picks regional variants (e.g. "egypt"). Rebuilt when the defs or the city's region change.
  void build_thumbs(SDL_GPUDevice* dev, const render::Atlas& atlas, const data::Defs& defs, const std::string& region);
  // Draws a thumbnail into the current window's draw list, filling the square at `p0` of side `size`.
  bool draw_thumb(const std::string& key, ImVec2 p0, float size) const;
  // Draws a cached icon (e.g. "goods/denarii_16") into the current window's draw list.
  bool draw_icon(const std::string& name, ImVec2 p0, float size) const;
  // An image button with a category icon; `on` keeps it highlighted.
  bool icon_button(const char* id, const std::string& name, float size, bool on) const;
  [[nodiscard]] bool big_uv(const std::string& name, ImVec2& uv0, ImVec2& uv1) const;

 private:
  struct Entry {
    ImVec2 uv0, uv1;
  };
  SDL_GPUTexture* tex_ = nullptr;
  SDL_GPUTexture* big_ = nullptr;
  // The previous thumbnail texture is kept one rebuild longer: a draw list built this frame may still use it.
  SDL_GPUTexture* thumbs_ = nullptr;
  SDL_GPUTexture* thumbs_old_ = nullptr;
  std::unordered_map<std::string, Entry> entries_, big_entries_, thumb_entries_;
};

}  // namespace urbs::game
