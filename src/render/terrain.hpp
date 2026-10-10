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
  Sprite edge[kTerrainCount][4];    // n, e, s, w
  Sprite corner[kTerrainCount][4];  // ne, se, sw, nw
  Sprite cliff_s, cliff_e, cliff_w;
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

// Depth for y-sorted sprites (buildings, trees, agents) from the world-px y of their foot.
inline f32 depth_for_foot(f32 foot_y, f32 map_px_h) { return 0.9f - 0.8f * (foot_y / (map_px_h + 64.0f)); }
inline constexpr f32 kDepthTerrain = 0.999f;
inline constexpr f32 kDepthRoad = 0.998f;
// Cast shadows sit between the ground layers and the farthest y-sorted sprite (depth_for_foot <= 0.9).
// Instances at or beyond kDepthShadow are ground (never cast); at or below kDepthOverlayMax they are
// screen overlays such as smoke, rain and selection marks (never cast either).
inline constexpr f32 kDepthShadow = 0.95f;
inline constexpr f32 kDepthOverlayMax = 0.08f;

// Appends all static instances (terrain, blends, cliffs, roads, decorations) of one chunk.
void build_chunk(const sim::TileMap& map, const TerrainSprites& ts, int cx, int cy, std::vector<SpriteInstance>& out);

// One RGBA8 pixel per tile for far zoom levels / minimap.
void build_tile_colors(const sim::TileMap& map, const TerrainSprites& ts, std::vector<u32>& out);

}  // namespace urbs::render
