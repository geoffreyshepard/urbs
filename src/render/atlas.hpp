#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace urbs::render {

struct Sprite {
  u16 u = 0, v = 0, w = 0, h = 0;
  i16 ax = 0, ay = 0;  // anchor pixel inside the sprite
  bool valid = false;
};

// Texture atlas produced by tools/atlas/pack.py (assets/atlas/main.png + main.json).
class Atlas {
 public:
  bool load(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const std::string& png_path, const std::string& json_path,
            std::vector<SDL_GPUTransferBuffer*>& to_release);
  void release(SDL_GPUDevice* dev);

  // Name lookup is for load time only; resolve sprites once and keep the Sprite.
  [[nodiscard]] Sprite find(const std::string& name) const;
  [[nodiscard]] Sprite get(const std::string& name) const;  // logs once and returns the fallback if missing
  [[nodiscard]] bool has(const std::string& name) const { return sprites_.count(name) != 0; }

  [[nodiscard]] SDL_GPUTexture* texture() const { return tex_; }
  [[nodiscard]] u32 width() const { return w_; }
  [[nodiscard]] u32 height() const { return h_; }
  // Average colour of a sprite's opaque pixels (used for far-zoom terrain and minimap).
  [[nodiscard]] u32 average_color(const Sprite& s) const;

 private:
  SDL_GPUTexture* tex_ = nullptr;
  u32 w_ = 0, h_ = 0;
  std::vector<u8> pixels_;
  std::unordered_map<std::string, Sprite> sprites_;
  Sprite fallback_;
};

}  // namespace urbs::render
