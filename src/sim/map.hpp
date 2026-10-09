#pragma once
#include <vector>

#include "core/math.hpp"
#include "core/types.hpp"

namespace urbs::sim {

inline constexpr i32 kTilePx = 16;
inline constexpr i32 kChunkTiles = 32;
inline constexpr u8 kMaxHeight = 7;

enum class Terrain : u8 { Grass, Fertile, Sand, Rock, Forest, Marsh, Shallow, Deep, Count };
enum class Road : u8 { None, Dirt, Paved, Bridge };

// Natural resources placed by map generation (mines, quarries, clay pits need them underneath).
enum class Resource : u8 { None, Clay, Stone, IronOre, SilverOre, Marble, Limestone, Pozzolana, Fish, Count };

inline constexpr const char* terrain_name(Terrain t) {
  constexpr const char* kNames[] = {"grass", "fertile", "sand", "rock", "forest", "marsh", "shallow", "deep"};
  return kNames[static_cast<int>(t)];
}

inline constexpr bool terrain_walkable(Terrain t) { return t != Terrain::Deep && t != Terrain::Shallow; }
inline constexpr bool terrain_water(Terrain t) { return t == Terrain::Deep || t == Terrain::Shallow; }

// Tile-grid world state. Plain arrays, one entry per tile, row-major.
struct TileMap {
  i32 w = 0, h = 0;
  std::vector<Terrain> terrain;
  std::vector<u8> height;       // 0..kMaxHeight
  std::vector<u8> groundwater;  // 0..255 well quality
  std::vector<u8> fertility;    // 0..255 farm yield multiplier (128 = 1.0)
  std::vector<Resource> resource;
  std::vector<Road> road;
  std::vector<u8> variant;  // visual variant (stable random per tile)
  std::vector<BuildingId> building;
  std::vector<u8> chunk_dirty;  // per chunk: static render layer must be rebuilt
  u32 revision = 0;             // bumps on any structural change (roads, buildings, terrain)

  void resize(i32 width, i32 height_tiles);
  [[nodiscard]] bool in_bounds(i32 x, i32 y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  [[nodiscard]] bool in_bounds(Vec2i p) const { return in_bounds(p.x, p.y); }
  [[nodiscard]] i32 idx(i32 x, i32 y) const { return y * w + x; }
  [[nodiscard]] i32 idx(Vec2i p) const { return p.y * w + p.x; }
  [[nodiscard]] Vec2i pos(i32 i) const { return {i % w, i / w}; }
  [[nodiscard]] i32 chunks_x() const { return (w + kChunkTiles - 1) / kChunkTiles; }
  [[nodiscard]] i32 chunks_y() const { return (h + kChunkTiles - 1) / kChunkTiles; }
  void mark_dirty(i32 x, i32 y);  // marks the chunk (and neighbours across borders) dirty
  void mark_all_dirty();
};

struct MapGenParams {
  i32 size = 192;
  u64 seed = 1;
  // Coast: -1 none, 0 north, 1 east, 2 south, 3 west.
  i32 coast = -1;
  bool river = true;
  f32 hills = 0.5f;   // 0..1
  f32 forest = 0.5f;  // 0..1
  f32 marsh = 0.3f;   // 0..1
};

void generate_map(TileMap& map, const MapGenParams& p);

}  // namespace urbs::sim
