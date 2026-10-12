#include "game/panel_kit.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "game/style.hpp"
#include "game/ui_icons.hpp"

namespace urbs::game {

namespace {
float g_left = 0, g_right = 0;  // screen x of the current panel's content edges

// Text size in `f` at `size` (0: the current size), measured the way ImGui will draw it.
ImVec2 text_size(ImFont* f, float size, const char* s) {
  ImGui::PushFont(f, size);
  const ImVec2 r = ImGui::CalcTextSize(s);
  ImGui::PopFont();
  return r;
}

// A stone button with a minus or a plus drawn on it (the UI font lacks the glyphs).
bool glyph_button(const char* id, bool plus, float h) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::Button(id, ImVec2(h, h));
  const bool held = ImGui::IsItemActive() && ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 c(std::round(p.x + h * 0.5f), std::round(p.y + h * 0.5f + (held ? 1 : 0)));
  const float r = std::round(h * 0.22f), t = std::max(2.0f, std::round(h * 0.10f));
  dl->AddRectFilled(ImVec2(c.x - r, c.y - t * 0.5f), ImVec2(c.x + r, c.y + t * 0.5f), kColInk);
  if (plus) dl->AddRectFilled(ImVec2(c.x - t * 0.5f, c.y - r), ImVec2(c.x + t * 0.5f, c.y + r), kColInk);
  return pressed;
}

// Draws a skin piece, or a flat stand-in when the chrome is missing.
void piece(ImDrawList* dl, Skin s, ImVec2 a, ImVec2 b, ImU32 flat, ImU32 tint = IM_COL32_WHITE) {
  if (skinned()) {
    ui_skin()->draw(dl, s, a, b, tint);
  } else if (flat) {
    dl->AddRectFilled(a, b, flat, 3);
    dl->AddRect(a, b, kColBronze, 3, 0, 1.0f);
  }
}

// Cream text with a dark line under it, for lettering on Pompeian red.
void engraved(ImDrawList* dl, ImFont* f, float size, ImVec2 p, const char* text) {
  dl->AddText(f, size, ImVec2(p.x, p.y + 1.5f), IM_COL32(52, 14, 8, 170), text);
  dl->AddText(f, size, p, kColCream, text);
}
}  // namespace

bool begin_panel(const char* id, const Dock& d) {
  const ImVec2 ds = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(d.pos, ImGuiCond_Always, d.right ? ImVec2(1, 0) : ImVec2(0, 0));
  ImGui::SetNextWindowSizeConstraints(ImVec2(d.width, 0), ImVec2(d.width, d.max_h > 0 ? d.max_h : ds.y));
  const bool open = begin_window(id, nullptr,
                                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings);
  g_left = ImGui::GetCursorScreenPos().x;
  g_right = g_left + ImGui::GetContentRegionAvail().x;
  if (d.notch_x >= 0) {
    // A little gable on the top edge, pointing up at the button the panel hangs from.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float s = std::round(ImGui::GetFontSize() * 0.6f);
    const float x = std::clamp(std::round(d.notch_x), wp.x + s * 3, wp.x + ImGui::GetWindowWidth() - s * 3);
    dl->PushClipRectFullScreen();
    if (skinned()) {
      const ImVec2 n = ui_skin()->size(Skin::Notch);
      const float k = s * 2 / n.x;
      ui_skin()->draw(dl, Skin::Notch, ImVec2(x - n.x * k * 0.5f, wp.y - n.y * k + 3),
                      ImVec2(x + n.x * k * 0.5f, wp.y + 3));
    } else {
      const ImVec2 a(x - s, wp.y + 1), b(x, wp.y - s), c(x + s, wp.y + 1);
      dl->AddTriangleFilled(a, b, c, ImGui::GetColorU32(ImGuiCol_WindowBg));
      dl->AddLine(a, b, kColBronze, 2.0f);
      dl->AddLine(b, c, kColBronze, 2.0f);
    }
    dl->PopClipRect();
  }
  return open;
}

void end_panel() { ImGui::End(); }

float panel_left() { return g_left; }
float panel_right() { return g_right; }

void content_bounds() {
  g_left = ImGui::GetCursorScreenPos().x;
  g_right = g_left + ImGui::GetContentRegionAvail().x;
}

bool dialog_button(const char* label, float w) {
  const float fs = ImGui::GetFontSize();
  if (w <= 0) w = std::min(g_right - g_left, std::round(fs * 15.0f));
  ImGui::SetCursorScreenPos(ImVec2(std::round((g_left + g_right - w) * 0.5f), ImGui::GetCursorScreenPos().y));
  return ImGui::Button(label, ImVec2(w, std::round(fs * 2.0f)));
}

void framed_picture(const UiIcons& icons, const std::string& picture, Pic kind, ImVec2 p, float size) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 q(p.x + size, p.y + size);
  if (kind != Pic::Icon) {
    // On an ivory tile with a bronze rim: a building, or a portrait filling it.
    piece(dl, Skin::Tile, p, q, IM_COL32(246, 236, 210, 255));
    const float in = std::round(size * (kind == Pic::Thumb ? 0.09f : 0.1f));
    if (kind == Pic::Thumb)
      icons.draw_thumb(picture, ImVec2(p.x + in, p.y + in), size - 2 * in);
    else
      icons.draw_icon(picture, ImVec2(p.x + in, p.y + in), size - 2 * in);
  } else {
    // An icon on a round bronze medallion.
    if (skinned())
      ui_skin()->draw(dl, Skin::Medal, p, q);
    else
      piece(dl, Skin::Medal, p, q, IM_COL32(204, 158, 77, 255));
    const float is = std::round(size * 0.6f);
    icons.draw_icon(picture, ImVec2(std::round(p.x + (size - is) * 0.5f), std::round(p.y + (size - is) * 0.5f)), is);
  }
}

bool close_button(ImVec2 c, float d) {
  ImGui::SetCursorScreenPos(ImVec2(c.x - d * 0.5f, c.y - d * 0.5f));
  const bool clicked = ImGui::InvisibleButton("##close", ImVec2(d, d));
  const bool hot = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (skinned()) {
    ui_skin()->draw(dl, hot ? Skin::CloseHov : Skin::Close, ImVec2(c.x - d * 0.5f, c.y - d * 0.5f),
                    ImVec2(c.x + d * 0.5f, c.y + d * 0.5f));
  } else {
    const float r = d * 0.26f;
    const ImU32 col = hot ? kColInk : kColInk2;
    dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f);
    dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 2.0f);
  }
  if (hot) ImGui::SetTooltip("Close");
  return clicked;
}

namespace {
// The red band across a panel's top, inside its frame: returns its rectangle.
void title_band_rect(float h, ImVec2& a, ImVec2& b) {
  const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
  const float in = skinned() ? ui_skin()->border(Skin::Panel) : 2.0f;
  a = ImVec2(wp.x + in, wp.y + in);
  b = ImVec2(wp.x + ws.x - in, wp.y + in + h);
}
}  // namespace

bool panel_header(const UiIcons& icons, const std::string& picture, Pic kind, const std::string& title,
                  const std::string& sub, ImU32 sub_col) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  // The band: Pompeian red with gilt lines, the width of the panel, set into the top of the frame.
  const float bh = std::round(fs * 2.2f);
  ImVec2 ba, bb;
  title_band_rect(bh, ba, bb);
  piece(dl, Skin::Band, ba, bb, kColRed);
  // The picture hangs from the band: a medallion for an icon, a tile for a building.
  const float pic = std::round(fs * (kind == Pic::Thumb ? 4.0f : 3.5f));
  const ImVec2 pp(g_left - std::round(fs * 0.2f), ba.y + std::round(fs * 0.3f));
  framed_picture(icons, picture, kind, pp, pic);
  // Title in the band, summary under it.
  const float tx = pp.x + pic + std::round(fs * 0.6f), th = std::round(fs * 1.45f);
  engraved(dl, f.heading, th, ImVec2(tx, std::round(ba.y + (bh - th) * 0.5f)), title.c_str());
  const float sy = bb.y + std::round(fs * 0.3f);
  if (!sub.empty()) dl->AddText(f.body, fs, ImVec2(tx, sy), sub_col ? sub_col : kColInk2, sub.c_str());
  // Close button, at the band's right end.
  const float cd = std::round(bh * 0.66f);
  const bool closed = close_button(ImVec2(bb.x - std::round(bh * 0.55f), std::round((ba.y + bb.y) * 0.5f)), cd);
  const float bottom = std::max(pp.y + pic, sub.empty() ? bb.y : sy + fs * 1.1f) + std::round(fs * 0.35f);
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(g_right - g_left, bottom - p.y));
  return !closed;
}

void title_band(const char* title) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize(), bh = std::round(fs * 2.0f), th = std::round(fs * 1.3f);
  g_left = ImGui::GetCursorScreenPos().x;
  g_right = g_left + ImGui::GetContentRegionAvail().x;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImVec2 ba, bb;
  title_band_rect(bh, ba, bb);
  piece(dl, Skin::Band, ba, bb, kColRed);
  engraved(dl, f.heading, th, ImVec2(g_left, std::round(ba.y + (bh - th) * 0.5f)), title);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(0, std::max(0.0f, bb.y - p.y + std::round(fs * 0.25f))));
}

void menu_title(const char* title) {
  g_left = ImGui::GetCursorScreenPos().x;
  g_right = g_left + ImGui::GetContentRegionAvail().x;
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize(), th = std::round(fs * 1.55f);
  const ImVec2 tsz = text_size(f.heading, th, title);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  if (skinned()) {
    // A tabula ansata: the red plaque with dovetail handles.
    const float h = ui_skin()->size(Skin::Plaque).y;
    const float w = std::min(g_right - g_left, std::round(tsz.x + fs * 5.2f));
    const float x0 = std::round((g_left + g_right - w) * 0.5f);
    ui_skin()->draw(dl, Skin::Plaque, ImVec2(x0, p.y), ImVec2(x0 + w, p.y + h));
    engraved(dl, f.heading, th,
             ImVec2(std::round((g_left + g_right - tsz.x) * 0.5f), std::round(p.y + (h - tsz.y) * 0.5f)), title);
    ImGui::Dummy(ImVec2(g_right - g_left, h + std::round(fs * 0.5f)));
  } else {
    ImGui::PushFont(f.heading, th);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (g_right - g_left - tsz.x) * 0.5f));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    divider();
  }
}

void tag(const char* label, ImU32 col) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize(), ts = std::round(fs * 0.85f), h = std::round(fs * 1.35f);
  const ImVec2 sz = text_size(f.bold, ts, label);
  const float w = std::round(sz.x + fs);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col, h * 0.5f);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(60, 20, 10, 255), h * 0.5f, 0, 1.5f);
  dl->AddText(f.bold, ts, ImVec2(p.x + (w - sz.x) * 0.5f, p.y + (h - sz.y) * 0.5f), IM_COL32(255, 246, 228, 255),
              label);
  ImGui::Dummy(ImVec2(w, h));
}

void divider() {
  const float fs = ImGui::GetFontSize();
  ImGui::Dummy(ImVec2(0, std::round(fs * 0.2f)));
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float mid = std::round((g_left + g_right) * 0.5f);
  if (skinned()) {
    // A bronze rule fading at both ends, with a lozenge between laurel sprigs at its middle.
    const UiSkin& sk = *ui_skin();
    const ImVec2 o = sk.size(Skin::RuleOrn), r = sk.size(Skin::Rule);
    const float y = std::round(p.y + o.y * 0.5f);
    sk.draw(dl, Skin::Rule, ImVec2(g_left, y - r.y * 0.5f), ImVec2(g_right, y + r.y * 0.5f));
    sk.draw_at(dl, Skin::RuleOrn, ImVec2(mid, y));
    ImGui::Dummy(ImVec2(0, o.y + std::round(fs * 0.25f)));
  } else {
    const float y = std::round(p.y) + 0.5f, d = std::round(fs * 0.28f);
    dl->AddLine(ImVec2(g_left, y), ImVec2(g_right, y), IM_COL32(190, 165, 120, 255), 1.0f);
    dl->AddQuadFilled(ImVec2(mid - d, y), ImVec2(mid, y - d), ImVec2(mid + d, y), ImVec2(mid, y + d), kColBronze);
    ImGui::Dummy(ImVec2(0, std::round(fs * 0.45f)));
  }
}

void section(const char* label) {
  divider();
  ImGui::PushFont(ui_fonts().heading, std::round(ImGui::GetFontSize() * 0.86f));
  ImGui::PushStyleColor(ImGuiCol_Text, kColSection);
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void draw_bar(ImDrawList* dl, ImVec2 p, float width, float h, float frac, ImU32 col, float goal) {
  const float fw = width * std::clamp(frac, 0.0f, 1.0f);
  if (skinned()) {
    // Inlaid: a sunken track, the fill in the bar's colour with a highlight, a bronze goal marker.
    const UiSkin& sk = *ui_skin();
    sk.draw(dl, Skin::BarTrack, p, ImVec2(p.x + width, p.y + h));
    const float in = std::max(1.0f, std::round(h * 0.14f));
    if (fw > 0.5f) {
      // The fill picture runs a little dark: lift the colour so it reads like the flat colour did.
      ImVec4 c = col4(col);
      c.x = std::min(1.0f, c.x * 1.18f + 0.04f);
      c.y = std::min(1.0f, c.y * 1.18f + 0.04f);
      c.z = std::min(1.0f, c.z * 1.18f + 0.04f);
      sk.draw(dl, Skin::BarFill, ImVec2(p.x + in, p.y + in), ImVec2(p.x + std::max(fw - in, h - in), p.y + h - in),
              ImGui::ColorConvertFloat4ToU32(c));
    }
    if (goal >= 0) {
      const float gx = std::round(p.x + width * std::clamp(goal, 0.0f, 1.0f));
      const float tw = std::max(3.0f, std::round(h * 0.45f));
      sk.draw(dl, Skin::Tick, ImVec2(gx - tw * 0.5f, p.y - std::round(h * 0.45f)),
              ImVec2(gx + tw * 0.5f, p.y + h + std::round(h * 0.45f)));
    }
    return;
  }
  dl->AddRectFilled(p, ImVec2(p.x + width, p.y + h), kColTrack, h * 0.5f);
  if (fw > 0.5f) dl->AddRectFilled(p, ImVec2(p.x + std::max(fw, h), p.y + h), col, h * 0.5f);
  if (goal >= 0) {
    const float gx = std::round(p.x + width * std::clamp(goal, 0.0f, 1.0f));
    dl->AddLine(ImVec2(gx, p.y - h * 0.6f), ImVec2(gx, p.y + h * 1.6f), kColTick, 2.0f);
  }
}

void bar(float width, float frac, ImU32 col, float goal) {
  const float fs = ImGui::GetFontSize(), h = std::max(6.0f, std::round(fs * 0.5f));
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float y = std::round(p.y + (ImGui::GetTextLineHeight() - h) * 0.5f);
  draw_bar(ImGui::GetWindowDrawList(), ImVec2(p.x, y), width, h, frac, col, goal);
  ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));
}

void text_right(const std::string& s, ImU32 col, bool bold) {
  const UiFonts& f = ui_fonts();
  ImFont* font = bold ? f.bold : f.body;
  const float w = text_size(font, 0.0f, s.c_str()).x;
  ImGui::SameLine(0, 0);
  ImGui::SetCursorScreenPos(ImVec2(g_right - w, ImGui::GetCursorScreenPos().y));
  ImGui::PushFont(font, 0.0f);
  if (col) ImGui::PushStyleColor(ImGuiCol_Text, col);
  ImGui::TextUnformatted(s.c_str());
  if (col) ImGui::PopStyleColor();
  ImGui::PopFont();
}

void meter_row(const char* label, float frac, ImU32 col, const std::string& value, float goal, const char* tip) {
  const float fs = ImGui::GetFontSize();
  ImGui::BeginGroup();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(0, 0);
  const float bx = g_left + std::round(fs * 7.0f), bw = std::max(fs * 3, g_right - bx - std::round(fs * 4.6f));
  ImGui::SetCursorScreenPos(ImVec2(bx, ImGui::GetCursorScreenPos().y));
  bar(bw, frac, col, goal);
  text_right(value);
  ImGui::EndGroup();
  if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
}

void cell_right(const std::string& s, ImU32 col, bool bold) {
  ImGui::PushFont(bold ? ui_fonts().bold : ui_fonts().body, 0.0f);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(s.c_str()).x);
  if (col) ImGui::PushStyleColor(ImGuiCol_Text, col);
  ImGui::TextUnformatted(s.c_str());
  if (col) ImGui::PopStyleColor();
  ImGui::PopFont();
}

void table_headings(const char* const* names, int n) {
  ImGui::TableNextRow();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  for (int k = 0; k < n; ++k) {
    ImGui::TableNextColumn();
    if (k == 0)
      ImGui::TextUnformatted(names[k]);
    else
      cell_right(names[k]);
  }
  ImGui::PopStyleColor();
}

void value_row(const char* label, const std::string& value, ImU32 col) {
  ImGui::TextUnformatted(label);
  text_right(value, col);
}

bool selectable_button(const char* label, bool on, ImVec2 size) {
  if (!on) return ImGui::Button(label, size);
  // Selected: Pompeian red with a gilt edge and cream lettering (flat: the pressed colour).
  ImGui::PushStyleColor(ImGuiCol_Button,
                        skinned() ? skin_col4(Skin::ButtonSel) : ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        skinned() ? skin_col4(Skin::ButtonSel) : ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  ImGui::PushStyleColor(ImGuiCol_Text, kColCream);
  const bool r = ImGui::Button(label, size);
  ImGui::PopStyleColor(3);
  return r;
}

bool stepper(const char* id, int* v, int lo, int hi, const char* unit, int step) {
  const float h = ImGui::GetFrameHeight(), box = std::round(ImGui::GetFontSize() * 3.4f), gap = 4;
  ImGui::SameLine(0, 0);
  ImGui::SetCursorScreenPos(ImVec2(g_right - (h * 2 + box + gap * 2), ImGui::GetCursorScreenPos().y));
  ImGui::PushID(id);
  int nv = *v;
  if (glyph_button("##minus", false, h)) nv = std::max(lo, nv - step);
  ImGui::SameLine(0, gap);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  piece(dl, Skin::Frame, p, ImVec2(p.x + box, p.y + h), IM_COL32(246, 236, 210, 255));
  const std::string s = std::to_string(*v) + unit;
  const ImVec2 ts = text_size(ui_fonts().bold, 0.0f, s.c_str());
  dl->AddText(ui_fonts().bold, ImGui::GetFontSize(), ImVec2(p.x + (box - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
              ImGui::GetColorU32(ImGuiCol_Text), s.c_str());
  ImGui::Dummy(ImVec2(box, h));
  ImGui::SameLine(0, gap);
  if (glyph_button("##plus", true, h)) nv = std::min(hi, nv + step);
  ImGui::PopID();
  const bool changed = nv != *v;
  *v = nv;
  return changed;
}

bool choice(const char* id, int* v, const char* const* options, int n, float each_w) {
  const float fs = ImGui::GetFontSize();
  if (each_w <= 0)
    for (int k = 0; k < n; ++k) each_w = std::max(each_w, ImGui::CalcTextSize(options[k]).x + std::round(fs * 1.4f));
  ImGui::SameLine(0, 0);
  const float gap = skinned() ? 2.0f : 0.0f;
  ImGui::SetCursorScreenPos(ImVec2(g_right - each_w * static_cast<float>(n) - gap * static_cast<float>(n - 1),
                                   ImGui::GetCursorScreenPos().y));
  ImGui::PushID(id);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(skinned() ? 2.0f : 0.0f, ImGui::GetStyle().ItemSpacing.y));
  bool changed = false;
  for (int k = 0; k < n; ++k) {
    if (k) ImGui::SameLine();
    const bool on = *v == k;
    ImGui::PushID(k);
    if (selectable_button(options[k], on, ImVec2(each_w, 0)) && !on) {
      *v = k;
      changed = true;
    }
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  ImGui::PopID();
  return changed;
}

bool coin_button(const UiIcons& icons, const char* label, i64 cost, bool afford) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize(), pad = ImGui::GetStyle().FramePadding.x;
  const std::string c = with_commas(cost);
  const float lw = ImGui::CalcTextSize(label).x, cw = text_size(f.bold, 0.0f, c.c_str()).x;
  const float w = pad * 2 + lw + fs * 0.6f + fs + 4 + cw, h = ImGui::GetFrameHeight();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  if (!afford) ImGui::BeginDisabled();
  const bool pressed = ImGui::Button((std::string("##") + label).c_str(), ImVec2(w, h));
  if (!afford) ImGui::EndDisabled();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float ty = p.y + (h - fs) * 0.5f;
  const ImU32 ink = ImGui::GetColorU32(afford ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  dl->AddText(ImVec2(p.x + pad, ty), ink, label);
  const float cx = p.x + pad + lw + fs * 0.6f;
  icons.draw_icon("goods/denarii_16", ImVec2(cx, ty + 1), fs);
  dl->AddText(f.bold, fs, ImVec2(cx + fs + 4, ty), afford ? ink : kColBad, c.c_str());
  return pressed;
}

void check_mark(bool ok) {
  const float s = std::round(ImGui::GetFontSize() * 1.15f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float y = p.y + std::round((ImGui::GetTextLineHeight() - s) * 0.5f);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (skinned()) {
    ui_skin()->draw(dl, ok ? Skin::CheckOn : Skin::CheckOff, ImVec2(p.x, y), ImVec2(p.x + s, y + s));
  } else {
    const ImVec2 c(p.x + s * 0.5f, y + s * 0.5f);
    dl->AddCircleFilled(c, s * 0.5f, ok ? kColGood : IM_COL32(222, 206, 168, 255));
    dl->AddCircle(c, s * 0.5f, kColBronze, 0, 1.5f);
    if (ok) {
      const ImVec2 pts[] = {ImVec2(c.x - s * 0.24f, c.y + s * 0.02f), ImVec2(c.x - s * 0.06f, c.y + s * 0.2f),
                            ImVec2(c.x + s * 0.26f, c.y - s * 0.2f)};
      dl->AddPolyline(pts, 3, IM_COL32(250, 244, 226, 255), 0, std::max(2.0f, s * 0.13f));
    }
  }
  ImGui::Dummy(ImVec2(s, ImGui::GetTextLineHeight()));
}

void inline_icon(const UiIcons& icons, const std::string& sprite, float size) {
  const float s = size > 0 ? size : std::round(ImGui::GetFontSize() * 1.2f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  icons.draw_icon(sprite, ImVec2(p.x, std::round(p.y + (ImGui::GetTextLineHeight() - s) * 0.5f)), s);
  ImGui::Dummy(ImVec2(s, std::max(s, ImGui::GetTextLineHeight())));
}

void inline_thumb(const UiIcons& icons, const std::string& def_id, float size) {
  const float s = size > 0 ? size : std::round(ImGui::GetFontSize() * 1.8f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  icons.draw_thumb(def_id, ImVec2(p.x, std::round(p.y + (ImGui::GetTextLineHeight() - s) * 0.5f)), s);
  ImGui::Dummy(ImVec2(s, std::max(s, ImGui::GetTextLineHeight())));
}

void good_icon(const UiIcons& icons, const std::string& good_id, float size) {
  inline_icon(icons, "goods/" + good_id + "_16", size);
}

void good_chip(const UiIcons& icons, const std::string& good_id, const std::string& name) {
  const float fs = ImGui::GetFontSize(), is = std::round(fs * 1.25f), pad = std::round(fs * 0.45f);
  const float w = pad * 2 + is + fs * 0.35f + ImGui::CalcTextSize(name.c_str()).x, h = std::round(fs * 1.9f);
  if (ImGui::GetCursorScreenPos().x + w > g_right + 1) ImGui::NewLine();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  piece(dl, Skin::Frame, p, ImVec2(p.x + w, p.y + h), IM_COL32(246, 236, 210, 255));
  icons.draw_icon("goods/" + good_id + "_16", ImVec2(p.x + pad, std::round(p.y + (h - is) * 0.5f)), is);
  dl->AddText(ImVec2(p.x + pad + is + fs * 0.35f, std::round(p.y + (h - fs) * 0.5f)), ImGui::GetColorU32(ImGuiCol_Text),
              name.c_str());
  ImGui::Dummy(ImVec2(w, h));
}

bool link(const char* label) {
  ImGui::PushStyleColor(ImGuiCol_TextLink, kColLink);
  const bool r = ImGui::TextLink(label);
  ImGui::PopStyleColor();
  return r;
}

std::string with_commas(i64 v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}

std::string signed_commas(i64 v) { return (v > 0 ? "+" : "") + with_commas(v); }

}  // namespace urbs::game
