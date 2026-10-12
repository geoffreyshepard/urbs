#pragma once
// Sprites shipped by mods (docs/10-modding.md, "Sprites").  A mod's sprites/ folder holds PNGs named
// like atlas sprites (sprites/bld/my_temple.png -> "bld/my_temple"), rendered at native iso scale
// (128 x 64 px per tile diamond, docs/13-isometric.md), with optional anchors in sprites/sprites.json
// (or sprites/meta/*.json, as tools/artgen3d writes them).  At load time the enabled mods' PNGs are
// decoded, resampled to the atlas scale (world art; icons stay 1:1) and shelf-packed into extra atlas
// pages; Atlas::set_mod_sprites uploads them and merge_mod_sprites() decides which names win.
//
// No SDL and no GPU here, so the packing and the override rules are unit-tested (tests/test_mod_sprites.cpp).
// Load time only: this allocates freely and never runs inside the tick.

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/types.hpp"
#include "render/sprite.hpp"

namespace urbs::render {

inline constexpr int kModSpriteMaxPx = 2048;                 // largest world sprite PNG, either side (native px)
inline constexpr int kModIconMaxPx = 1024;                   // largest ui/ or goods/ icon PNG, either side
inline constexpr int kIsoHalfTilePx = 64;                    // a footprint's canvas is (fw + fh) * 64 px wide
inline constexpr long long kModSpriteMaxBytes = 32ll << 20;  // a bigger PNG file is refused unread

struct ModSpriteConfig {
  u32 page_w = 4096, page_h = 2048;  // must match the base atlas pages (one texture array)
  u32 first_page = 0;                // index of the first extra page (= Atlas::base_pages())
  u32 max_pages = 4;                 // extra pages at most (32 MB each at 4096 x 2048)
  f32 world_scale = 1.0f;            // atlas texels per iso px for world art (Atlas::scale())
  // Building sprite base names (BuildingDef::sprite, e.g. "bld/vintner") -> unrotated footprint
  // (fw, fh): bld/ sprites of these buildings get their anchor and expected width from it.
  std::unordered_map<std::string, std::pair<int, int>> footprints;
};

struct ModSpriteLayer {
  std::string mod;                                      // the mod's folder
  std::vector<std::pair<std::string, Sprite>> sprites;  // name -> sprite, sorted by name
};

struct ModSpritePack {
  u32 page_w = 0, page_h = 0;
  u32 pages = 0;                       // extra pages used
  std::vector<u8> pixels;              // RGBA8, pages stacked: pages * page_h rows of page_w texels
  std::vector<ModSpriteLayer> layers;  // one per mod with a sprites/ folder, in load order
  std::vector<std::string> warnings;   // one line per refused file (also logged)
  [[nodiscard]] size_t count() const;
};

// Decodes and packs the sprites of `mod_dirs` (mod folders, in load order).  Bad files (unreadable,
// not a PNG, too big, the wrong width for their footprint) are skipped with a warning, so the base
// sprite or the grey placeholder is drawn instead.  Never throws.
ModSpritePack pack_mod_sprites(const std::vector<std::string>& mod_dirs, const ModSpriteConfig& cfg);

// Applies the pack's layers, in load order, to a name -> sprite table (the base atlas's).  A mod
// sprite replaces the sprite of the same name.  A bld/ sprite also hides the earlier versions of the
// same art that the mod does not ship itself, so a replaced building never shows the old art when
// turned, at night or in a province: bld/<x> hides bld/<x>_v<n>, _r<k>, _lit and @<region> (and
// their combinations); bld/<x>@east hides bld/<x>_r1@east, and so on.  Tiers (_t<k>) are separate art.
void merge_mod_sprites(std::unordered_map<std::string, Sprite>& table, const ModSpritePack& pack);

// The footprint (fw, fh, rotation applied) a bld/ sprite name stands for, from `footprints`
// (bld/<x>[_t<k>][_v<n>][_r<k>][_lit][@region]).  False for names of no known building.
bool mod_sprite_footprint(const std::string& name,
                          const std::unordered_map<std::string, std::pair<int, int>>& footprints, int& fw, int& fh);

}  // namespace urbs::render
