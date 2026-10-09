#include "path/search.hpp"

#include <algorithm>

namespace urbs::path {

static constexpr u32 kInf = 0xFFFFFFFFu;

void GridSearch::init(u32 max_area) {
  g_.assign(max_area, kInf);
  parent_.assign(max_area, -1);
  stamp_.assign(max_area, 0);
  heap_.init(max_area);
  gen_ = 0;
}

bool GridSearch::local(i32 tile, u32& li) const {
  i32 x = tile % map_w_, y = tile / map_w_;
  if (x < rect_.x || y < rect_.y || x >= rect_.x + rect_.w || y >= rect_.y + rect_.h) return false;
  li = static_cast<u32>((y - rect_.y) * rect_.w + (x - rect_.x));
  return true;
}

u32 GridSearch::cost_to(i32 tile) const {
  u32 li;
  if (!local(tile, li) || stamp_[li] != gen_) return kInf;
  return g_[li];
}

u32 GridSearch::astar(const NavGrid& nav, Recti rect, i32 s, i32 goal, std::vector<i32>* out) {
  rect_ = rect;
  map_w_ = nav.w;
  if (++gen_ == 0) {
    std::fill(stamp_.begin(), stamp_.end(), 0u);
    gen_ = 1;
  }
  heap_.clear();
  u32 ls, lg;
  if (!local(s, ls) || !local(goal, lg)) return kInf;
  const Vec2i gp{goal % nav.w, goal / nav.w};
  auto h = [&](i32 x, i32 y) { return static_cast<u32>(octile10({x, y}, gp) * kCostMin / 10); };
  stamp_[ls] = gen_;
  g_[ls] = 0;
  parent_[ls] = -1;
  heap_.push_or_decrease(ls, h(s % nav.w, s / nav.w));
  bool found = false;
  while (!heap_.empty()) {
    u32 cur = heap_.pop();
    if (cur == lg) {
      found = true;
      break;
    }
    i32 cx = rect.x + static_cast<i32>(cur) % rect.w, cy = rect.y + static_cast<i32>(cur) / rect.w;
    for (int d = 0; d < 8; ++d) {
      i32 nx = cx + kDir8X[d], ny = cy + kDir8Y[d];
      if (nx < rect.x || ny < rect.y || nx >= rect.x + rect.w || ny >= rect.y + rect.h) continue;
      u32 sc = nav.step_cost(cx, cy, d);
      if (sc == 0) continue;
      u32 ni = static_cast<u32>((ny - rect.y) * rect.w + (nx - rect.x));
      u32 ng = g_[cur] + sc;
      if (stamp_[ni] != gen_) {
        stamp_[ni] = gen_;
        g_[ni] = kInf;
      }
      if (ng < g_[ni]) {
        g_[ni] = ng;
        parent_[ni] = static_cast<i32>(cur);
        heap_.push_or_decrease(ni, ng + h(nx, ny));
      }
    }
  }
  if (!found) return kInf;
  if (out) {
    size_t start = out->size();
    for (i32 c = static_cast<i32>(lg); c != -1; c = parent_[static_cast<size_t>(c)])
      out->push_back((rect.y + c / rect.w) * nav.w + rect.x + c % rect.w);
    std::reverse(out->begin() + static_cast<std::ptrdiff_t>(start), out->end());
  }
  return g_[lg];
}

void GridSearch::dijkstra(const NavGrid& nav, Recti rect, i32 s, const i32* stops, u32 stop_count) {
  rect_ = rect;
  map_w_ = nav.w;
  if (++gen_ == 0) {
    std::fill(stamp_.begin(), stamp_.end(), 0u);
    gen_ = 1;
  }
  heap_.clear();
  u32 ls;
  if (!local(s, ls)) return;
  stamp_[ls] = gen_;
  g_[ls] = 0;
  heap_.push_or_decrease(ls, 0);
  u32 remaining = stop_count;
  while (!heap_.empty()) {
    u32 cur = heap_.pop();
    if (stops && remaining > 0) {
      i32 tile = (rect.y + static_cast<i32>(cur) / rect.w) * nav.w + rect.x + static_cast<i32>(cur) % rect.w;
      for (u32 k = 0; k < stop_count; ++k)
        if (stops[k] == tile) {
          if (--remaining == 0) return;
          break;
        }
    }
    i32 cx = rect.x + static_cast<i32>(cur) % rect.w, cy = rect.y + static_cast<i32>(cur) / rect.w;
    for (int d = 0; d < 8; ++d) {
      i32 nx = cx + kDir8X[d], ny = cy + kDir8Y[d];
      if (nx < rect.x || ny < rect.y || nx >= rect.x + rect.w || ny >= rect.y + rect.h) continue;
      u32 sc = nav.step_cost(cx, cy, d);
      if (sc == 0) continue;
      u32 ni = static_cast<u32>((ny - rect.y) * rect.w + (nx - rect.x));
      u32 ng = g_[cur] + sc;
      if (stamp_[ni] != gen_) {
        stamp_[ni] = gen_;
        g_[ni] = kInf;
      }
      if (ng < g_[ni]) {
        g_[ni] = ng;
        heap_.push_or_decrease(ni, ng);
      }
    }
  }
}

}  // namespace urbs::path
