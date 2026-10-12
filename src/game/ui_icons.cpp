#include "game/ui_icons.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>

#include "game/style.hpp"
#include "render/gpu.hpp"

namespace urbs::game {

// RGBA texture with a full mip chain built on the CPU (alpha-weighted 2x2 boxes, so transparent
// texels never darken the edges).  w and h must be divisible by 2^(levels - 1).
SDL_GPUTexture* create_mipmapped(SDL_GPUDevice* dev, u32 w, u32 h, u32 levels, std::vector<u8> px) {
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = w;
  ti.height = h;
  ti.layer_count_or_depth = 1;
  ti.num_levels = levels;
  SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev, &ti);
  if (!tex) return nullptr;
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xf;
  for (u32 l = 0; l < levels; ++l) {
    if (l > 0) {  // halve the previous level
      const u32 nw = w / 2, nh = h / 2;
      std::vector<u8> nx(static_cast<size_t>(nw) * nh * 4);
      for (u32 y = 0; y < nh; ++y)
        for (u32 x = 0; x < nw; ++x) {
          float c[4] = {0, 0, 0, 0}, plain[3] = {0, 0, 0};
          for (u32 k = 0; k < 4; ++k) {
            const u8* q = &px[(static_cast<size_t>(y * 2 + k / 2) * w + x * 2 + k % 2) * 4];
            const float a = q[3] / 255.0f;
            for (int i = 0; i < 3; ++i) {
              c[i] += q[i] * a;
              plain[i] += q[i];
            }
            c[3] += a;
          }
          u8* d = &nx[(static_cast<size_t>(y) * nw + x) * 4];
          for (int i = 0; i < 3; ++i) d[i] = static_cast<u8>(c[3] > 0 ? c[i] / c[3] : plain[i] / 4.0f);
          d[3] = static_cast<u8>(std::lround(c[3] / 4.0f * 255.0f));
        }
      px = std::move(nx);
      w = nw;
      h = nh;
    }
    SDL_GPUTransferBufferCreateInfo xi{};
    xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xi.size = w * h * 4;
    SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev, &xi);
    std::memcpy(SDL_MapGPUTransferBuffer(dev, xb, false), px.data(), xi.size);
    SDL_UnmapGPUTransferBuffer(dev, xb);
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = xb;
    SDL_GPUTextureRegion dst{};
    dst.texture = tex;
    dst.mip_level = l;
    dst.w = w;
    dst.h = h;
    dst.d = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    xf.push_back(xb);
  }
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev);
  for (auto* x : xf) SDL_ReleaseGPUTransferBuffer(dev, x);
  return tex;
}

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
  // Rendered icons (bigger than the 32 px pixel art) are kept apart: copied 1:1, mipmapped.
  std::vector<std::string> hires;
  {
    std::vector<std::string> pixel;
    for (const std::string& n : names) {
      const render::Sprite s = atlas.find(n);
      (s.valid && (s.w > 32 || s.h > 32) ? hires : pixel).push_back(n);
    }
    names = std::move(pixel);
  }
  constexpr int kScale = 6, kCell = 32 * kScale, kCols = 8;
  // At least one row: every icon may be a rendered one, and a texture cannot be empty.
  const int rows = std::max(1, static_cast<int>((names.size() + kCols - 1) / kCols));
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
  if (!hires.empty()) {
    int cell = 64;  // power of two holding the largest icon, so every mip level keeps cells apart
    for (const std::string& n : hires) {
      const render::Sprite sp = atlas.find(n);
      while (cell < sp.w || cell < sp.h) cell *= 2;
    }
    constexpr int kHCols = 8;
    constexpr u32 kLevels = 4;  // down to 1/8: a 64 px icon still has 8 texels at 8 px on screen
    const int hrows = static_cast<int>((hires.size() + kHCols - 1) / kHCols);
    const u32 HW = static_cast<u32>(kHCols * cell), HH = static_cast<u32>(hrows * cell);
    std::vector<u8> hp(static_cast<size_t>(HW) * HH * 4, 0);
    for (size_t k = 0; k < hires.size(); ++k) {
      const render::Sprite sp = atlas.find(hires[k]);
      const int cx = static_cast<int>(k % kHCols) * cell + (cell - sp.w) / 2;
      const int cy = static_cast<int>(k / kHCols) * cell + (cell - sp.h) / 2;
      for (int y = 0; y < sp.h; ++y)
        std::memcpy(&hp[(static_cast<size_t>(cy + y) * HW + static_cast<size_t>(cx)) * 4],
                    &src[(static_cast<size_t>(sp.v + y) * atlas.width() + sp.u) * 4], static_cast<size_t>(sp.w) * 4);
      entries_[hires[k]] = Entry{
          ImVec2(static_cast<float>(cx) / static_cast<float>(HW), static_cast<float>(cy) / static_cast<float>(HH)),
          ImVec2(static_cast<float>(cx + sp.w) / static_cast<float>(HW),
                 static_cast<float>(cy + sp.h) / static_cast<float>(HH)),
          true};
    }
    hires_ = create_mipmapped(dev, HW, HH, kLevels, std::move(hp));
  }
  // Big pictures in their own mipmapped texture, stacked vertically: pixel art (province map) upscaled x3
  // with nearest neighbour, rendered art (the logo from tools/artgen3d/ui_art.py, 1 texel per iso px)
  // copied 1:1.  Each picture starts on a multiple of 8 rows so mip levels never mix two pictures.
  {
    const char* bigs[] = {"ui/province_map", "ui/logo"};
    constexpr u32 kLevels = 4, kAlign = 1u << (kLevels - 1);
    auto scale_of = [](const render::Sprite& s) { return s.iso_per_texel > 1.5f ? 3u : 1u; };
    auto align = [](u32 v) { return (v + kAlign - 1) / kAlign * kAlign; };
    u32 BW = 0, BH = 0;
    for (const char* n : bigs) {
      const render::Sprite s = atlas.find(n);
      if (!s.valid) continue;
      BW = std::max<u32>(BW, align(s.w * scale_of(s)));
      BH += align(s.h * scale_of(s));
    }
    if (BW > 0) {
      std::vector<u8> bp(static_cast<size_t>(BW) * BH * 4, 0);
      u32 oy = 0;
      for (const char* n : bigs) {
        const render::Sprite s = atlas.find(n);
        if (!s.valid) continue;
        const u32 k = scale_of(s);
        for (u32 y = 0; y < static_cast<u32>(s.h) * k; ++y)
          for (u32 x = 0; x < static_cast<u32>(s.w) * k; ++x) {
            const size_t si = (static_cast<size_t>(s.v + y / k) * atlas.width() + s.u + x / k) * 4;
            const size_t di = (static_cast<size_t>(oy + y) * BW + x) * 4;
            for (int c = 0; c < 4; ++c) bp[di + c] = src[si + c];
          }
        big_entries_[n] = Entry{ImVec2(0, static_cast<float>(oy) / BH),
                                ImVec2(static_cast<float>(s.w * k) / BW, static_cast<float>(oy + s.h * k) / BH)};
        oy += align(s.h * k);
      }
      big_ = create_mipmapped(dev, BW, BH, kLevels, std::move(bp));
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
  for (SDL_GPUTexture* t : {tex_, hires_, big_, thumbs_, thumbs_old_})
    if (t) SDL_ReleaseGPUTexture(dev, t);
  tex_ = hires_ = big_ = thumbs_ = thumbs_old_ = nullptr;
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

// Top-left texel of a piece: pieces are ground-attached iso sprites anchored at the bottom corner of
// their tile (docs/13-isometric.md); tile (tx, ty) of the little layout sits on the iso grid.
void piece_origin(const render::Atlas& atlas, const Piece& p, int& x, int& y) {
  const float k = 1.0f / p.s.iso_per_texel;  // texels per iso px
  (void)atlas;
  x = static_cast<int>(std::lround(static_cast<float>(p.tx - p.ty) * 64.0f * k)) - p.s.ax;
  y = static_cast<int>(std::lround((static_cast<float>(p.tx + p.ty) + 2.0f) * 32.0f * k)) - p.s.ay;
}

// Paints the pieces in order onto one canvas, cropped to the opaque pixels.
Canvas compose(const render::Atlas& atlas, const std::vector<Piece>& pieces) {
  Canvas c;
  int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
  for (const Piece& p : pieces) {
    int x, y;
    piece_origin(atlas, p, x, y);
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
    piece_origin(atlas, p, ox, oy);
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
      if (hut.valid) p.push_back({hut, 0, 1});  // on the front-left tile, as in the world view
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

ImTextureRef UiIcons::ref(const Entry& e) const {
  return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(e.hires ? hires_ : tex_)));
}

bool UiIcons::draw_icon(const std::string& name, ImVec2 p0, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !(it->second.hires ? hires_ : tex_)) return false;
  ImGui::GetWindowDrawList()->AddImage(ref(it->second), p0, ImVec2(p0.x + size, p0.y + size), it->second.uv0,
                                       it->second.uv1);
  return true;
}

bool UiIcons::icon_button(const char* id, const std::string& name, float size, bool on, bool tile) const {
  // Skinned: a round bronze medallion (or a square tile), Pompeian red when on.
  int pushed = 0;
  if (skinned()) {
    const Skin n = tile ? Skin::Tile : Skin::Medal, h = tile ? Skin::TileHov : Skin::MedalHov,
               a = tile ? Skin::TileAct : Skin::MedalAct, sel = tile ? Skin::TileSel : Skin::MedalSel;
    ImGui::PushStyleColor(ImGuiCol_Button, skin_col4(on ? sel : n));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, skin_col4(on ? sel : h));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, skin_col4(a));
    pushed = 3;
  } else if (on) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    pushed = 1;
  }
  auto it = entries_.find(name);
  const bool pressed =
      it == entries_.end() || !(it->second.hires ? hires_ : tex_)
          ? ImGui::Button(id, ImVec2(size, size))
          : ImGui::ImageButton(id, ref(it->second), ImVec2(size, size), it->second.uv0, it->second.uv1);
  ImGui::PopStyleColor(pushed);
  return pressed;
}

bool UiIcons::image(const std::string& name, float size) const {
  auto it = entries_.find(name);
  if (it == entries_.end() || !(it->second.hires ? hires_ : tex_)) return false;
  ImGui::Image(ref(it->second), ImVec2(size, size), it->second.uv0, it->second.uv1);
  return true;
}

}  // namespace urbs::game
