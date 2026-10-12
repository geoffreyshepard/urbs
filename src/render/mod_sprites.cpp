#include "render/mod_sprites.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "core/log.hpp"

namespace urbs::render {

namespace {

namespace fs = std::filesystem;

constexpr int kPad = 4;           // texels between sprites (3 mip levels: 1/4 still keeps them apart)
constexpr int kBleed = kPad / 2;  // colour spread into the transparent texels round each sprite

struct Anchor {
  int ax = 0, ay = 0;
};

// One decoded sprite waiting for a place on a page.
struct Item {
  std::string name;
  std::string shown;  // <mod>/sprites/<name>.png, for warnings
  size_t layer = 0;
  std::vector<u8> px;  // RGBA8 at atlas resolution
  int w = 0, h = 0;
  int ax = 0, ay = 0;  // atlas texels
  f32 scale = 1.0f;    // atlas texels per iso px
};

// bld/<stem>[_v<n>][_r<k>][_lit][@region]; the stem keeps any _t<k>.
struct NameParts {
  std::string stem;
  int v = -1, r = -1;
  bool lit = false, has_region = false;
  std::string region;
};

bool ends_with(const std::string& s, const char* tail) {
  const size_t n = std::strlen(tail);
  return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

// Removes a trailing <tag><digits> (value within [lo, hi]) and returns the value, or -1.
int take_num(std::string& s, const char* tag, int lo, int hi) {
  const size_t at = s.rfind(tag);
  if (at == std::string::npos) return -1;
  const size_t d = at + std::strlen(tag);
  if (d >= s.size() || s.size() - d > 2) return -1;
  int v = 0;
  for (size_t i = d; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  if (v < lo || v > hi) return -1;
  s.resize(at);
  return v;
}

NameParts split_name(const std::string& name) {
  NameParts p;
  std::string s = name;
  const size_t at = s.find('@');
  if (at != std::string::npos) {
    p.region = s.substr(at + 1);
    p.has_region = true;
    s.resize(at);
  }
  if (ends_with(s, "_lit")) {
    p.lit = true;
    s.resize(s.size() - 4);
  }
  p.r = take_num(s, "_r", 1, 3);
  p.v = take_num(s, "_v", 1, 99);
  p.stem = std::move(s);
  return p;
}

// Does the mod's sprite `m` stand for the same art as `b`, more generally (b only adds a variant,
// rotation, night overlay or region to it)?
bool generalises(const NameParts& m, const NameParts& b) {
  return m.stem == b.stem && (m.v < 0 || m.v == b.v) && (m.r < 0 || m.r == b.r) && (!m.lit || b.lit) &&
         (!m.has_region || (b.has_region && m.region == b.region));
}

bool is_bld(const std::string& name) { return name.rfind("bld/", 0) == 0; }
bool is_icon(const std::string& name) { return name.rfind("ui/", 0) == 0 || name.rfind("goods/", 0) == 0; }

// Reads {name: {"ax": int, "ay": int}} into `out` (later files win).
void read_anchors(const fs::path& file, const std::string& shown, std::unordered_map<std::string, Anchor>& out,
                  std::vector<std::string>& warn) {
  try {
    std::ifstream f(file);
    const nlohmann::json j = nlohmann::json::parse(f);
    if (!j.is_object()) {
      warn.push_back(shown + ": expected an object {\"bld/name\": {\"ax\": .., \"ay\": ..}}; ignored");
      return;
    }
    for (const auto& [name, e] : j.items()) {
      if (!e.is_object() || !e.contains("ax") || !e.contains("ay") || !e["ax"].is_number() || !e["ay"].is_number()) {
        warn.push_back(shown + ": \"" + name + "\" needs numbers \"ax\" and \"ay\"; ignored");
        continue;
      }
      out[name] = Anchor{static_cast<int>(std::lround(e["ax"].get<double>())),
                         static_cast<int>(std::lround(e["ay"].get<double>()))};
    }
  } catch (const std::exception& e) {
    warn.push_back(shown + ": cannot parse (" + std::string(e.what()) + "); ignored");
  }
}

// Area-weighted resample of straight-alpha RGBA8 to ow x oh (premultiplied while filtering, so
// transparent texels never darken the edges).
std::vector<u8> resample(const std::vector<u8>& src, int w, int h, int ow, int oh) {
  std::vector<f32> pm(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < pm.size(); i += 4) {
    const f32 a = src[i + 3] / 255.0f;
    for (int c = 0; c < 3; ++c) pm[i + c] = src[i + c] * a;
    pm[i + 3] = a;
  }
  // Horizontal pass (w -> ow), then vertical (h -> oh).
  auto pass = [](const std::vector<f32>& in, int n_in, int n_out, int lines, bool horizontal) {
    std::vector<f32> out(static_cast<size_t>(n_out) * lines * 4, 0.0f);
    const f64 step = static_cast<f64>(n_in) / n_out;
    for (int o = 0; o < n_out; ++o) {
      const f64 a = o * step, b = (o + 1) * step;
      const int i0 = static_cast<int>(std::floor(a)), i1 = std::min(n_in, static_cast<int>(std::ceil(b)));
      for (int i = i0; i < i1; ++i) {
        const f32 wgt = static_cast<f32>((std::min(b, i + 1.0) - std::max(a, static_cast<f64>(i))) / step);
        if (wgt <= 0) continue;
        for (int l = 0; l < lines; ++l) {
          const size_t si =
              horizontal ? (static_cast<size_t>(l) * n_in + i) * 4 : (static_cast<size_t>(i) * lines + l) * 4;
          const size_t di =
              horizontal ? (static_cast<size_t>(l) * n_out + o) * 4 : (static_cast<size_t>(o) * lines + l) * 4;
          for (int c = 0; c < 4; ++c) out[di + c] += in[si + c] * wgt;
        }
      }
    }
    return out;
  };
  const std::vector<f32> rows = pass(pm, w, ow, h, true);
  const std::vector<f32> both = pass(rows, h, oh, ow, false);
  std::vector<u8> out(static_cast<size_t>(ow) * oh * 4);
  for (size_t i = 0; i < out.size(); i += 4) {
    const f32 a = both[i + 3];
    for (int c = 0; c < 3; ++c)
      out[i + c] = static_cast<u8>(std::clamp(a > 0 ? both[i + c] / a + 0.5f : 0.0f, 0.0f, 255.0f));
    out[i + 3] = static_cast<u8>(std::clamp(a * 255.0f + 0.5f, 0.0f, 255.0f));
  }
  return out;
}

// Crops fully transparent margins (keeping one texel); the anchor moves with the crop.
void trim(Item& it) {
  int x0 = it.w, y0 = it.h, x1 = -1, y1 = -1;
  for (int y = 0; y < it.h; ++y)
    for (int x = 0; x < it.w; ++x)
      if (it.px[(static_cast<size_t>(y) * it.w + x) * 4 + 3] > 0) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
      }
  if (x1 < 0) {  // nothing visible: keep one transparent texel
    x0 = y0 = x1 = y1 = 0;
  } else {
    x0 = std::max(0, x0 - 1);
    y0 = std::max(0, y0 - 1);
    x1 = std::min(it.w - 1, x1 + 1);
    y1 = std::min(it.h - 1, y1 + 1);
  }
  const int nw = x1 - x0 + 1, nh = y1 - y0 + 1;
  if (nw == it.w && nh == it.h) return;
  std::vector<u8> out(static_cast<size_t>(nw) * nh * 4);
  for (int y = 0; y < nh; ++y)
    std::memcpy(&out[static_cast<size_t>(y) * nw * 4], &it.px[(static_cast<size_t>(y0 + y) * it.w + x0) * 4],
                static_cast<size_t>(nw) * 4);
  it.px = std::move(out);
  it.w = nw;
  it.h = nh;
  it.ax -= x0;
  it.ay -= y0;
}

// Spreads opaque colours into the transparent texels of a page rectangle (alpha untouched), so
// linear filtering and mips never pull in black fringes (as tools/atlas/pack.py does).
void bleed(u8* page, int pw, int ph, int x0, int y0, int x1, int y1) {
  x0 = std::max(0, x0);
  y0 = std::max(0, y0);
  x1 = std::min(pw, x1);
  y1 = std::min(ph, y1);
  const int w = x1 - x0, h = y1 - y0;
  if (w <= 0 || h <= 0) return;
  std::vector<u8> known(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      known[static_cast<size_t>(y) * w + x] = page[(static_cast<size_t>(y0 + y) * pw + x0 + x) * 4 + 3] > 0;
  std::vector<u8> next = known;
  for (int it = 0; it < 6; ++it) {
    bool grew = false;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        if (known[static_cast<size_t>(y) * w + x]) continue;
        int sum[3] = {0, 0, 0}, n = 0;
        const int nb[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (const auto& d : nb) {
          const int nx = x + d[0], ny = y + d[1];
          if (nx < 0 || ny < 0 || nx >= w || ny >= h || !known[static_cast<size_t>(ny) * w + nx]) continue;
          const u8* q = &page[(static_cast<size_t>(y0 + ny) * pw + x0 + nx) * 4];
          for (int c = 0; c < 3; ++c) sum[c] += q[c];
          ++n;
        }
        if (n == 0) continue;
        u8* p = &page[(static_cast<size_t>(y0 + y) * pw + x0 + x) * 4];
        for (int c = 0; c < 3; ++c) p[c] = static_cast<u8>((sum[c] + n / 2) / n);
        next[static_cast<size_t>(y) * w + x] = 1;
        grew = true;
      }
    if (!grew) break;
    known = next;
  }
}

}  // namespace

size_t ModSpritePack::count() const {
  size_t n = 0;
  for (const auto& l : layers) n += l.sprites.size();
  return n;
}

bool mod_sprite_footprint(const std::string& name,
                          const std::unordered_map<std::string, std::pair<int, int>>& footprints, int& fw, int& fh) {
  if (!is_bld(name)) return false;
  const NameParts p = split_name(name);
  auto it = footprints.find(p.stem);
  if (it == footprints.end()) {
    std::string base = p.stem;
    if (take_num(base, "_t", 0, 9) < 0) return false;
    it = footprints.find(base);
    if (it == footprints.end()) return false;
  }
  fw = it->second.first;
  fh = it->second.second;
  if (p.r == 1 || p.r == 3) std::swap(fw, fh);
  return true;
}

ModSpritePack pack_mod_sprites(const std::vector<std::string>& mod_dirs, const ModSpriteConfig& cfg) {
  ModSpritePack pack;
  pack.page_w = cfg.page_w;
  pack.page_h = cfg.page_h;
  if (cfg.page_w == 0 || cfg.page_h == 0) return pack;
  // v = page * page_h + y must fit a u16 (Sprite::v).
  const u32 v_pages = 65536u / cfg.page_h;
  const u32 max_pages = cfg.first_page >= v_pages ? 0 : std::min(cfg.max_pages, v_pages - cfg.first_page);
  std::vector<std::string>& warn = pack.warnings;
  std::vector<Item> items;

  for (const std::string& dir : mod_dirs) {
    const fs::path root = fs::path(dir) / "sprites";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) continue;
    const std::string mod = fs::path(dir).filename().string();
    const size_t layer = pack.layers.size();
    pack.layers.push_back(ModSpriteLayer{dir, {}});

    // Anchors: sprites/meta/*.json (what tools/artgen3d writes), then sprites/sprites.json.
    std::unordered_map<std::string, Anchor> anchors;
    std::vector<fs::path> metas;
    if (fs::is_directory(root / "meta", ec))
      for (const auto& e : fs::directory_iterator(root / "meta", ec))
        if (e.path().extension() == ".json" && e.path().filename().string()[0] != '.') metas.push_back(e.path());
    std::sort(metas.begin(), metas.end());
    if (fs::exists(root / "sprites.json", ec)) metas.push_back(root / "sprites.json");
    for (const auto& m : metas)
      read_anchors(m, mod + "/" + fs::relative(m, fs::path(dir), ec).generic_string(), anchors, warn);

    std::vector<std::pair<std::string, fs::path>> files;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec) break;
      if (it->is_directory(ec) && it->path().filename() == "meta" && it.depth() == 0) {
        it.disable_recursion_pending();
        continue;
      }
      if (!it->is_regular_file(ec) || it->path().extension() != ".png") continue;
      // Hidden files: macOS copies onto some drives and zip tools leave "._name.png" metadata.
      if (it->path().filename().string()[0] == '.') continue;
      std::string name = fs::relative(it->path(), root, ec).generic_string();
      name.resize(name.size() - 4);
      files.emplace_back(std::move(name), it->path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& [name, path] : files) {
      const std::string shown = mod + "/sprites/" + name + ".png";
      auto refuse = [&](const std::string& why) {
        warn.push_back(shown + ": " + why + "; drawn with the base sprite or a placeholder");
      };
      if (name.find('/') == std::string::npos) {
        refuse("must sit in a group folder named like the atlas (bld/, goods/, ui/, people/, obj/, ...)");
        continue;
      }
      const auto bytes = fs::file_size(path, ec);
      if (ec || bytes > static_cast<std::uintmax_t>(kModSpriteMaxBytes)) {
        refuse(ec ? "cannot read the file" : "file larger than 32 MB");
        continue;
      }
      const std::string p = path.string();
      int w = 0, h = 0, comp = 0;
      if (!stbi_info(p.c_str(), &w, &h, &comp)) {
        refuse(std::string("not a readable PNG (") + stbi_failure_reason() + ")");
        continue;
      }
      const bool icon = is_icon(name);
      const int limit = icon ? kModIconMaxPx : kModSpriteMaxPx;
      if (w <= 0 || h <= 0 || w > limit || h > limit) {
        refuse(std::to_string(w) + "x" + std::to_string(h) + " px is too big (at most " + std::to_string(limit) +
               " px a side)");
        continue;
      }
      Anchor a{w / 2, icon ? h / 2 : h};  // icons: centre; world sprites: bottom centre
      auto given = anchors.find(name);
      int fw = 0, fh = 0;
      if (given != anchors.end()) {
        a = given->second;
      } else if (!icon && mod_sprite_footprint(name, cfg.footprints, fw, fh)) {
        const int want = (fw + fh) * kIsoHalfTilePx;
        if (w != want || h < want / 2) {
          refuse(std::to_string(w) + "x" + std::to_string(h) + " px does not fit its " + std::to_string(fw) + "x" +
                 std::to_string(fh) + " footprint: the canvas must be (" + std::to_string(fw) + " + " +
                 std::to_string(fh) + ") x 64 = " + std::to_string(want) + " px wide and at least " +
                 std::to_string(want / 2) + " px tall (or give ax/ay in sprites.json)");
          continue;
        }
        a = Anchor{fw * kIsoHalfTilePx, h};  // the footprint's bottom (south-east) corner
      }
      int lw = 0, lh = 0;
      unsigned char* px = stbi_load(p.c_str(), &lw, &lh, &comp, 4);
      if (!px || lw != w || lh != h) {
        if (px) stbi_image_free(px);
        refuse(std::string("cannot decode (") + stbi_failure_reason() + ")");
        continue;
      }
      Item it;
      it.name = name;
      it.shown = shown;
      it.layer = layer;
      it.px.assign(px, px + static_cast<size_t>(w) * h * 4);
      stbi_image_free(px);
      it.w = w;
      it.h = h;
      it.ax = a.ax;
      it.ay = a.ay;
      if (!icon) {  // world art: down to the atlas scale like tools/atlas/pack.py, then trimmed
        it.scale = cfg.world_scale > 0 ? cfg.world_scale : 1.0f;
        if (it.scale != 1.0f) {
          const f64 k = it.scale;
          const int ow = std::max(1, static_cast<int>(std::lround(w * k)));
          const int oh = std::max(1, static_cast<int>(std::lround(h * k)));
          it.px = resample(it.px, w, h, ow, oh);
          it.w = ow;
          it.h = oh;
          it.ax = static_cast<int>(std::lround(a.ax * k));
          it.ay = static_cast<int>(std::lround(a.ay * k));
        }
        trim(it);
      }
      items.push_back(std::move(it));
    }
  }

  // Shelf packing, tallest first (ties: wider, then name, then load order).
  std::vector<size_t> order(items.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const Item &x = items[a], &y = items[b];
    if (x.h != y.h) return x.h > y.h;
    if (x.w != y.w) return x.w > y.w;
    if (x.name != y.name) return x.name < y.name;
    return x.layer < y.layer;
  });
  const int pw = static_cast<int>(cfg.page_w), ph = static_cast<int>(cfg.page_h);
  const size_t page_bytes = static_cast<size_t>(pw) * ph * 4;
  u32 page = 0;
  int x = kBleed, y = kBleed, shelf = 0;
  struct Placed {
    size_t item;
    u32 page;
    int x, y;
  };
  std::vector<Placed> placed;
  for (size_t idx : order) {
    const Item& it = items[idx];
    if (it.w + 2 * kBleed > pw || it.h + 2 * kBleed > ph) {
      warn.push_back(it.shown + ": larger than an atlas page; drawn with the base sprite or a placeholder");
      continue;
    }
    if (x > kBleed && x + it.w + kBleed > pw) {
      y += shelf + kPad;
      x = kBleed;
      shelf = 0;
    }
    if (y + it.h + kBleed > ph) {
      ++page;
      x = y = kBleed;
      shelf = 0;
    }
    if (page >= max_pages) {
      warn.push_back(it.shown + ": no room left in the " + std::to_string(max_pages) +
                     " atlas page(s) for mod sprites; drawn with the base sprite or a placeholder");
      continue;
    }
    placed.push_back(Placed{idx, page, x, y});
    x += it.w + kPad;
    shelf = std::max(shelf, it.h);
  }
  for (const Placed& p : placed) pack.pages = std::max(pack.pages, p.page + 1);
  pack.pixels.assign(page_bytes * pack.pages, 0);
  for (const Placed& p : placed) {
    const Item& it = items[p.item];
    u8* base = pack.pixels.data() + page_bytes * p.page;
    for (int r = 0; r < it.h; ++r)
      std::memcpy(base + (static_cast<size_t>(p.y + r) * pw + p.x) * 4, &it.px[static_cast<size_t>(r) * it.w * 4],
                  static_cast<size_t>(it.w) * 4);
  }
  // Bleed each sprite's rectangle plus its share of the padding (the rectangles never overlap).
  for (const Placed& p : placed) {
    const Item& it = items[p.item];
    bleed(pack.pixels.data() + page_bytes * p.page, pw, ph, p.x - kBleed, p.y - kBleed, p.x + it.w + kBleed,
          p.y + it.h + kBleed);
  }
  for (const Placed& p : placed) {
    const Item& it = items[p.item];
    Sprite s;
    s.u = static_cast<u16>(p.x);
    s.v = static_cast<u16>((cfg.first_page + p.page) * cfg.page_h + static_cast<u32>(p.y));
    s.w = static_cast<u16>(it.w);
    s.h = static_cast<u16>(it.h);
    s.ax = static_cast<i16>(it.ax);
    s.ay = static_cast<i16>(it.ay);
    s.iso_per_texel = 1.0f / it.scale;
    s.valid = true;
    pack.layers[it.layer].sprites.emplace_back(it.name, s);
  }
  for (auto& l : pack.layers)
    std::sort(l.sprites.begin(), l.sprites.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& w : warn) URBS_LOG_WARN("mod sprites: {}", w);
  if (pack.count() > 0)
    URBS_LOG_INFO("mod sprites: {} sprite(s) from {} mod(s) in {} extra atlas page(s)", pack.count(),
                  pack.layers.size(), pack.pages);
  return pack;
}

void merge_mod_sprites(std::unordered_map<std::string, Sprite>& table, const ModSpritePack& pack) {
  for (const ModSpriteLayer& layer : pack.layers) {
    if (layer.sprites.empty()) continue;
    std::unordered_set<std::string> names;
    std::unordered_map<std::string, std::vector<NameParts>> by_stem;
    for (const auto& [n, s] : layer.sprites) {
      names.insert(n);
      if (is_bld(n)) {
        NameParts p = split_name(n);
        by_stem[p.stem].push_back(std::move(p));
      }
    }
    for (auto it = table.begin(); it != table.end();) {
      if (is_bld(it->first) && names.count(it->first) == 0) {
        const NameParts b = split_name(it->first);
        auto f = by_stem.find(b.stem);
        if (f != by_stem.end() &&
            std::any_of(f->second.begin(), f->second.end(), [&](const NameParts& m) { return generalises(m, b); })) {
          it = table.erase(it);
          continue;
        }
      }
      ++it;
    }
    for (const auto& [n, s] : layer.sprites) table[n] = s;
  }
}

}  // namespace urbs::render
