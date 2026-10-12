// Mod sprites (render/mod_sprites.hpp): name rules, anchors, packing and overrides, without a GPU.
#include <doctest/doctest.h>
#include <stb_image_write.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "render/mod_sprites.hpp"

using namespace urbs;
using render::ModSpriteConfig;
using render::ModSpritePack;
using render::Sprite;
namespace fs = std::filesystem;

namespace {

// A scratch folder for one test, removed when it goes out of scope.
struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() /
           ("urbs_mod_sprites_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

// Writes a w x h PNG filled with one colour; `opaque_rows` from the bottom are opaque, the rest transparent.
void write_png(const fs::path& p, int w, int h, u8 r, u8 g, u8 b, int opaque_rows = -1) {
  fs::create_directories(p.parent_path());
  std::vector<u8> px(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      u8* q = &px[(static_cast<size_t>(y) * w + x) * 4];
      q[0] = r;
      q[1] = g;
      q[2] = b;
      q[3] = opaque_rows < 0 || y >= h - opaque_rows ? 255 : 0;
    }
  REQUIRE(stbi_write_png(p.string().c_str(), w, h, 4, px.data(), w * 4) != 0);
}

void write_text(const fs::path& p, const std::string& s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << s;
}

const Sprite* find(const ModSpritePack& pack, size_t layer, const std::string& name) {
  for (const auto& [n, s] : pack.layers.at(layer).sprites)
    if (n == name) return &s;
  return nullptr;
}

// The RGBA of a pack texel at sprite-relative (x, y).
const u8* texel(const ModSpritePack& pack, const ModSpriteConfig& cfg, const Sprite& s, int x, int y) {
  const size_t row = s.v - cfg.first_page * cfg.page_h + static_cast<size_t>(y);
  return &pack.pixels[(row * pack.page_w + s.u + static_cast<size_t>(x)) * 4];
}

bool has_warning(const ModSpritePack& pack, const std::string& part) {
  for (const auto& w : pack.warnings)
    if (w.find(part) != std::string::npos) return true;
  return false;
}

ModSpriteConfig small_config() {
  ModSpriteConfig cfg;
  cfg.page_w = 1024;
  cfg.page_h = 512;
  cfg.first_page = 7;
  cfg.world_scale = 0.5f;
  cfg.footprints = {{"bld/hall", {3, 2}}, {"bld/house_plot", {2, 2}}, {"bld/bad", {2, 2}}};
  return cfg;
}

}  // namespace

TEST_CASE("Mod sprite names map to building footprints") {
  const auto fp = small_config().footprints;
  int fw = 0, fh = 0;
  CHECK(render::mod_sprite_footprint("bld/hall", fp, fw, fh));
  CHECK((fw == 3 && fh == 2));
  CHECK(render::mod_sprite_footprint("bld/hall_r1", fp, fw, fh));  // odd turns swap the footprint
  CHECK((fw == 2 && fh == 3));
  CHECK(render::mod_sprite_footprint("bld/hall_r2_lit@east", fp, fw, fh));
  CHECK((fw == 3 && fh == 2));
  CHECK(render::mod_sprite_footprint("bld/house_plot_t2_v1_r3_lit@egypt", fp, fw, fh));
  CHECK((fw == 2 && fh == 2));
  CHECK_FALSE(render::mod_sprite_footprint("bld/hall_c", fp, fw, fh));  // line pieces are one tile
  CHECK_FALSE(render::mod_sprite_footprint("bld/unknown", fp, fw, fh));
  CHECK_FALSE(render::mod_sprite_footprint("goods/hall", fp, fw, fh));
}

TEST_CASE("Mod sprites are packed with their anchors, and bad files are refused") {
  TempDir tmp;
  const fs::path a = tmp.path / "mod_a", b = tmp.path / "mod_b";
  // A 3x2 building: (3 + 2) * 64 = 320 px wide; the lower 100 rows opaque.
  write_png(a / "sprites/bld/hall.png", 320, 240, 200, 40, 30, 100);
  write_png(a / "sprites/bld/hall_r1.png", 320, 200, 10, 200, 30);
  write_png(a / "sprites/goods/paste_16.png", 64, 64, 90, 90, 200);
  write_png(a / "sprites/obj/stone.png", 40, 30, 120, 120, 120);
  write_text(a / "sprites/sprites.json", R"({"obj/stone": {"ax": 10, "ay": 24}})");
  write_png(a / "sprites/bld/bad.png", 250, 200, 1, 2, 3);            // a 2x2 footprint needs 256 px
  write_text(a / "sprites/bld/junk.png", "not a png");                // unreadable
  write_text(a / "sprites/bld/._hall.png", "AppleDouble");            // macOS metadata: skipped, no warning
  write_png(a / "sprites/loose.png", 16, 16, 1, 2, 3);                // no group folder
  write_png(a / "sprites/obj/huge.png", 3000, 20, 1, 2, 3);           // too big
  write_png(b / "sprites/goods/paste_16.png", 64, 64, 250, 250, 10);  // a later mod overrides mod_a's icon
  fs::create_directories(tmp.path / "mod_c");                         // no sprites/ folder: no layer

  const ModSpriteConfig cfg = small_config();
  const ModSpritePack pack =
      render::pack_mod_sprites({a.string(), b.string(), (tmp.path / "mod_c").string(), "/no/such/mod"}, cfg);
  REQUIRE(pack.layers.size() == 2);
  CHECK(pack.layers[0].sprites.size() == 4);
  CHECK(pack.layers[1].sprites.size() == 1);
  CHECK(pack.warnings.size() == 4);
  CHECK(has_warning(pack, "bld/bad.png: 250x200 px does not fit its 2x2 footprint"));
  CHECK(has_warning(pack, "bld/junk.png: not a readable PNG"));
  CHECK(has_warning(pack, "loose.png: must sit in a group folder"));
  CHECK(has_warning(pack, "obj/huge.png: 3000x20 px is too big"));
  REQUIRE(pack.pages == 1);
  CHECK(pack.pixels.size() == size_t{1024} * 512 * 4);

  // World art is resampled to the atlas scale (0.5), trimmed to its opaque rows, anchored on the
  // footprint's bottom corner: ax = fw * 64 at native scale.
  const Sprite* hall = find(pack, 0, "bld/hall");
  REQUIRE(hall);
  CHECK(hall->valid);
  CHECK(hall->w == 160);
  CHECK(hall->h == 51);  // 50 opaque rows + one transparent row kept above them
  CHECK(hall->ax == 96);
  CHECK(hall->ay == 51);
  CHECK(hall->iso_per_texel == doctest::Approx(2.0f));
  CHECK(hall->iso_w() == doctest::Approx(320.0f));
  CHECK(hall->v >= 7 * 512);
  const u8* c = texel(pack, cfg, *hall, 5, hall->h - 1);
  CHECK((c[0] == 200 && c[1] == 40 && c[2] == 30 && c[3] == 255));
  const u8* top = texel(pack, cfg, *hall, 5, 0);  // transparent, but its colour is bled from below
  CHECK(top[3] == 0);
  CHECK(top[0] == 200);

  const Sprite* r1 = find(pack, 0, "bld/hall_r1");
  REQUIRE(r1);
  CHECK(r1->ax == 64);  // turned: the footprint is 2x3
  CHECK(r1->ay == 100);

  const Sprite* stone = find(pack, 0, "obj/stone");  // anchor from sprites.json
  REQUIRE(stone);
  CHECK(stone->w == 20);
  CHECK(stone->ax == 5);
  CHECK(stone->ay == 12);

  const Sprite* icon = find(pack, 0, "goods/paste_16");  // icons: 1:1, anchored at the centre
  REQUIRE(icon);
  CHECK(icon->w == 64);
  CHECK(icon->ax == 32);
  CHECK(icon->ay == 32);
  CHECK(icon->iso_per_texel == doctest::Approx(1.0f));
  const Sprite* icon_b = find(pack, 1, "goods/paste_16");
  REQUIRE(icon_b);
  CHECK(texel(pack, cfg, *icon_b, 3, 3)[0] == 250);

  // Merged over a base table: same names are replaced, the base's turned / night / regional
  // versions of a replaced building are hidden unless the mod ships them, the later mod wins.
  std::unordered_map<std::string, Sprite> table;
  Sprite base;
  base.valid = true;
  base.u = 1;
  for (const char* n : {"bld/hall", "bld/hall_r1", "bld/hall_r2", "bld/hall_lit", "bld/hall_r1_lit", "bld/hall@east",
                        "bld/hall_v1", "bld/hall_t2", "bld/hall_c", "bld/hallway", "goods/paste_16", "goods/paste"})
    table[n] = base;
  render::merge_mod_sprites(table, pack);
  CHECK(table.at("bld/hall").u == hall->u);
  CHECK(table.at("bld/hall").v == hall->v);
  CHECK(table.at("bld/hall_r1").v == r1->v);
  for (const char* gone : {"bld/hall_r2", "bld/hall_lit", "bld/hall_r1_lit", "bld/hall@east", "bld/hall_v1"})
    CHECK_MESSAGE(table.count(gone) == 0, gone);
  for (const char* kept : {"bld/hall_t2", "bld/hall_c", "bld/hallway", "goods/paste"})
    CHECK_MESSAGE(table.at(kept).u == 1, kept);
  CHECK(table.at("goods/paste_16").v == icon_b->v);
  CHECK(table.at("goods/paste_16").u == icon_b->u);
  CHECK(table.count("obj/stone") == 1);
}

TEST_CASE("Mod sprites that do not fit the allowed pages are refused, not crashed on") {
  TempDir tmp;
  const fs::path m = tmp.path / "mod";
  for (int k = 0; k < 6; ++k) write_png(m / ("sprites/obj/rock_" + std::to_string(k) + ".png"), 200, 200, 9, 9, 9);
  write_text(m / "sprites/sprites.json", "{ broken");
  ModSpriteConfig cfg = small_config();
  cfg.page_w = 256;
  cfg.page_h = 128;
  cfg.max_pages = 2;
  const ModSpritePack pack = render::pack_mod_sprites({m.string()}, cfg);
  CHECK(pack.pages == 2);
  CHECK(pack.count() == 4);  // 100 x 100 texels each at scale 0.5: two side by side per page
  CHECK(has_warning(pack, "sprites.json: cannot parse"));
  CHECK(has_warning(pack, "no room left"));
  for (const auto& [n, s] : pack.layers.at(0).sprites) CHECK(s.v + s.h <= (cfg.first_page + 2) * cfg.page_h);

  // The v coordinate is a u16: no page may start past it.
  cfg.first_page = 65536 / cfg.page_h;
  CHECK(render::pack_mod_sprites({m.string()}, cfg).count() == 0);
}
