#include "sim/services.hpp"

#include <algorithm>

#include "core/profile.hpp"

namespace urbs::sim {

namespace {
constexpr u32 kBuckets = 128;  // > max step cost
constexpr u32 kNil = 0xFFFFFFFFu;
}  // namespace

void ServiceMaps::init(i32 w, i32 h, u32 services, u32 max_providers) {
  w_ = w;
  h_ = h;
  const size_t n = static_cast<size_t>(w * h);
  maps_.assign(services, Map{});
  for (auto& m : maps_) {
    m.prov.assign(n, kUncovered);
    m.cost.assign(n, 0xFFFF);
    m.ids.reserve(max_providers);
  }
  work_.prov.assign(n, kUncovered);
  work_.cost.assign(n, 0xFFFF);
  work_.ids.reserve(max_providers);
  pending_ids_.assign(services, {});
  pending_entr_.assign(services, {});
  for (u32 s = 0; s < services; ++s) {
    pending_ids_[s].reserve(max_providers);
    pending_entr_[s].reserve(max_providers);
  }
  dirty_.assign(services, 1);
  bucket_head_.assign(kBuckets, kNil);
  next_.assign(n, kNil);
  prev_.assign(n, kNil);
  settled_.assign(n, 0);
  running_ = -1;
}

void ServiceMaps::set_providers(u32 s, const std::vector<BuildingId>& ids, const std::vector<i32>& entr) {
  auto& pi = pending_ids_[s];
  auto& pe = pending_entr_[s];
  pi.clear();
  pe.clear();
  for (size_t i = 0; i < ids.size() && i < pi.capacity() && i < 0xFFFE; ++i) {
    pi.push_back(ids[i]);
    pe.push_back(entr[i]);
  }
  dirty_[s] = 1;
}

void ServiceMaps::mark_all_dirty() { std::fill(dirty_.begin(), dirty_.end(), u8{1}); }

void ServiceMaps::link(u32 t, u32 b) {
  prev_[t] = kNil;
  next_[t] = bucket_head_[b];
  if (next_[t] != kNil) prev_[next_[t]] = t;
  bucket_head_[b] = t;
}

void ServiceMaps::unlink(u32 t) {
  u32 b = work_.cost[t] % kBuckets;
  if (prev_[t] != kNil) next_[prev_[t]] = next_[t];
  else bucket_head_[b] = next_[t];
  if (next_[t] != kNil) prev_[next_[t]] = prev_[t];
  next_[t] = prev_[t] = kNil;
}

void ServiceMaps::begin(u32 s, const path::NavGrid& nav) {
  running_ = static_cast<i32>(s);
  std::fill(work_.prov.begin(), work_.prov.end(), kUncovered);
  std::fill(work_.cost.begin(), work_.cost.end(), u16{0xFFFF});
  std::fill(settled_.begin(), settled_.end(), u8{0});
  std::fill(bucket_head_.begin(), bucket_head_.end(), kNil);
  work_.ids.clear();
  for (auto id : pending_ids_[s]) work_.ids.push_back(id);
  cur_cost_ = 0;
  in_queue_ = 0;
  const auto& entr = pending_entr_[s];
  for (size_t i = 0; i < entr.size(); ++i) {
    i32 t = entr[i];
    if (t < 0 || t >= w_ * h_ || nav.cost[static_cast<size_t>(t)] == 0) continue;
    u32 ut = static_cast<u32>(t);
    if (work_.cost[ut] == 0 && work_.prov[ut] != kUncovered) continue;  // two providers share an entrance
    if (work_.prov[ut] != kUncovered) unlink(ut);
    else ++in_queue_;
    work_.cost[ut] = 0;
    work_.prov[ut] = static_cast<u16>(i);
    link(ut, 0);
  }
  dirty_[s] = 0;
}

void ServiceMaps::step(const path::NavGrid& nav, u32 budget) {
  URBS_ZONE("service_maps");
  if (running_ < 0) {
    // Pick the next dirty service, round robin.
    const u32 n = static_cast<u32>(maps_.size());
    for (u32 k = 0; k < n; ++k) {
      u32 s = (rr_ + k) % n;
      if (dirty_[s]) {
        rr_ = s + 1;
        begin(s, nav);
        break;
      }
    }
    if (running_ < 0) return;
  }
  while (budget > 0 && in_queue_ > 0) {
    u32 b = cur_cost_ % kBuckets;
    u32 t = bucket_head_[b];
    if (t == kNil) {
      ++cur_cost_;
      continue;
    }
    // Pop t (its cost equals cur_cost_ by construction).
    bucket_head_[b] = next_[t];
    if (next_[t] != kNil) prev_[next_[t]] = kNil;
    next_[t] = prev_[t] = kNil;
    --in_queue_;
    settled_[t] = 1;
    --budget;
    const i32 x = static_cast<i32>(t) % w_, y = static_cast<i32>(t) / w_;
    const u32 base = work_.cost[t];
    for (int d = 0; d < 8; ++d) {
      u32 sc = nav.step_cost(x, y, d);
      if (sc == 0) continue;
      u32 nc = base + sc;
      if (nc > kMaxCost) continue;
      u32 nt = static_cast<u32>((y + kDir8Y[d]) * w_ + x + kDir8X[d]);
      if (settled_[nt]) continue;
      if (nc < work_.cost[nt]) {
        if (work_.prov[nt] != kUncovered && work_.cost[nt] != 0xFFFF) unlink(nt);
        else ++in_queue_;
        work_.cost[nt] = static_cast<u16>(nc);
        work_.prov[nt] = work_.prov[t];
        link(nt, nc % kBuckets);
      }
    }
  }
  if (in_queue_ == 0) {
    Map& m = maps_[static_cast<size_t>(running_)];
    std::swap(m.prov, work_.prov);
    std::swap(m.cost, work_.cost);
    std::swap(m.ids, work_.ids);
    m.ready = true;
    ++m.generation;
    running_ = -1;
  }
}

}  // namespace urbs::sim

#include "save/serialize.hpp"

namespace urbs::sim {

void ServiceMaps::serialize(save::Writer& w) const {
  w.pod(static_cast<u32>(maps_.size()));
  for (const Map& m : maps_) {
    w.vec(m.prov);
    w.vec(m.cost);
    w.vec(m.ids);
    w.pod(static_cast<u8>(m.ready));
    w.pod(m.generation);
  }
  for (size_t s = 0; s < maps_.size(); ++s) {
    w.vec(pending_ids_[s]);
    w.vec(pending_entr_[s]);
  }
  w.vec(dirty_);
  w.vec(work_.prov);
  w.vec(work_.cost);
  w.vec(work_.ids);
  w.pod(running_);
  w.vec(bucket_head_);
  w.vec(next_);
  w.vec(prev_);
  w.vec(settled_);
  w.pod(cur_cost_);
  w.pod(in_queue_);
  w.pod(rr_);
}

void ServiceMaps::deserialize(save::Reader& r) {
  u32 n = r.pod<u32>();
  if (n != maps_.size()) throw std::runtime_error("save: service count changed (defs differ from save)");
  for (Map& m : maps_) {
    r.vec(m.prov);
    r.vec(m.cost);
    r.vec_fixed(m.ids);
    m.ready = r.pod<u8>() != 0;
    r.pod(m.generation);
  }
  for (size_t s = 0; s < maps_.size(); ++s) {
    r.vec_fixed(pending_ids_[s]);
    r.vec_fixed(pending_entr_[s]);
  }
  r.vec(dirty_);
  r.vec(work_.prov);
  r.vec(work_.cost);
  r.vec_fixed(work_.ids);
  r.pod(running_);
  r.vec(bucket_head_);
  r.vec(next_);
  r.vec(prev_);
  r.vec(settled_);
  r.pod(cur_cost_);
  r.pod(in_queue_);
  r.pod(rr_);
}

}  // namespace urbs::sim
