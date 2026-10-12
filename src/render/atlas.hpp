#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"
#include "render/sprite.hpp"

namespace urbs::render {

struct ModSpritePack;

// Texture atlas produced by tools/atlas/pack.py (assets/atlas/main_<page>.png + main.json), uploaded
// as one 2D-array texture with a short mip chain.  Enabled mods' sprites (render/mod_sprites.hpp)
// go into extra pages after the base ones and override base sprites by name.
class Atlas {
 public:
  // dir: folder holding main.json and main_<page>.png.  Call generate_mips() once the copy pass ends.
  bool load(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const std::string& dir,
            std::vector<SDL_GPUTransferBuffer*>& to_release);
  // Replaces the mod pages and the mods' sprite overrides (an empty pack drops them).  Load time only:
  // when the page count changes the texture is recreated (base pages copied on the GPU), so call
  // generate_mips() once the copy pass ends.  Base sprites keep their u/v.
  void set_mod_sprites(SDL_GPUDevice* dev, SDL_GPUCopyPass* pass, const ModSpritePack& pack,
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
  [[nodiscard]] u32 base_pages() const { return base_pages_; }  // pages from assets/atlas (mod pages follow)
  // Bumped whenever the mod sprites change: code caching Sprites resolves them again when it moves.
  [[nodiscard]] u32 revision() const { return revision_; }
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
  u32 w_ = 0, h_ = 0, pages_ = 0, base_pages_ = 0, revision_ = 0;
  f32 scale_ = 1.0f;
  std::vector<u8> pixels_;
  std::unordered_map<std::string, Sprite> sprites_;
  std::unordered_map<std::string, Sprite> base_sprites_;  // main.json alone, before any mod
  Sprite fallback_;
};

}  // namespace urbs::render
