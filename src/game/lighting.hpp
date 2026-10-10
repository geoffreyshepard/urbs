#pragma once
#include "render/renderer.hpp"
#include "sim/world.hpp"

namespace urbs::game {

// World lighting: colour grade and cast-shadow parameters from the sim clock, season and weather.
// Pure presentation; reads the world, never changes it.
struct Lighting {
  // Smoothed state so weather changes fade instead of snapping.
  render::Renderer::Grade grade;
  render::Renderer::Shadows shadows;
  bool primed = false;

  // dt: real seconds since the last frame (0 snaps to the target).
  void update(const sim::World& w, sim::Weather weather, f32 alpha, f32 dt);
};

}  // namespace urbs::game
