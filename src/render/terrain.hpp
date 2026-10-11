#pragma once
#include <vector>

#include "render/atlas.hpp"
#include "render/gpu.hpp"
#include "sim/map.hpp"

namespace urbs::render {

inline constexpr int kTerrainCount = static_cast<int>(sim::Terrain::Count);

// Sprites the static terrain layer needs, resolved once from the atlas (art: tools/artgen3d/
// terrain_tex.py and terrain.py).  Ground textures repeat every two tiles; [p] is the position
// class (x & 1) + 2 * (y & 1) of the dual cell.
struct TerrainSprites {
  Sprite base[kTerrainCount][4];
  // Dual-grid overlays by corner mask (bit NW=1, NE=2, SW=4, SE=8; 1..14 used), see build_chunk.
  Sprite dual[kTerrainCount][16][4];
  // One quarter (0 NW, 1 NE, 2 SW, 3 SE) of a dual cell, for cells across a height step.
  Sprite quarter[kTerrainCount][4][4];
  Sprite cliff[2][2];       // [south, east][variant]: one height level of face under that edge
  Sprite road_center[3];    // indexed by sim::Road (None unused)
  Sprite road_spoke[3][8];  // N, NE, E, SE, S, SW, W, NW
  Sprite road_fill[3][4];   // ne, se, sw, nw: drawn on a non-road tile whose two side neighbours join diagonally
  struct Decor {
    Sprite body, shadow;  // the object, and the shadow it casts (drawn under every body)
  };
  Decor tree[7], bush[3], reeds[3], boulder[3];
  Decor res[static_cast<int>(sim::Resource::Count)];
  u32 base_color[kTerrainCount] = {};
  u32 road_color[3] = {};

  void resolve(const Atlas& atlas);
};

// Blend priority: higher draws its edges over lower.
int terrain_priority(sim::Terrain t);

// Depth bands (0 near .. 1 far).
//  - Ground layer (static chunks): kDepthGroundNear .. kDepthGroundFar.  Within it each instance
//    gets a slot from its tile diagonal (x + y) and a sub-layer, so raised ground in front covers
//    what lies behind it no matter which chunk draws first (see ground_depth).
//  - kDepthFlat: ground-attached dynamic sprites (farm and plaza tiles, foundations) just above
//    every ground layer.
//  - y-sorted sprites (buildings, trees, agents): iso::depth_for_iso, 0.1 .. 0.9.
inline constexpr f32 kDepthGroundNear = 0.9982f;
inline constexpr f32 kDepthGroundFar = 0.99996f;
inline constexpr f32 kDepthFlat = 0.9976f;
inline constexpr f32 kDepthDecorShadow = 0.9979f;  // baked tree/rock shadows: over ground, under kDepthFlat
inline constexpr f32 kDepthTerrain = 0.999f;  // far-zoom diamond (drawn alone)
inline constexpr f32 kDepthRoad = 0.998f;     // legacy: kept so kDepthRoad - small offsets stay above ground
inline constexpr int kGroundSlots = 1024;     // > map w + h + 2 for every supported map size
// Sub-layers inside one ground slot, back to front.
enum GroundLayer : int { kLayerCell = 0, kLayerRoad = 1, kLayerQuarter = 2, kLayerCliff = 3 };
inline f32 ground_depth(int slot, int layer) {
  constexpr f32 k = (kDepthGroundFar - kDepthGroundNear) / static_cast<f32>(4 * (kGroundSlots + 2));
  return kDepthGroundNear + static_cast<f32>((kGroundSlots - slot) * 4 + (3 - layer)) * k;
}
static_assert(kDepthFlat < kDepthDecorShadow && kDepthDecorShadow < kDepthGroundNear &&
              kDepthRoad - 0.0004f < kDepthGroundNear);

// Legacy top-down helper (world-px foot y); iso code uses iso::depth_for_iso.
inline f32 depth_for_foot(f32 foot_y, f32 map_px_h) { return 0.9f - 0.8f * (foot_y / (map_px_h + 64.0f)); }
// Instances beyond kDepthShadow are ground; at or below kDepthOverlayMax they are screen overlays.
inline constexpr f32 kDepthShadow = 0.95f;
inline constexpr f32 kDepthOverlayMax = 0.08f;

// Static chunk instances carry a code in their colour's alpha byte (alpha = 255 - code) that
// shaders/ground.vert decodes: code = 1 + kind * 8 + height level; kind 0 terrain, 1 road,
// 2 cliff face, 3 decoration.  The ground shader treats every chunk instance as fully opaque tint.
enum GroundKind : int { kKindTerrain = 0, kKindRoad = 1, kKindCliff = 2, kKindDecor = 3 };
inline constexpr u32 ground_code_color(int kind, int level) {
  return 0x00FFFFFFu | (static_cast<u32>(255 - (1 + kind * 8 + level)) << 24);
}

// Appends all static instances of one chunk: first the ground (terrain, blends, cliffs, roads), then
// the decorations (tree/rock shadows, then bodies far to near).  Returns how many ground instances
// lead the appended range; the renderer draws every chunk's ground before any chunk's decorations,
// so a tree hanging over the next chunk never punches a hole in that chunk's ground.
u32 build_chunk(const sim::TileMap& map, const TerrainSprites& ts, int cx, int cy, std::vector<SpriteInstance>& out);

// Ground texture texels (RGBA8, one per tile; see shaders/ground.frag for the channels) for the tile
// rect [x0, x1) x [y0, y1), row-major into out (resized to the rect).
void build_ground_texels(const sim::TileMap& map, i32 x0, i32 y0, i32 x1, i32 y1, std::vector<u32>& out);

// One RGBA8 pixel per tile for far zoom levels / minimap.
void build_tile_colors(const sim::TileMap& map, const TerrainSprites& ts, std::vector<u32>& out);

}  // namespace urbs::render
