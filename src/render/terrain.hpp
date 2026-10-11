#pragma once
#include <vector>

#include "render/atlas.hpp"
#include "render/gpu.hpp"
#include "sim/map.hpp"

namespace urbs::render {

inline constexpr int kTerrainCount = static_cast<int>(sim::Terrain::Count);

// Sprites the static terrain layer needs, resolved once from the atlas.
struct TerrainSprites {
  Sprite base[kTerrainCount][4];
  // Dual-grid overlays by corner mask (bit NW=1, NE=2, SW=4, SE=8; 1..14 used), see build_chunk.
  Sprite dual[kTerrainCount][16];
  Sprite hcliff[16];  // height-step contours by mask of corners on the higher level (1..14)
  Sprite road_center[3];   // indexed by sim::Road (None unused)
  Sprite road_spoke[3][8];  // N, NE, E, SE, S, SW, W, NW
  Sprite road_fill[3][4];   // ne, se, sw, nw: drawn on a non-road tile whose two side neighbours join diagonally
  Sprite tree[6], bush[3], reeds[3], boulder[3];
  Sprite res[static_cast<int>(sim::Resource::Count)];
  u32 base_color[kTerrainCount] = {};
  u32 road_color[3] = {};

  void resolve(const Atlas& atlas);
};

// Blend priority: higher draws its edges over lower.
int terrain_priority(sim::Terrain t);

// Cliff faces draw just above the terrain so neighbouring dual cells never cover them.
inline constexpr f32 kDepthCliff = 0.9988f;

// Depth for y-sorted sprites (buildings, trees, agents) from the world-px y of their foot.
inline f32 depth_for_foot(f32 foot_y, f32 map_px_h) { return 0.9f - 0.8f * (foot_y / (map_px_h + 64.0f)); }
inline constexpr f32 kDepthTerrain = 0.999f;
inline constexpr f32 kDepthRoad = 0.998f;
static_assert(kDepthCliff > (0.999f + kDepthRoad) * 0.5f, "cliffs must still count as terrain in ground.frag");
// Cast shadows sit between the ground layers and the farthest y-sorted sprite (depth_for_foot <= 0.9).
// Instances at or beyond kDepthShadow are ground (never cast); at or below kDepthOverlayMax they are
// screen overlays such as smoke, rain and selection marks (never cast either).
inline constexpr f32 kDepthShadow = 0.95f;
inline constexpr f32 kDepthOverlayMax = 0.08f;

// Appends all static instances (terrain, blends, cliffs, roads, decorations) of one chunk.
void build_chunk(const sim::TileMap& map, const TerrainSprites& ts, int cx, int cy, std::vector<SpriteInstance>& out);

// Ground texture texels (RGBA8, one per tile; see shaders/ground.frag for the channels) for the tile
// rect [x0, x1) x [y0, y1), row-major into out (resized to the rect).
void build_ground_texels(const sim::TileMap& map, i32 x0, i32 y0, i32 x1, i32 y1, std::vector<u32>& out);

// One RGBA8 pixel per tile for far zoom levels / minimap.
void build_tile_colors(const sim::TileMap& map, const TerrainSprites& ts, std::vector<u32>& out);

}  // namespace urbs::render
