#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

#include <array>
#include <string>

#include "core/types.hpp"

namespace urbs::game {

// The UI's chrome: carved travertine frames, bronze trim, parchment, Pompeian-red plaques. The pictures come from
// tools/uiart/gen_chrome.py (assets/sprites_iso/ui/chrome/chrome.{png,json}), drawn as 9-slices.
//
// Two ways in. Custom drawing calls draw() with a piece. Plain ImGui widgets and windows are skinned without
// touching their code: the style gives them "skin colours" (an RGB that names a piece, see skin_col()), and after
// ImGui::Render() process() replaces every rectangle filled with such a colour by that piece's 9-slice, keeping the
// draw order. A skin colour used for anything but a plain rectangle (a radio button's circle) is painted with an
// ordinary colour instead.
enum class Skin : u8 {
  None = 0,
  Panel,  // big windows: shadow, carved travertine, bronze bead, parchment
  Card,   // tooltips, popups, toasts
  Inset,  // child areas: a sunken parchment well
  Frame,
  FrameHov,
  FrameAct,  // inputs, checkboxes, slider tracks
  Slot,      // a recess in the top bar's marble
  Button,
  ButtonHov,
  ButtonAct,
  ButtonSel,
  ButtonOff,
  Medal,
  MedalHov,
  MedalAct,
  MedalSel,  // round bronze-rimmed icon buttons
  Tile,
  TileHov,
  TileAct,
  TileSel,
  TileOff,  // square bronze-rimmed picture buttons (the build rail)
  Hilite,
  HiliteSel,  // a hovered / selected row
  Cornice,    // the top bar
  Column,     // the build rail
  Band,       // a panel's red title band
  Plaque,     // a menu's red title plaque (tabula ansata)
  Rule,
  RuleOrn,
  Meander,
  BarTrack,
  BarFill,  // a bar's fill in greys, tinted by the colour passed to draw()
  Tick,
  Grab,
  GrabAct,
  ScrollTrack,
  ScrollGrab,
  ScrollGrabHov,
  Notch,
  Close,
  CloseHov,
  CheckOn,
  CheckOff,
  PScrollTrack,  // a Panel's scrollbar: the same pictures, kept inside the frame's stone
  PScrollGrab,
  PScrollGrabHov,
  Count
};

// The colour that makes ImGui's rectangles come out as `s` (alpha carries a window's or a disabled widget's fade).
constexpr ImU32 skin_col(Skin s, u8 alpha = 255) { return IM_COL32(1, 3, static_cast<u8>(s), alpha); }
inline ImVec4 skin_col4(Skin s) { return ImGui::ColorConvertU32ToFloat4(skin_col(s)); }

class UiSkin {
 public:
  bool init(SDL_GPUDevice* dev, const std::string& data_root);
  void shutdown(SDL_GPUDevice* dev);
  [[nodiscard]] bool ready() const { return tex_ != nullptr; }
  // Points per UI point of art: follows the UI scale setting.
  void set_scale(float ui_scale) const { scale_ = ui_scale; }

  // Draws `s` over the rectangle a..b (its shadow or rim may reach outside). `tint` multiplies the picture.
  void draw(ImDrawList* dl, Skin s, ImVec2 a, ImVec2 b, ImU32 tint = IM_COL32_WHITE) const;
  // A piece at its own size (in points) centred on `c`, times `k`.
  void draw_at(ImDrawList* dl, Skin s, ImVec2 c, float k = 1.0f, ImU32 tint = IM_COL32_WHITE) const;
  // A piece's natural size in points (without its outset).
  [[nodiscard]] ImVec2 size(Skin s) const;
  // The width of a frame's carved border inside its rectangle, in points (Panel: stone and bronze).
  [[nodiscard]] float border(Skin s) const;
  // How far a piece's fixed edge reaches into its rectangle on `side` (0 left, 1 top, 2 right, 3 bottom), in
  // points: the cornice's fillet and mouldings, the column's capital and base.
  [[nodiscard]] float edge(Skin s, int side) const;

  // Rewrites skin-coloured rectangles in the frame's draw data (call after ImGui::Render()).
  void process(ImDrawData* dd) const;

 private:
  struct Piece {
    float x = 0, y = 0, w = 0, h = 0;  // texels
    std::array<float, 4> m{}, o{};     // margins, outset (left, top, right, bottom), texels
    float inner = 0;                   // the border's width inside the rectangle, texels
    bool tile_x = false, tile_y = false;
    bool ok = false;
  };
  template <class Quad>
  void nine(const Piece& p, ImVec2 a, ImVec2 b, Quad&& quad) const;
  [[nodiscard]] ImTextureRef ref() const;

  SDL_GPUTexture* tex_ = nullptr;
  float tex_w_ = 1, tex_h_ = 1, texels_per_point_ = 2;
  mutable float scale_ = 1;
  std::array<Piece, static_cast<size_t>(Skin::Count)> pieces_{};
  // process()'s scratch, kept between frames so a frame does not allocate.
  mutable ImVector<ImDrawCmd> cmds_;
  mutable ImVector<ImDrawIdx> idx_;
};

// The skin, for the UI code (null until App::init loaded it).
const UiSkin* ui_skin();
void set_ui_skin(const UiSkin* s);

}  // namespace urbs::game
