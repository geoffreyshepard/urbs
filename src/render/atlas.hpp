#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace urbs::render {

// A sprite in the atlas.  u/v/w/h and ax/ay are atlas texels.  The atlas is a stack of pages
// (texture-array layers) of Atlas::height() rows each, and v counts rows through the whole stack:
// page = v / height(), row in page = v % height().  CPU code can index Atlas::pixels() with
// (v + y) * width() + u + x directly; GPU instances pass v through unchanged (the shaders split it).
struct Sprite {
  u16 u = 0, v = 0, w = 0, h = 0;
  i16 ax = 0, ay = 0;     // anchor inside the sprite (texels)
  f32 iso_per_texel = 1;  // iso px per atlas texel (1 / the sprite's packing scale)
  bool valid = false;

  [[nodiscard]] f32 iso_w() const { return static_cast<f32>(w) * iso_per_texel; }
  [[nodiscard]] f32 iso_h() const { return static_cast<f32>(h) * iso_per_texel; }
  [[nodiscard]] f32 iso_ax() const { return static_cast<f32>(ax) * iso_per_texel; }
  [[nodiscard]] f32 iso_ay() const { return static_cast<f32>(ay) * iso_per_texel; }
};

// Texture atlas produced by tools/atlas/pack.py (assets/atlas/main_<page>.png + main.json), uploaded
// as one 2D-array texture with a short mip chain.
class Atlas {
 public:
  // dir: folder holding main.json and main_<page>.png.  Call generate_mips() once the copy pass ends.
  bool load(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const std::string& dir,
            std::vector<SDL_GPUTransferBuffer*>& to_release);
  void generate_mips(SDL_GPUCommandBuffer* cmd) const;
  void release(SDL_GPUDevice* dev);

  // Name lookup is for load time only; resolve sprites once and keep the Sprite.
  [[nodiscard]] Sprite find(const std::string& name) const;
  [[nodiscard]] Sprite get(const std::string& name) const;  // logs once and returns the fallback if missing
  [[nodiscard]] bool has(const std::string& name) const { return sprites_.count(name) != 0; }

  [[nodiscard]] SDL_GPUTexture* texture() const { return tex_; }
  [[nodiscard]] u32 width() const { return w_; }   // page width
  [[nodiscard]] u32 height() const { return h_; }  // page height (rows per page)
  [[nodiscard]] u32 pages() const { return pages_; }
  // Atlas texels per iso px for world sprites (main.json "scale"; world art is packed below its
  // 128 px-per-tile native size).  UI icons (ui/, goods/) keep their own scale; use the Sprite's
  // iso_* helpers, which are right for both.
  [[nodiscard]] f32 scale() const { return scale_; }
  [[nodiscard]] f32 iso_w(const Sprite& s) const { return s.iso_w(); }
  [[nodiscard]] f32 iso_h(const Sprite& s) const { return s.iso_h(); }
  // Average colour of a sprite's opaque pixels (used for far-zoom terrain and minimap).
  [[nodiscard]] u32 average_color(const Sprite& s) const;
  // RGBA8 of every page stacked vertically (width() x height() * pages()).
  [[nodiscard]] const std::vector<u8>& pixels() const { return pixels_; }

 private:
  SDL_GPUTexture* tex_ = nullptr;
  u32 w_ = 0, h_ = 0, pages_ = 0;
  f32 scale_ = 1.0f;
  std::vector<u8> pixels_;
  std::unordered_map<std::string, Sprite> sprites_;
  Sprite fallback_;
};

}  // namespace urbs::render
