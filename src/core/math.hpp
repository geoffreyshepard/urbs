#pragma once
#include <algorithm>
#include <cmath>

#include "core/types.hpp"

namespace urbs {

struct Vec2i {
  i32 x = 0, y = 0;
  friend constexpr Vec2i operator+(Vec2i a, Vec2i b) { return {a.x + b.x, a.y + b.y}; }
  friend constexpr Vec2i operator-(Vec2i a, Vec2i b) { return {a.x - b.x, a.y - b.y}; }
  friend constexpr bool operator==(Vec2i a, Vec2i b) { return a.x == b.x && a.y == b.y; }
  friend constexpr bool operator!=(Vec2i a, Vec2i b) { return !(a == b); }
};

struct Vec2 {
  f32 x = 0, y = 0;
  friend constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
  friend constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
  friend constexpr Vec2 operator*(Vec2 a, f32 s) { return {a.x * s, a.y * s}; }
};

struct Recti {
  i32 x = 0, y = 0, w = 0, h = 0;
  [[nodiscard]] constexpr bool contains(Vec2i p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
  [[nodiscard]] constexpr bool intersects(const Recti& o) const {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
  }
};

inline constexpr i32 kDir8X[8] = {0, 1, 1, 1, 0, -1, -1, -1};  // N, NE, E, SE, S, SW, W, NW
inline constexpr i32 kDir8Y[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
inline constexpr i32 kDir4X[4] = {0, 1, 0, -1};  // N, E, S, W
inline constexpr i32 kDir4Y[4] = {-1, 0, 1, 0};

constexpr i32 manhattan(Vec2i a, Vec2i b) {
  return (a.x > b.x ? a.x - b.x : b.x - a.x) + (a.y > b.y ? a.y - b.y : b.y - a.y);
}
// Octile distance in tenths of a tile (orthogonal = 10, diagonal = 14).
constexpr i32 octile10(Vec2i a, Vec2i b) {
  i32 dx = a.x > b.x ? a.x - b.x : b.x - a.x;
  i32 dy = a.y > b.y ? a.y - b.y : b.y - a.y;
  i32 mn = dx < dy ? dx : dy, mx = dx < dy ? dy : dx;
  return 14 * mn + 10 * (mx - mn);
}

template <typename T>
constexpr T clamp(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace urbs
