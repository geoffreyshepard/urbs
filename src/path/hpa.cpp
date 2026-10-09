#include "path/hpa.hpp"

#include <algorithm>

namespace urbs::path {

void PathScratch::init(u32 total_nodes) {
  local.init(static_cast<u32>(sim::kChunkTiles * sim::kChunkTiles));
  near.init(static_cast<u32>((kNearRange + 2 * kNearMargin + 1) * (kNearRange + 2 * kNearMargin + 1)));
  aheap.init(total_nodes);
  ag.assign(total_nodes, kNoPath);
  aparent.assign(total_nodes, -1);
  astamp.assign(total_nodes, 0);
  agen = 0;
  start_cost.assign(kMaxNodes, kNoPath);
  goal_cost.assign(kMaxNodes, kNoPath);
  abstract_path.clear();
  abstract_path.reserve(total_nodes);
  tiles.clear();
  tiles.reserve(1 << 16);
}

void Hpa::init(const NavGrid& nav) {
  cx_ = nav.chunks_x();
  cy_ = nav.chunks_y();
  chunks_.assign(static_cast<size_t>(cx_ * cy_), ChunkGraph{});
  build_search_.init(static_cast<u32>(sim::kChunkTiles * sim::kChunkTiles));
  touched_.assign(chunks_.size(), 0);
}

Recti Hpa::chunk_rect(const NavGrid& nav, i32 c) const {
  i32 x = (c % cx_) * sim::kChunkTiles, y = (c / cx_) * sim::kChunkTiles;
  return {x, y, std::min(sim::kChunkTiles, nav.w - x), std::min(sim::kChunkTiles, nav.h - y)};
}

i32 Hpa::find_node(i32 c, i32 tile) const {
  const ChunkGraph& g = chunks_[static_cast<size_t>(c)];
  for (int i = 0; i < g.count; ++i)
    if (g.tile[i] == tile) return c * kMaxNodes + i;
  return -1;
}

void Hpa::build_nodes(const NavGrid& nav, i32 c) {
  ChunkGraph& cg = chunks_[static_cast<size_t>(c)];
  cg.count = 0;
  const Recti r = chunk_rect(nav, c);
  auto add = [&](i32 x, i32 y) {
    i32 t = nav.idx(x, y);
    for (int i = 0; i < cg.count; ++i)
      if (cg.tile[i] == t) return;
    if (cg.count < kMaxNodes) cg.tile[cg.count++] = t;
  };
  // Each side: scan the border in increasing coordinate so both chunks pick the same portals.
  for (int side = 0; side < 4; ++side) {
    const bool horiz = side == 0 || side == 2;
    const i32 len = horiz ? r.w : r.h;
    i32 bx = 0, by = 0, ox = 0, oy = 0;  // first border tile (ours), offset to the outside tile
    switch (side) {
      case 0: bx = r.x, by = r.y, oy = -1; break;
      case 1: bx = r.x + r.w - 1, by = r.y, ox = 1; break;
      case 2: bx = r.x, by = r.y + r.h - 1, oy = 1; break;
      default: bx = r.x, by = r.y, ox = -1; break;
    }
    if (!nav.in_bounds(bx + ox, by + oy)) continue;
    int placed = 0;
    i32 run_start = -1;
    for (i32 k = 0; k <= len; ++k) {
      bool open = false;
      if (k < len) {
        i32 x = horiz ? bx + k : bx, y = horiz ? by : by + k;
        open = nav.walkable(x, y) && nav.walkable(x + ox, y + oy);
      }
      if (open && run_start < 0) run_start = k;
      if (!open && run_start >= 0) {
        i32 run_len = k - run_start;
        auto place = [&](i32 at) {
          if (placed >= kMaxPortalsPerSide) return;
          add(horiz ? bx + at : bx, horiz ? by : by + at);
          ++placed;
        };
        if (run_len <= 8) {
          place(run_start + (run_len - 1) / 2);
        } else {
          for (i32 at = run_start + 3; at < k; at += 8) place(at);
        }
        run_start = -1;
      }
    }
  }
}

void Hpa::build_intra(const NavGrid& nav, i32 c) {
  ChunkGraph& cg = chunks_[static_cast<size_t>(c)];
  const Recti r = chunk_rect(nav, c);
  for (int i = 0; i < cg.count; ++i) cg.cost[i][i] = 0;
  for (int i = 0; i + 1 < cg.count; ++i) {
    build_search_.dijkstra(nav, r, cg.tile[i], &cg.tile[i + 1], static_cast<u32>(cg.count - i - 1));
    for (int j = i + 1; j < cg.count; ++j) {
      u32 cst = build_search_.cost_to(cg.tile[j]);
      cg.cost[i][j] = cg.cost[j][i] = cst;
    }
  }
}

void Hpa::build_partners(const NavGrid& nav, i32 c) {
  ChunkGraph& cg = chunks_[static_cast<size_t>(c)];
  const Recti r = chunk_rect(nav, c);
  for (int i = 0; i < cg.count; ++i) {
    cg.partner[i][0] = cg.partner[i][1] = -1;
    cg.partner_cost[i][0] = cg.partner_cost[i][1] = kNoPath;
    i32 x = cg.tile[i] % nav.w, y = cg.tile[i] / nav.w;
    int slot = 0;
    for (int side = 0; side < 4 && slot < 2; ++side) {
      bool on = (side == 0 && y == r.y) || (side == 1 && x == r.x + r.w - 1) || (side == 2 && y == r.y + r.h - 1) ||
                (side == 3 && x == r.x);
      if (!on) continue;
      i32 nx = x + kDir4X[side], ny = y + kDir4Y[side];
      if (!nav.walkable(nx, ny)) continue;
      i32 node = find_node(nav.chunk_of(nx, ny), nav.idx(nx, ny));
      if (node < 0) continue;
      cg.partner[i][slot] = node;
      cg.partner_cost[i][slot] = nav.step_cost(x, y, side * 2);
      ++slot;
    }
  }
}

int Hpa::rebuild_dirty(NavGrid& nav, int budget) {
  int rebuilt = 0;
  std::fill(touched_.begin(), touched_.end(), u8{0});
  for (i32 c = 0; c < static_cast<i32>(chunks_.size()) && rebuilt < budget; ++c) {
    if (!nav.chunk_dirty[static_cast<size_t>(c)]) continue;
    build_nodes(nav, c);
    build_intra(nav, c);
    nav.chunk_dirty[static_cast<size_t>(c)] = 0;
    ++rebuilt;
    i32 x = c % cx_, y = c / cx_;
    touched_[static_cast<size_t>(c)] = 1;
    for (int d = 0; d < 4; ++d) {
      i32 nx = x + kDir4X[d], ny = y + kDir4Y[d];
      if (nx >= 0 && ny >= 0 && nx < cx_ && ny < cy_) touched_[static_cast<size_t>(ny * cx_ + nx)] = 1;
    }
  }
  for (i32 c = 0; c < static_cast<i32>(chunks_.size()); ++c)
    if (touched_[static_cast<size_t>(c)]) build_partners(nav, c);
  return rebuilt;
}

u32 Hpa::find(const NavGrid& nav, i32 s, i32 g, PathScratch& sc, bool* need_fallback) const {
  if (need_fallback) *need_fallback = false;
  sc.tiles.clear();
  if (!nav.connected(s, g)) return kNoPath;
  if (s == g) {
    sc.tiles.push_back(s);
    return 0;
  }
  const i32 cs = nav.chunk_of(s % nav.w, s / nav.w), cgc = nav.chunk_of(g % nav.w, g / nav.w);
  auto finish_cost = [&]() {
    u32 total = 0;
    for (size_t i = 1; i < sc.tiles.size(); ++i) {
      i32 a = sc.tiles[i - 1], b = sc.tiles[i];
      i32 dx = b % nav.w - a % nav.w, dy = b / nav.w - a / nav.w;
      for (int d = 0; d < 8; ++d)
        if (kDir8X[d] == dx && kDir8Y[d] == dy) {
          total += nav.step_cost(a % nav.w, a / nav.w, d);
          break;
        }
    }
    return total;
  };
  // Same chunk: the in-chunk path becomes a direct S->G edge; the abstract search still considers
  // routes that leave the chunk (e.g. around a river bend) and keeps whichever is cheaper.
  u32 direct = kNoPath;
  Recti near_rect{};
  {
    const i32 sx = s % nav.w, sy = s / nav.w, gx = g % nav.w, gy = g / nav.w;
    if (std::abs(sx - gx) <= kNearRange && std::abs(sy - gy) <= kNearRange) {
      i32 x0 = std::max(0, std::min(sx, gx) - kNearMargin), y0 = std::max(0, std::min(sy, gy) - kNearMargin);
      i32 x1 = std::min(nav.w - 1, std::max(sx, gx) + kNearMargin), y1 = std::min(nav.h - 1, std::max(sy, gy) + kNearMargin);
      near_rect = {x0, y0, x1 - x0 + 1, y1 - y0 + 1};
      direct = sc.near.astar(nav, near_rect, s, g, nullptr);
    }
  }

  const ChunkGraph& gs = chunks_[static_cast<size_t>(cs)];
  const ChunkGraph& gg = chunks_[static_cast<size_t>(cgc)];
  sc.local.dijkstra(nav, chunk_rect(nav, cs), s);
  for (int k = 0; k < gs.count; ++k) sc.start_cost[static_cast<size_t>(k)] = sc.local.cost_to(gs.tile[k]);
  sc.local.dijkstra(nav, chunk_rect(nav, cgc), g);
  for (int k = 0; k < gg.count; ++k) sc.goal_cost[static_cast<size_t>(k)] = sc.local.cost_to(gg.tile[k]);

  const u32 S = total_nodes() - 2, G = total_nodes() - 1;
  if (++sc.agen == 0) {
    std::fill(sc.astamp.begin(), sc.astamp.end(), 0u);
    sc.agen = 1;
  }
  const Vec2i gp{g % nav.w, g / nav.w};
  auto h = [&](u32 node) -> u32 {
    if (node == G) return 0;
    i32 t = node == S ? s : chunks_[node / kMaxNodes].tile[node % kMaxNodes];
    return static_cast<u32>(octile10({t % nav.w, t / nav.w}, gp) * kCostMin / 10);
  };
  auto gcost = [&](u32 n) -> u32& {
    if (sc.astamp[n] != sc.agen) {
      sc.astamp[n] = sc.agen;
      sc.ag[n] = kNoPath;
      sc.aparent[n] = -1;
    }
    return sc.ag[n];
  };
  sc.aheap.clear();
  gcost(S) = 0;
  sc.aheap.push_or_decrease(S, h(S));
  auto relax = [&](u32 from, u32 to, u32 w) {
    u32 ng = sc.ag[from] + w;
    u32& cur = gcost(to);
    if (ng < cur) {
      cur = ng;
      sc.aparent[to] = static_cast<i32>(from);
      sc.aheap.push_or_decrease(to, ng + h(to));
    }
  };
  bool found = false;
  while (!sc.aheap.empty()) {
    u32 n = sc.aheap.pop();
    if (n == G) {
      found = true;
      break;
    }
    if (n == S) {
      if (direct != kNoPath) relax(S, G, direct);
      for (int k = 0; k < gs.count; ++k)
        if (sc.start_cost[static_cast<size_t>(k)] != kNoPath)
          relax(S, static_cast<u32>(cs * kMaxNodes + k), sc.start_cost[static_cast<size_t>(k)]);
      continue;
    }
    const u32 c = n / kMaxNodes, l = n % kMaxNodes;
    const ChunkGraph& cg = chunks_[c];
    for (int k = 0; k < cg.count; ++k)
      if (static_cast<u32>(k) != l && cg.cost[l][k] != kNoPath) relax(n, c * kMaxNodes + static_cast<u32>(k), cg.cost[l][k]);
    for (int p = 0; p < 2; ++p)
      if (cg.partner[l][p] >= 0) relax(n, static_cast<u32>(cg.partner[l][p]), cg.partner_cost[l][p]);
    if (static_cast<i32>(c) == cgc && sc.goal_cost[l] != kNoPath) relax(n, G, sc.goal_cost[l]);
  }
  if (!found) {
    if (need_fallback) *need_fallback = true;
    return kNoPath;
  }

  if (sc.aparent[G] == static_cast<i32>(S)) {  // the direct short-trip path won
    sc.near.astar(nav, near_rect, s, g, &sc.tiles);
    return finish_cost();
  }
  sc.abstract_path.clear();
  for (i32 n = static_cast<i32>(G); n != -1; n = sc.aparent[static_cast<size_t>(n)]) sc.abstract_path.push_back(n);
  std::reverse(sc.abstract_path.begin(), sc.abstract_path.end());

  sc.tiles.push_back(s);
  for (size_t i = 1; i < sc.abstract_path.size(); ++i) {
    const u32 node = static_cast<u32>(sc.abstract_path[i]);
    const i32 t = node == G ? g : chunks_[node / kMaxNodes].tile[node % kMaxNodes];
    const i32 prev = sc.tiles.back();
    if (t == prev) continue;
    const i32 pc = nav.chunk_of(prev % nav.w, prev / nav.w), tc = nav.chunk_of(t % nav.w, t / nav.w);
    if (pc != tc) {
      sc.tiles.push_back(t);  // inter-chunk edge between paired portals
      continue;
    }
    sc.tiles.pop_back();
    if (sc.local.astar(nav, chunk_rect(nav, tc), prev, t, &sc.tiles) == kNoPath) {
      if (need_fallback) *need_fallback = true;
      sc.tiles.clear();
      return kNoPath;
    }
  }
  return finish_cost();
}

}  // namespace urbs::path
