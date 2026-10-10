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

// A button frame drawn by hand (for the stepper's − and +, which the UI font lacks).
bool glyph_button(const char* id, bool plus, float h) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(h, h));
  const bool hot = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 fill = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hot ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
  dl->AddRectFilled(p, ImVec2(p.x + h, p.y + h), fill, ImGui::GetStyle().FrameRounding);
  dl->AddRect(p, ImVec2(p.x + h, p.y + h), ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
  const ImVec2 c(std::round(p.x + h * 0.5f), std::round(p.y + h * 0.5f));
  const float r = std::round(h * 0.24f), t = std::max(2.0f, std::round(h * 0.09f));
  const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
  dl->AddRectFilled(ImVec2(c.x - r, c.y - t * 0.5f), ImVec2(c.x + r, c.y + t * 0.5f), ink);
  if (plus) dl->AddRectFilled(ImVec2(c.x - t * 0.5f, c.y - r), ImVec2(c.x + t * 0.5f, c.y + r), ink);
  return pressed;
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
    // A notch on the top edge, pointing up at the button the panel hangs from.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float s = std::round(ImGui::GetFontSize() * 0.55f);
    const float x = std::clamp(std::round(d.notch_x), wp.x + s * 2, wp.x + ImGui::GetWindowWidth() - s * 2);
    dl->PushClipRectFullScreen();
    const ImVec2 a(x - s, wp.y + 1), b(x, wp.y - s), c(x + s, wp.y + 1);
    dl->AddTriangleFilled(a, b, c, ImGui::GetColorU32(ImGuiCol_WindowBg));
    dl->AddLine(a, b, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().WindowBorderSize);
    dl->AddLine(b, c, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().WindowBorderSize);
    dl->PopClipRect();
  }
  return open;
}

void end_panel() { ImGui::End(); }

bool panel_header(const UiIcons& icons, const std::string& picture, bool thumb, const std::string& title,
                  const std::string& sub, ImU32 sub_col) {
  const UiFonts& f = ui_fonts();
  const float fs = ImGui::GetFontSize();
  const float pic = std::round(fs * (thumb ? 4.4f : 3.2f));
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  // The picture, framed like the rail's buttons (a building sits on a light ground).
  const float rnd = ImGui::GetStyle().FrameRounding;
  dl->AddRectFilled(p, ImVec2(p.x + pic, p.y + pic),
                    thumb ? IM_COL32(246, 236, 210, 255) : ImGui::GetColorU32(ImGuiCol_Button), rnd);
  dl->AddRect(p, ImVec2(p.x + pic, p.y + pic), ImGui::GetColorU32(ImGuiCol_Border), rnd, 0, 1.5f);
  if (thumb) {
    icons.draw_thumb(picture, ImVec2(p.x + 3, p.y + 3), pic - 6);
  } else {
    const float is = std::round(pic * 0.7f);
    icons.draw_icon(picture, ImVec2(std::round(p.x + (pic - is) * 0.5f), std::round(p.y + (pic - is) * 0.5f)), is);
  }
  // Title and summary.
  const float tx = p.x + pic + std::round(fs * 0.7f), th = std::round(fs * 1.5f);
  const float ty = p.y + std::round((pic - th - fs * 1.15f) * 0.5f);
  dl->AddText(f.heading, th, ImVec2(tx, ty), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
  if (!sub.empty())
    dl->AddText(f.body, fs, ImVec2(tx, ty + th + fs * 0.15f),
                sub_col ? sub_col : ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
  // Close button.
  const float cs = std::round(fs * 1.3f);
  ImGui::SetCursorScreenPos(ImVec2(g_right - cs, p.y));
  const bool closed = ImGui::InvisibleButton("##close", ImVec2(cs, cs));
  {
    const ImVec2 c(g_right - cs * 0.5f, p.y + cs * 0.5f);
    const float r = cs * 0.26f;
    const ImU32 col = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f);
    dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 2.0f);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close");
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(g_right - g_left, pic));
  return !closed;
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
  ImGui::Dummy(ImVec2(0, std::round(fs * 0.25f)));
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float y = std::round(p.y) + 0.5f, mid = std::round((g_left + g_right) * 0.5f), d = std::round(fs * 0.28f);
  dl->AddLine(ImVec2(g_left, y), ImVec2(g_right, y), IM_COL32(190, 165, 120, 255), 1.0f);
  // A small lozenge at the middle, like the inscription stops on the top bar's trim.
  dl->AddQuadFilled(ImVec2(mid - d, y), ImVec2(mid, y - d), ImVec2(mid + d, y), ImVec2(mid, y + d),
                    ImGui::GetColorU32(ImGuiCol_Border));
  ImGui::Dummy(ImVec2(0, std::round(fs * 0.45f)));
}

void section(const char* label) {
  divider();
  ImGui::PushFont(ui_fonts().heading, std::round(ImGui::GetFontSize() * 0.82f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void bar(float width, float frac, ImU32 col, float goal) {
  const float fs = ImGui::GetFontSize(), h = std::max(4.0f, std::round(fs * 0.42f));
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float y = std::round(p.y + (ImGui::GetTextLineHeight() - h) * 0.5f);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + width, y + h), kColTrack, h * 0.5f);
  const float fw = width * std::clamp(frac, 0.0f, 1.0f);
  if (fw > 0.5f) dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + std::max(fw, h), y + h), col, h * 0.5f);
  if (goal >= 0) {
    const float gx = std::round(p.x + width * std::clamp(goal, 0.0f, 1.0f));
    dl->AddLine(ImVec2(gx, y - h * 0.6f), ImVec2(gx, y + h * 1.6f), kColTick, 2.0f);
  }
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

void value_row(const char* label, const std::string& value, ImU32 col) {
  ImGui::TextUnformatted(label);
  text_right(value, col);
}

bool stepper(const char* id, int* v, int lo, int hi, const char* unit) {
  const float h = ImGui::GetFrameHeight(), box = std::round(ImGui::GetFontSize() * 3.4f), gap = 4;
  ImGui::SameLine(0, 0);
  ImGui::SetCursorScreenPos(ImVec2(g_right - (h * 2 + box + gap * 2), ImGui::GetCursorScreenPos().y));
  ImGui::PushID(id);
  int nv = *v;
  if (glyph_button("##minus", false, h)) nv = std::max(lo, nv - 1);
  ImGui::SameLine(0, gap);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + box, p.y + h), IM_COL32(246, 236, 210, 255), ImGui::GetStyle().FrameRounding);
  dl->AddRect(p, ImVec2(p.x + box, p.y + h), ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
  const std::string s = std::to_string(*v) + unit;
  const ImVec2 ts = text_size(ui_fonts().bold, 0.0f, s.c_str());
  dl->AddText(ui_fonts().bold, ImGui::GetFontSize(), ImVec2(p.x + (box - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f),
              ImGui::GetColorU32(ImGuiCol_Text), s.c_str());
  ImGui::Dummy(ImVec2(box, h));
  ImGui::SameLine(0, gap);
  if (glyph_button("##plus", true, h)) nv = std::min(hi, nv + 1);
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
  ImGui::SetCursorScreenPos(ImVec2(g_right - each_w * static_cast<float>(n), ImGui::GetCursorScreenPos().y));
  ImGui::PushID(id);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
  bool changed = false;
  for (int k = 0; k < n; ++k) {
    if (k) ImGui::SameLine();
    const bool on = *v == k;
    if (on) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 244, 225, 255));
    }
    ImGui::PushID(k);
    if (ImGui::Button(options[k], ImVec2(each_w, 0)) && !on) {
      *v = k;
      changed = true;
    }
    ImGui::PopID();
    if (on) ImGui::PopStyleColor(2);
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
  const float s = std::round(ImGui::GetFontSize() * 1.1f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float y = p.y + std::round((ImGui::GetTextLineHeight() - s) * 0.5f);
  const ImVec2 c(p.x + s * 0.5f, y + s * 0.5f);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddCircleFilled(c, s * 0.5f, ok ? kColGood : IM_COL32(222, 206, 168, 255));
  dl->AddCircle(c, s * 0.5f, ImGui::GetColorU32(ImGuiCol_Border), 0, 1.5f);
  if (ok) {
    const ImVec2 pts[] = {ImVec2(c.x - s * 0.24f, c.y + s * 0.02f), ImVec2(c.x - s * 0.06f, c.y + s * 0.2f),
                          ImVec2(c.x + s * 0.26f, c.y - s * 0.2f)};
    dl->AddPolyline(pts, 3, IM_COL32(250, 244, 226, 255), 0, std::max(2.0f, s * 0.13f));
  }
  ImGui::Dummy(ImVec2(s, ImGui::GetTextLineHeight()));
}

void good_icon(const UiIcons& icons, const std::string& good_id, float size) {
  const float s = size > 0 ? size : std::round(ImGui::GetFontSize() * 1.2f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  icons.draw_icon("goods/" + good_id + "_16", ImVec2(p.x, std::round(p.y + (ImGui::GetTextLineHeight() - s) * 0.5f)),
                  s);
  ImGui::Dummy(ImVec2(s, std::max(s, ImGui::GetTextLineHeight())));
}

void good_chip(const UiIcons& icons, const std::string& good_id, const std::string& name) {
  const float fs = ImGui::GetFontSize(), is = std::round(fs * 1.25f), pad = std::round(fs * 0.45f);
  const float w = pad * 2 + is + fs * 0.35f + ImGui::CalcTextSize(name.c_str()).x, h = std::round(fs * 1.9f);
  if (ImGui::GetCursorScreenPos().x + w > g_right + 1) ImGui::NewLine();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(246, 236, 210, 255), 4);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImGuiCol_Border), 4, 0, 1.5f);
  icons.draw_icon("goods/" + good_id + "_16", ImVec2(p.x + pad, std::round(p.y + (h - is) * 0.5f)), is);
  dl->AddText(ImVec2(p.x + pad + is + fs * 0.35f, std::round(p.y + (h - fs) * 0.5f)), ImGui::GetColorU32(ImGuiCol_Text),
              name.c_str());
  ImGui::Dummy(ImVec2(w, h));
}

bool link(const char* label) {
  ImGui::PushStyleColor(ImGuiCol_TextLink, IM_COL32(148, 64, 38, 255));
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
