#include "game/ui_skin.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

#include "core/log.hpp"
#include "game/ui_icons.hpp"

namespace urbs::game {

namespace {

const UiSkin* g_skin = nullptr;

// Sprite names in chrome.json, in Skin order.
constexpr const char* kNames[] = {"",
                                  "panel",
                                  "card",
                                  "inset",
                                  "frame",
                                  "frame_hov",
                                  "frame_act",
                                  "slot",
                                  "button",
                                  "button_hov",
                                  "button_act",
                                  "button_sel",
                                  "button_off",
                                  "medal",
                                  "medal_hov",
                                  "medal_act",
                                  "medal_sel",
                                  "tile",
                                  "tile_hov",
                                  "tile_act",
                                  "tile_sel",
                                  "tile_off",
                                  "hilite",
                                  "hilite_sel",
                                  "cornice",
                                  "column",
                                  "band",
                                  "plaque",
                                  "rule",
                                  "rule_orn",
                                  "meander",
                                  "bar_track",
                                  "bar_fill",
                                  "tick",
                                  "grab",
                                  "grab_act",
                                  "scroll_track",
                                  "scroll_grab",
                                  "scroll_grab_hov",
                                  "notch",
                                  "close",
                                  "close_hov",
                                  "check_on",
                                  "check_off",
                                  "scroll_track",
                                  "scroll_grab",
                                  "scroll_grab_hov"};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<size_t>(Skin::Count));

bool is_skin(ImU32 c) { return (c & 0xFFFFu) == 0x0301u; }  // R = 1, G = 3 (IM_COL32 packs R in the low byte)
Skin skin_of(ImU32 c) { return static_cast<Skin>((c >> IM_COL32_B_SHIFT) & 0xFFu); }
u8 alpha_of(ImU32 c) { return static_cast<u8>(c >> IM_COL32_A_SHIFT); }

// What a skin colour paints when it fills something other than a rectangle (a radio button's circle, a rounded
// rect drawn by hand): a plain colour in the piece's spirit.
ImU32 fallback(Skin s, u8 a) {
  ImU32 c;
  switch (s) {
    case Skin::Panel:
    case Skin::Card:
      c = IM_COL32(242, 231, 205, 255);
      break;
    case Skin::Inset:
    case Skin::Frame:
      c = IM_COL32(236, 224, 194, 255);
      break;
    case Skin::FrameHov:
      c = IM_COL32(248, 240, 222, 255);
      break;
    case Skin::FrameAct:
      c = IM_COL32(222, 206, 170, 255);
      break;
    case Skin::ButtonSel:
    case Skin::MedalSel:
    case Skin::TileSel:
      c = IM_COL32(142, 51, 36, 255);
      break;
    case Skin::ButtonHov:
    case Skin::MedalHov:
    case Skin::TileHov:
      c = IM_COL32(236, 214, 160, 255);
      break;
    case Skin::Grab:
    case Skin::ScrollGrab:
      c = IM_COL32(125, 82, 38, 255);
      break;
    case Skin::GrabAct:
    case Skin::ScrollGrabHov:
      c = IM_COL32(199, 146, 70, 255);
      break;
    case Skin::Hilite:
      c = IM_COL32(215, 165, 78, 60);
      break;
    case Skin::HiliteSel:
      c = IM_COL32(212, 154, 62, 110);
      break;
    default:
      c = IM_COL32(222, 204, 160, 255);
      break;
  }
  const u8 ca = static_cast<u8>((c >> IM_COL32_A_SHIFT) * a / 255);
  return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(ca) << IM_COL32_A_SHIFT);
}

// A faded skin colour on a button means ImGui is drawing it disabled: use the disabled picture. Window backgrounds
// ignore the fade (a half-transparent stone frame looks broken); everything else fades.
Skin resolve(Skin s, u8 a, u8& out_alpha) {
  out_alpha = 255;
  const bool faded = a < 200;
  switch (s) {
    case Skin::Button:
    case Skin::ButtonHov:
    case Skin::ButtonAct:
    case Skin::ButtonSel:
      return faded ? Skin::ButtonOff : s;
    case Skin::Tile:
    case Skin::TileHov:
    case Skin::TileAct:
    case Skin::TileSel:
      return faded ? Skin::TileOff : s;
    case Skin::Panel:
    case Skin::Card:
    case Skin::Inset:
    case Skin::Cornice:
    case Skin::Column:
    case Skin::Slot:
      return s;
    default:
      out_alpha = a;
      return s;
  }
}

}  // namespace

const UiSkin* ui_skin() { return g_skin; }
void set_ui_skin(const UiSkin* s) { g_skin = s; }

bool UiSkin::init(SDL_GPUDevice* dev, const std::string& data_root) {
  const std::string dir = data_root + "assets/sprites_iso/ui/chrome/";
  std::ifstream f(dir + "chrome.json");
  if (!f) {
    URBS_LOG_ERROR("ui chrome: cannot open {}chrome.json", dir);
    return false;
  }
  nlohmann::json j;
  try {
    f >> j;
  } catch (const std::exception& e) {
    URBS_LOG_ERROR("ui chrome: {}", e.what());
    return false;
  }
  int w = 0, h = 0, comp = 0;
  unsigned char* px = stbi_load((dir + "chrome.png").c_str(), &w, &h, &comp, 4);
  constexpr u32 kLevels = 3;
  if (!px || w % 4 || h % 4) {
    URBS_LOG_ERROR("ui chrome: cannot load {}chrome.png", dir);
    if (px) stbi_image_free(px);
    return false;
  }
  std::vector<u8> pixels(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(px);
  tex_w_ = static_cast<float>(w);
  tex_h_ = static_cast<float>(h);
  texels_per_point_ = j.value("texels_per_point", 2.0f);
  const auto& sprites = j.at("sprites");
  for (size_t k = 1; k < pieces_.size(); ++k) {
    if (!sprites.contains(kNames[k])) {
      URBS_LOG_ERROR("ui chrome: no sprite '{}'", kNames[k]);
      continue;
    }
    const auto& s = sprites.at(kNames[k]);
    Piece& p = pieces_[k];
    p.x = s.at("x").get<float>();
    p.y = s.at("y").get<float>();
    p.w = s.at("w").get<float>();
    p.h = s.at("h").get<float>();
    for (int i = 0; i < 4; ++i) {
      p.m[static_cast<size_t>(i)] = s.at("m")[static_cast<size_t>(i)].get<float>();
      p.o[static_cast<size_t>(i)] = s.at("o")[static_cast<size_t>(i)].get<float>();
    }
    p.tile_x = s.at("t")[0].get<int>() > 0;
    p.tile_y = s.at("t")[1].get<int>() > 0;
    p.inner = s.value("i", 0.0f);
    p.ok = true;
  }
  tex_ = create_mipmapped(dev, static_cast<u32>(w), static_cast<u32>(h), kLevels, std::move(pixels));
  return tex_ != nullptr;
}

void UiSkin::shutdown(SDL_GPUDevice* dev) {
  if (tex_) SDL_ReleaseGPUTexture(dev, tex_);
  tex_ = nullptr;
}

ImTextureRef UiSkin::ref() const { return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex_))); }

ImVec2 UiSkin::size(Skin s) const {
  const Piece& p = pieces_[static_cast<size_t>(s)];
  const float k = scale_ / texels_per_point_;
  return ImVec2((p.w - p.o[0] - p.o[2]) * k, (p.h - p.o[1] - p.o[3]) * k);
}

float UiSkin::edge(Skin s, int side) const {
  const Piece& p = pieces_[static_cast<size_t>(s)];
  return (p.m[static_cast<size_t>(side)] - p.o[static_cast<size_t>(side)]) * scale_ / texels_per_point_;
}

float UiSkin::border(Skin s) const { return pieces_[static_cast<size_t>(s)].inner * scale_ / texels_per_point_; }

// Calls quad(p0, p1, uv0, uv1) for each piece of the 9-slice of `p` over a..b (plus its outset). The middle row and
// column repeat the picture's centre when it tiles, so stone grain and parchment never stretch.
template <class Quad>
void UiSkin::nine(const Piece& p, ImVec2 a, ImVec2 b, Quad&& quad) const {
  if (!p.ok) return;
  const float k = scale_ / texels_per_point_;  // points per texel
  const float x0 = a.x - p.o[0] * k, y0 = a.y - p.o[1] * k, x3 = b.x + p.o[2] * k, y3 = b.y + p.o[3] * k;
  const float W = x3 - x0, H = y3 - y0;
  if (W <= 0.5f || H <= 0.5f) return;
  float ml = p.m[0] * k, mt = p.m[1] * k, mr = p.m[2] * k, mb = p.m[3] * k;
  if (ml + mr > W) {
    const float f = W / (ml + mr);
    ml *= f;
    mr *= f;
  }
  if (mt + mb > H) {
    const float f = H / (mt + mb);
    mt *= f;
    mb *= f;
  }
  const float xs[4] = {x0, x0 + ml, x3 - mr, x3}, ys[4] = {y0, y0 + mt, y3 - mb, y3};
  const float us[4] = {p.x / tex_w_, (p.x + p.m[0]) / tex_w_, (p.x + p.w - p.m[2]) / tex_w_, (p.x + p.w) / tex_w_};
  const float vs[4] = {p.y / tex_h_, (p.y + p.m[1]) / tex_h_, (p.y + p.h - p.m[3]) / tex_h_, (p.y + p.h) / tex_h_};
  for (int j = 0; j < 3; ++j) {
    if (ys[j + 1] - ys[j] < 0.01f) continue;
    const float py = (vs[j + 1] - vs[j]) * tex_h_ * k;  // the period, in points
    const bool ty = j == 1 && p.tile_y && py >= 4.0f;
    for (float y = ys[j]; y < ys[j + 1] - 0.01f;) {
      const float y1 = ty ? std::min(y + py, ys[j + 1]) : ys[j + 1];
      const float v1 = ty ? vs[j] + (y1 - y) / py * (vs[j + 1] - vs[j]) : vs[j + 1];
      for (int i = 0; i < 3; ++i) {
        if (xs[i + 1] - xs[i] < 0.01f) continue;
        const float px = (us[i + 1] - us[i]) * tex_w_ * k;
        const bool tx = i == 1 && p.tile_x && px >= 4.0f;
        for (float x = xs[i]; x < xs[i + 1] - 0.01f;) {
          const float x1 = tx ? std::min(x + px, xs[i + 1]) : xs[i + 1];
          const float u1 = tx ? us[i] + (x1 - x) / px * (us[i + 1] - us[i]) : us[i + 1];
          quad(ImVec2(x, y), ImVec2(x1, y1), ImVec2(us[i], vs[j]), ImVec2(u1, v1));
          x = x1;
        }
      }
      y = y1;
    }
  }
}

void UiSkin::draw(ImDrawList* dl, Skin s, ImVec2 a, ImVec2 b, ImU32 tint) const {
  if (!tex_ || s == Skin::None) return;
  dl->PushTexture(ref());
  nine(pieces_[static_cast<size_t>(s)], a, b, [&](ImVec2 p0, ImVec2 p1, ImVec2 uv0, ImVec2 uv1) {
    dl->PrimReserve(6, 4);
    dl->PrimRectUV(p0, p1, uv0, uv1, tint);
  });
  dl->PopTexture();
}

void UiSkin::draw_at(ImDrawList* dl, Skin s, ImVec2 c, float k, ImU32 tint) const {
  const ImVec2 sz = size(s);
  const ImVec2 h(sz.x * k * 0.5f, sz.y * k * 0.5f);
  draw(dl, s, ImVec2(c.x - h.x, c.y - h.y), ImVec2(c.x + h.x, c.y + h.y), tint);
}

void UiSkin::process(ImDrawData* dd) const {
  if (!tex_ || !dd) return;
  ImVector<ImDrawCmd>& cmds = cmds_;
  ImVector<ImDrawIdx>& idx = idx_;
  for (ImDrawList* dl : dd->CmdLists) {
    bool any = false;
    for (const ImDrawVert& v : dl->VtxBuffer)
      if (is_skin(v.col)) {
        any = true;
        break;
      }
    if (!any) continue;
    const int orig_vtx = dl->VtxBuffer.Size;
    cmds.resize(0);
    idx.resize(0);
    idx.reserve(dl->IdxBuffer.Size + 512);
    for (const ImDrawCmd& c : dl->CmdBuffer) {
      const ImDrawIdx* I = dl->IdxBuffer.Data + c.IdxOffset;
      if (c.UserCallback || c.ElemCount == 0) {
        ImDrawCmd n = c;
        n.IdxOffset = static_cast<unsigned>(idx.Size);
        for (unsigned i = 0; i < c.ElemCount; ++i) idx.push_back(I[i]);
        cmds.push_back(n);
        continue;
      }
      ImDrawCmd cur = c;
      cur.IdxOffset = static_cast<unsigned>(idx.Size);
      cur.ElemCount = 0;
      for (unsigned i = 0; i < c.ElemCount;) {
        if (i + 6 <= c.ElemCount) {
          const unsigned b = I[i];
          if (I[i + 1] == b + 1 && I[i + 2] == b + 2 && I[i + 3] == b && I[i + 4] == b + 2 && I[i + 5] == b + 3) {
            const ImDrawVert* q = dl->VtxBuffer.Data + c.VtxOffset + b;
            const ImU32 col = q[0].col;
            if (is_skin(col) && q[1].col == col && q[2].col == col && q[3].col == col) {
              const ImVec2 a(std::min(q[0].pos.x, q[2].pos.x), std::min(q[0].pos.y, q[2].pos.y));
              const ImVec2 e(std::max(q[0].pos.x, q[2].pos.x), std::max(q[0].pos.y, q[2].pos.y));
              u8 alpha = 255;
              const Skin s = resolve(skin_of(col), alpha_of(col), alpha);
              ImVec2 e2 = e;
              if (s == Skin::PScrollTrack || s == Skin::PScrollGrab || s == Skin::PScrollGrabHov)
                e2.x -= std::round(border(Skin::Panel) * 0.5f);
              if (cur.ElemCount) cmds.push_back(cur);
              ImDrawCmd sc = c;
              sc.TexRef = ref();
              sc.VtxOffset = static_cast<unsigned>(dl->VtxBuffer.Size);
              sc.IdxOffset = static_cast<unsigned>(idx.Size);
              sc.ElemCount = 0;
              if (static_cast<size_t>(s) < pieces_.size()) {
                const ImU32 tint = IM_COL32(255, 255, 255, alpha);
                nine(pieces_[static_cast<size_t>(s)], a, e2, [&](ImVec2 p0, ImVec2 p1, ImVec2 uv0, ImVec2 uv1) {
                  const auto base = static_cast<ImDrawIdx>(static_cast<unsigned>(dl->VtxBuffer.Size) - sc.VtxOffset);
                  dl->VtxBuffer.push_back(ImDrawVert{p0, uv0, tint});
                  dl->VtxBuffer.push_back(ImDrawVert{ImVec2(p1.x, p0.y), ImVec2(uv1.x, uv0.y), tint});
                  dl->VtxBuffer.push_back(ImDrawVert{p1, uv1, tint});
                  dl->VtxBuffer.push_back(ImDrawVert{ImVec2(p0.x, p1.y), ImVec2(uv0.x, uv1.y), tint});
                  for (int t : {0, 1, 2, 0, 2, 3}) idx.push_back(static_cast<ImDrawIdx>(base + t));
                  sc.ElemCount += 6;
                });
              }
              if (sc.ElemCount) cmds.push_back(sc);
              cur = c;
              cur.IdxOffset = static_cast<unsigned>(idx.Size);
              cur.ElemCount = 0;
              i += 6;
              continue;
            }
          }
        }
        idx.push_back(I[i]);
        idx.push_back(I[i + 1]);
        idx.push_back(I[i + 2]);
        cur.ElemCount += 3;
        i += 3;
      }
      if (cur.ElemCount) cmds.push_back(cur);
    }
    // Skin colours left on anything else get a plain colour.
    for (int v = 0; v < orig_vtx; ++v) {
      ImDrawVert& vx = dl->VtxBuffer.Data[v];
      if (is_skin(vx.col)) vx.col = fallback(skin_of(vx.col), alpha_of(vx.col));
    }
    dl->CmdBuffer.swap(cmds);
    dl->IdxBuffer.swap(idx);
  }
  int tv = 0, ti = 0;
  for (const ImDrawList* dl : dd->CmdLists) {
    tv += dl->VtxBuffer.Size;
    ti += dl->IdxBuffer.Size;
  }
  dd->TotalVtxCount = tv;
  dd->TotalIdxCount = ti;
}

}  // namespace urbs::game
