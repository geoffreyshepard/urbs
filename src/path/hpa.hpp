#pragma once
#include <vector>

#include "path/nav.hpp"
#include "path/search.hpp"

namespace urbs::path {

inline constexpr int kMaxPortalsPerSide = 12;
inline constexpr int kMaxNodes = 48;  // per chunk
inline constexpr u32 kNoPath = 0xFFFFFFFFu;
inline constexpr i32 kNearRange = 64;   // Chebyshev distance for the direct short-trip search
inline constexpr i32 kNearMargin = 12;

// Abstract graph over 32x32 chunks (HPA*). Nodes are border portal tiles; intra-chunk edges
// carry exact in-chunk costs; inter-chunk edges join paired portal tiles across a border.
struct ChunkGraph {
  u8 count = 0;
  i32 tile[kMaxNodes];
  u32 cost[kMaxNodes][kMaxNodes];  // intra-chunk cost, kNoPath if unreachable inside the chunk
  i32 partner[kMaxNodes][2];       // global node id across a border, -1 if none
  u32 partner_cost[kMaxNodes][2];
};

// Per-thread search scratch.
struct PathScratch {
  GridSearch local;     // chunk-sized
  GridSearch near;      // short trips: bounding box of start/goal plus margin
  IndexedHeap aheap;    // abstract graph
  std::vector<u32> ag;
  std::vector<i32> aparent;
  std::vector<u32> astamp;
  u32 agen = 0;
  std::vector<u32> start_cost, goal_cost;  // per local node of start/goal chunk
  std::vector<i32> abstract_path;
  std::vector<i32> tiles;  // output
  void init(u32 total_nodes);
};

class Hpa {
 public:
  void init(const NavGrid& nav);
  // Rebuilds dirty chunks (nav.chunk_dirty) and clears the flags. Returns chunks rebuilt.
  int rebuild_dirty(NavGrid& nav, int budget = 1 << 30);
  // Full search. Fills scratch.tiles with s..g. Returns total cost (tenths of ticks) or kNoPath.
  // Returns kNoPath with *need_fallback = true when HPA cannot find a path in a connected region
  // (possible when a border has more portal runs than kMaxPortalsPerSide).
  u32 find(const NavGrid& nav, i32 s, i32 g, PathScratch& sc, bool* need_fallback) const;
  [[nodiscard]] u32 total_nodes() const { return static_cast<u32>(chunks_.size()) * kMaxNodes + 2; }
  [[nodiscard]] const ChunkGraph& chunk(i32 c) const { return chunks_[static_cast<size_t>(c)]; }

 private:
  void build_nodes(const NavGrid& nav, i32 c);
  void build_intra(const NavGrid& nav, i32 c);
  void build_partners(const NavGrid& nav, i32 c);
  [[nodiscard]] Recti chunk_rect(const NavGrid& nav, i32 c) const;
  [[nodiscard]] i32 find_node(i32 c, i32 tile) const;

  std::vector<ChunkGraph> chunks_;
  i32 cx_ = 0, cy_ = 0;
  GridSearch build_search_;
  std::vector<u8> touched_;
};

}  // namespace urbs::path
