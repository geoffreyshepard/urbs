#include "game/ui_icons.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

#include "render/gpu.hpp"

namespace urbs::game {

bool UiIcons::init(SDL_GPUDevice* dev, const render::Atlas& atlas, const data::Defs& defs) {
  // Which sprites to include: advisors, gods, goods icons, HUD and category icons.
  std::vector<std::string> names;
  for (const char* a : {"engineer", "quaestor", "haruspex", "legate", "aedile"}) names.push_back(std::string("ui/advisor_") + a);
  for (const char* g : {"jupiter", "juno", "mars", "venus", "ceres", "neptune", "mercury", "minerva", "vulcan", "vesta", "isis",
                        "serapis", "divus"})
    names.push_back(std::string("ui/god_") + g);
  names.emplace_back("goods/denarii_16");
  for (const auto& g : defs.goods)
    if (atlas.has("goods/" + g.id + "_16")) names.push_back("goods/" + g.id + "_16");
  for (const char* h : {"population", "happiness", "culture", "prosperity", "security", "favor", "build", "rome", "advisors",
                        "finance", "goods", "trade", "labor", "religion", "military", "research", "alerts", "help", "menu",
                        "pause", "play", "fast", "faster", "road", "demolish", "overlay"})
    names.push_back(std::string("ui/icon_") + h);
  for (const char* c : {"roads", "housing", "farms", "food", "industry", "storage", "water", "health", "religion",
                        "culture", "civic", "military", "monument"})
    names.push_back(std::string("ui/cat_") + c);
  constexpr int kScale = 6, kCell = 32 * kScale, kCols = 8;
  const int rows = static_cast<int>((names.size() + kCols - 1) / kCols);
  const u32 W = kCols * kCell, H = static_cast<u32>(rows) * kCell;
  std::vector<u8> px(static_cast<size_t>(W) * H * 4, 0);
  const auto& src = atlas.pixels();
  for (size_t k = 0; k < names.size(); ++k) {
    const render::Sprite s = atlas.find(names[k]);
    if (!s.valid) continue;
    const int cx = static_cast<int>(k % kCols) * kCell, cy = static_cast<int>(k / kCols) * kCell;
    for (int y = 0; y < s.h * kScale && y < kCell; ++y)
      for (int x = 0; x < s.w * kScale && x < kCell; ++x) {
        const size_t si = (static_cast<size_t>(s.v + y / kScale) * atlas.width() + s.u + x / kScale) * 4;
        const size_t di = (static_cast<size_t>(cy + y) * W + cx + x) * 4;
        for (int c = 0; c < 4; ++c) px[di + c] = src[si + c];
      }
    entries_[names[k]] = Entry{ImVec2(static_cast<float>(cx) / W, static_cast<float>(cy) / H),
                               ImVec2(static_cast<float>(cx + s.w * kScale) / W, static_cast<float>(cy + s.h * kScale) / H)};
  }
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xf;
  tex_ = render::create_texture_rgba(dev, W, H, px.data(), cp, xf);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev);
  for (auto* x : xf) SDL_ReleaseGPUTransferBuffer(dev, x);
  // Big pictures, upscaled x3 into their own texture (stacked vertically).
  {
    const char* bigs[] = {"ui/province_map", "ui/logo"};
    constexpr int kS = 3;
    u32 BW = 0, BH = 0;
    for (const char* n : bigs) {
      const render::Sprite s = atlas.find(n);
      if (!s.valid) continue;
      BW = std::max<u32>(BW, s.w * kS);
      BH += s.h * kS;
    }
    if (BW > 0) {
      std::vector<u8> bp(static_cast<size_t>(BW) * BH * 4, 0);
      u32 oy = 0;
      for (const char* n : bigs) {
        const render::Sprite s = atlas.find(n);
        if (!s.valid) continue;
        for (u32 y = 0; y < static_cast<u32>(s.h) * kS; ++y)
          for (u32 x = 0; x < static_cast<u32>(s.w) * kS; ++x) {
            const size_t si = (static_cast<size_t>(s.v + y / kS) * atlas.width() + s.u + x / kS) * 4;
            const size_t di = (static_cast<size_t>(oy + y) * BW + x) * 4;
            for (int c = 0; c < 4; ++c) bp[di + c] = src[si + c];
          }
        big_entries_[n] = Entry{ImVec2(0, static_cast<float>(oy) / BH),
                                ImVec2(static_cast<float>(s.w * kS) / BW, static_cast<float>(oy + s.h * kS) / BH)};
        oy += s.h * kS;
      }
      SDL_GPUCommandBuffer* c2 = SDL_AcquireGPUCommandBuffer(dev);
      SDL_GPUCopyPass* p2 = SDL_BeginGPUCopyPass(c2);
      std::vector<SDL_GPUTransferBuffer*> x2;
      big_ = render::create_texture_rgba(dev, BW, BH, bp.data(), p2, x2);
      SDL_EndGPUCopyPass(p2);
      SDL_SubmitGPUCommandBuffer(c2);
      SDL_WaitForGPUIdle(dev);
      for (auto* x : x2) SDL_ReleaseGPUTransferBuffer(dev, x);
    }
  }
  return tex_ != nullptr;
}

bool UiIcons::picture(const std::string& name, ImVec2 size) const {
  auto it = big_entries_.find(name);
  if (it == big_entries_.end() || !big_) return false;
  ImGui::Image(big_texture(), size, it->second.uv0, it->second.uv1);
  return true;
}

ImTextureRef UiIcons::big_texture() const { return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(big_))); }

bool UiIcons::big_uv(const std::string& name, ImVec2& uv0, ImVec2& uv1) const {
  auto it = big_entries_.find(name);
  if (it == big_entries_.end()) return false;
  uv0 = it->second.uv0;
  uv1 = it->second.uv1;
  return true;
}

void UiIcons::shutdown(SDL_GPUDevice* dev) {
  for (SDL_GPUTexture* t : {tex_, big_, thumbs_, thumbs_old_})
    if (t) SDL_ReleaseGPUTexture(dev, t);
  tex_ = big_ = thumbs_ = thumbs_old_ = nullptr;
}

namespace {

// A sprite placed on a tile of a small layout (a farm field, a stretch of wall).
struct Piece {
  render::Sprite s;
  int tx = 0, ty = 0;
};

struct Canvas {
  int w = 0, h = 0;
  std::vector<u8> px;
};

// Top-left pixel of a piece: the anchor is the footprint's bottom-left pixel; sprites without one are
// tile-sized and cover their tile.
void piece_origin(const Piece& p, int& x, int& y) {
  constexpr int T = 16;
  if (p.s.ax == 0 && p.s.ay == 0) {
    x = p.tx * T;
    y = p.ty * T;
  } else {
    x = p.tx * T - p.s.ax;
    y = p.ty * T + T - 1 - p.s.ay;
  }
}

// Paints the pieces in order onto one canvas, cropped to the opaque pixels.
Canvas compose(const render::Atlas& atlas, const std::vector<Piece>& pieces) {
  Canvas c;
  int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
  for (const Piece& p : pieces) {
    int x, y;
    piece_origin(p, x, y);
    x0 = std::min(x0, x);
    y0 = std::min(y0, y);
    x1 = std::max(x1, x + p.s.w);
    y1 = std::max(y1, y + p.s.h);
  }
  if (pieces.empty()) return c;
  const int w = x1 - x0, h = y1 - y0;
  std::vector<u8> px(static_cast<size_t>(w) * h * 4, 0);
  const auto& src = atlas.pixels();
  for (const Piece& p : pieces) {
    int ox, oy;
    piece_origin(p, ox, oy);
    for (int y = 0; y < p.s.h; ++y)
      for (int x = 0; x < p.s.w; ++x) {
        const size_t si = (static_cast<size_t>(p.s.v + y) * atlas.width() + p.s.u + x) * 4;
        if (src[si + 3] == 0) continue;
        const size_t di = (static_cast<size_t>(oy - y0 + y) * w + ox - x0 + x) * 4;
        for (int k = 0; k < 4; ++k) px[di + k] = src[si + k];
      }
  }
  // Crop to the opaque bounding box.
  int bx0 = w, by0 = h, bx1 = -1, by1 = -1;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (px[(static_cast<size_t>(y) * w + x) * 4 + 3]) {
        bx0 = std::min(bx0, x);
        by0 = std::min(by0, y);
        bx1 = std::max(bx1, x);
        by1 = std::max(by1, y);
      }
  if (bx1 < 0) return c;
  c.w = bx1 - bx0 + 1;
  c.h = by1 - by0 + 1;
  c.px.resize(static_cast<size_t>(c.w) * c.h * 4);
  for (int y = 0; y < c.h; ++y)
    std::copy_n(&px[(static_cast<size_t>(by0 + y) * w + bx0) * 4], static_cast<size_t>(c.w) * 4,
                &c.px[static_cast<size_t>(y) * c.w * 4]);
  return c;
}

// Fits a canvas into a k x k cell of `dst` (row stride `stride` pixels) at (cx, cy), centred: whole-number
// nearest-neighbour upscaling up to 4x, or an area average when the picture must shrink.
void fit(const Canvas& c, std::vector<u8>& dst, int stride, int cx, int cy, int k) {
  const float cw = static_cast<float>(c.w), ch = static_cast<float>(c.h), kf = static_cast<float>(k);
  float f = std::min(kf / cw, kf / ch);
  if (f >= 1) f = std::min(4.0f, std::floor(f));
  const int dw = std::max(1, static_cast<int>(cw * f)), dh = std::max(1, static_cast<int>(ch * f));
  const int ox = cx + (k - dw) / 2, oy = cy + (k - dh) / 2;
  auto src_at = [&](float v) { return static_cast<int>(v / f); };
  for (int y = 0; y < dh; ++y)
    for (int x = 0; x < dw; ++x) {
      const float fx = static_cast<float>(x), fy = static_cast<float>(y);
      u8* d = &dst[(static_cast<size_t>(oy + y) * stride + ox + x) * 4];
      if (f >= 1) {
        const u8* s = &c.px[(static_cast<size_t>(src_at(fy)) * c.w + src_at(fx)) * 4];
        for (int q = 0; q < 4; ++q) d[q] = s[q];
        continue;
      }
      const int sx0 = src_at(fx), sy0 = src_at(fy);
      const int sx1 = std::min(c.w, std::max(sx0 + 1, src_at(fx + 1)));
      const int sy1 = std::min(c.h, std::max(sy0 + 1, src_at(fy + 1)));
      float r = 0, g = 0, b = 0, a = 0;
      int n = 0;
      for (int sy = sy0; sy < sy1; ++sy)
        for (int sx = sx0; sx < sx1; ++sx, ++n) {
          const u8* s = &c.px[(static_cast<size_t>(sy) * c.w + sx) * 4];
          const float al = s[3] / 255.0f;
          r += s[0] * al;
          g += s[1] * al;
          b += s[2] * al;
          a += al;
        }
      if (a <= 0) continue;
      d[0] = static_cast<u8>(r / a);
      d[1] = static_cast<u8>(g / a);
      d[2] = static_cast<u8>(b / a);
      d[3] = static_cast<u8>(std::min(255.0f, 255.0f * a / static_cast<float>(n)));
    }
}

}  // namespace

void UiIcons::build_thumbs(SDL_GPUDevice* dev, const render::Atlas& atlas, const data::Defs& defs,
                           const std::string& region) {
  auto find = [&](const std::string& n) {
    if (!region.empty()) {
      const render::Sprite v = atlas.find(n + "@" + region);
      if (v.valid) return v;
    }
    return atlas.find(n);
  };
  // A straight run of three pieces (wall, aqueduct, road): centre plus the spokes joining its neighbours.
  auto line_of = [&](const std::string& base, std::vector<Piece>& out) {
    const render::Sprite c = find(base + "_c"), e = find(base + "_e"), w = find(base + "_w");
    for (int i = 0; i < 3; ++i) {
      if (c.valid) out.push_back({c, i, 0});
      if (i < 2 && e.valid) out.push_back({e, i, 0});
      if (i > 0 && w.valid) out.push_back({w, i, 0});
    }
  };
  std::vector<std::pair<std::string, std::vector<Piece>>> items;
  for (const auto& b : defs.buildings) {
    std::vector<Piece> p;
    for (int t : {2, 1, 0}) {  // houses: a middle tier reads as "a house", not an empty plot
      const render::Sprite s = find(b.sprite + "_t" + std::to_string(t));
      if (s.valid) {
        p.push_back({s, 0, 0});
        break;
      }
    }
    if (p.empty() && find(b.sprite + "_c").valid) line_of(b.sprite, p);
    if (p.empty() && find(b.sprite + "_tile_0").valid) {  // painted fields: 2x2 tiles, the hut on the back corner
      for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) p.push_back({find(b.sprite + "_tile_" + std::to_string((x + y * 2) % 4)), x, y});
      const render::Sprite hut = find(b.sprite + "_hut");
      if (hut.valid) p.push_back({hut, 0, 0});
    }
    if (p.empty()) {
      const render::Sprite s = find(b.sprite);
      if (s.valid) p.push_back({s, 0, 0});
    }
    items.emplace_back(b.id, std::move(p));
  }
  for (const char* r : {"dirt", "paved"}) {
    std::vector<Piece> p;
    line_of(std::string("road/") + r, p);
    items.emplace_back(std::string("tool:") + r, std::move(p));
  }
  items.emplace_back("tool:demolish", std::vector<Piece>{{atlas.find("ui/icon_demolish"), 0, 0}});

  constexpr int kCell = 96, kCols = 16;
  const int rows = static_cast<int>((items.size() + kCols - 1) / kCols);
  const u32 W = kCols * kCell, H = static_cast<u32>(rows) * kCell;
  std::vector<u8> px(static_cast<size_t>(W) * H * 4, 0);
  thumb_entries_.clear();
  for (size_t k = 0; k < items.size(); ++k) {
    std::vector<Piece> pieces;
    for (const Piece& p : items[k].second)
      if (p.s.valid) pieces.push_back(p);
    const Canvas c = compose(atlas, pieces);
    if (c.w == 0) continue;
    const int cx = static_cast<int>(k % kCols) * kCell, cy = static_cast<int>(k / kCols) * kCell;
    fit(c, px, static_cast<int>(W), cx, cy, kCell);
    thumb_entries_[items[k].first] =
        Entry{ImVec2(static_cast<float>(cx) / W, static_cast<float>(cy) / H),
              ImVec2(static_cast<float>(cx + kCell) / W, static_cast<float>(cy + kCell) / H)};
  }
  if (thumbs_old_) SDL_ReleaseGPUTexture(dev, thumbs_old_);
  thumbs_old_ = thumbs_;
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xf;
  thumbs_ = render::create_texture_rgba(dev, W, H, px.data(), cp, xf);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev);
  for (auto* x : xf) SDL_ReleaseGPUTransferBuffer(dev, x);
}

bool UiIcons::draw_thumb(const std::string& key, ImVec2 p0, float size) const {
  auto it = thumb_entries_.find(key);
  if (it == thumb_entries_.end() || !thumbs_) return false;
  ImGui::GetWindowDrawList()->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(thumbs_))), p0,
                                       ImVec2(p0.x + size, p0.y + size), it->second.uv0, it->second.uv1);
  return true;
}

bool UiIcons::draw_icon(const std::string& name, ImVec2 p0, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !tex_) return false;
  ImGui::GetWindowDrawList()->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))), p0,
                                       ImVec2(p0.x + size, p0.y + size), it->second.uv0, it->second.uv1);
  return true;
}

bool UiIcons::icon_button(const char* id, const std::string& name, float size, bool on) const {
  if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  auto it = entries_.find(name);
  const bool pressed =
      it == entries_.end() || !tex_
          ? ImGui::Button(id, ImVec2(size, size))
          : ImGui::ImageButton(id, ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))),
                               ImVec2(size, size), it->second.uv0, it->second.uv1);
  if (on) ImGui::PopStyleColor();
  return pressed;
}

bool UiIcons::image(const std::string& name, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !tex_) return false;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))), ImVec2(size, size), it->second.uv0,
               it->second.uv1);
  return true;
}

}  // namespace urbs::game
