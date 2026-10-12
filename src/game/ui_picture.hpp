#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>

namespace urbs::game {

// One large picture for the menus (the campaign's painted province map, assets/ui/): loaded from an
// image file into its own mipmapped texture, outside the sprite atlas, so it can be big and still
// shrink cleanly to whatever size the window gives it.
class UiPicture {
 public:
  // Loads `path` (PNG or JPEG); returns false if the file is missing or unreadable.
  bool load(SDL_GPUDevice* dev, const std::string& path);
  void release(SDL_GPUDevice* dev);
  [[nodiscard]] bool valid() const { return tex_ != nullptr; }
  [[nodiscard]] ImTextureRef texture() const;
  [[nodiscard]] int width() const { return w_; }
  [[nodiscard]] int height() const { return h_; }

 private:
  SDL_GPUTexture* tex_ = nullptr;
  int w_ = 0, h_ = 0;
};

}  // namespace urbs::game
