#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "platform/platform.hpp"
#include "render/atlas.hpp"
#include "render/camera.hpp"
#include "render/gpu.hpp"
#include "render/iso.hpp"
#include "render/terrain.hpp"
#include "sim/map.hpp"

struct ImDrawData;

namespace urbs::render {

struct FrameStats {
  u32 chunks_drawn = 0;
  u32 chunk_instances = 0;
  u32 dynamic_instances = 0;
  u32 chunks_rebuilt = 0;
};

class Renderer {
 public:
  bool init(platform::Window& win, const std::string& data_root);
  void shutdown();

  // Static world layer. The renderer rebuilds dirty chunks of this map each frame.
  void set_map(const sim::TileMap* map);

  // Per-frame dynamic sprites (buildings, agents, effects). Cleared every frame. All positions are
  // iso px (render/iso.hpp).
  void push(const SpriteInstance& s) { dynamic_.push_back(s); }
  void push_translucent(const SpriteInstance& s) { translucent_.push_back(s); }
  // Emissive sprites (window lights): added over the graded frame, depth-tested against the world.
  void push_light(const SpriteInstance& s) { lights_.push_back(s); }
  // The sprite's anchor lands on the iso point (ix, iy); size follows the sprite's packing scale.
  void push_sprite(const Sprite& s, f32 ix, f32 iy, f32 z, u32 color = kWhite) { push(instance(s, ix, iy, z, color)); }
  void push_sprite_translucent(const Sprite& s, f32 ix, f32 iy, f32 z, u32 color = kWhite) {
    push_translucent(instance(s, ix, iy, z, color));
  }
  void push_light_sprite(const Sprite& s, f32 ix, f32 iy, f32 z, u32 color = kWhite) {
    push_light(instance(s, ix, iy, z, color));
  }
  // The instance push_sprite would add (for callers that crop or batch).
  [[nodiscard]] static SpriteInstance instance(const Sprite& s, f32 ix, f32 iy, f32 z, u32 color = kWhite) {
    return SpriteInstance{ix - s.iso_ax(), iy - s.iso_ay(), z, s.iso_w(), s.iso_h(), s.u, s.v, s.w, s.h, color};
  }
  // Axis-aligned solid rect in iso px.
  void push_rect(f32 x, f32 y, f32 w, f32 h, f32 z, u32 color, bool translucent = true);
  // Solid tile diamonds covering the footprint fw x fh at (x0, y0), lifted by `level` (pass
  // iso::level_at for the ground under it).  Translucent by default (selection, ghosts, overlays).
  void push_diamond(i32 x0, i32 y0, i32 fw, i32 fh, f32 level, f32 z, u32 color, bool translucent = true);

  // Per-tile overlay (RGBA8, map-sized). Pass nullptr to hide.
  void set_overlay(const u32* tile_rgba);

  Camera& camera() { return cam_; }
  const Atlas& atlas() const { return atlas_; }
  SDL_GPUDevice* device() const { return dev_; }
  const TerrainSprites& terrain_sprites() const { return ts_; }
  const FrameStats& stats() const { return stats_; }
  // Height of the map's iso bounding box (for iso::depth_for_iso).  map_px_h is the old name.
  [[nodiscard]] f32 map_iso_h() const { return map_ ? iso::map_iso_h(*map_) : 1.0f; }
  [[nodiscard]] f32 map_px_h() const { return map_iso_h(); }

  // Renders the world and ImGui (draw_data may be null) and presents.
  void render(ImDrawData* draw_data);
  // Writes the next rendered frame to a PNG (without ImGui unless include_ui).
  void request_screenshot(const std::string& path, bool include_ui = true) {
    screenshot_path_ = path;
    screenshot_ui_ = include_ui;
  }

  f32 time = 0;
  // Colour grade applied to the world layer after it is drawn (layout matches shaders/post.frag).
  struct Grade {
    f32 shadow_tint[4] = {1, 1, 1, 1};  // rgb; a = exposure
    f32 light_tint[4] = {1, 1, 1, 1};   // rgb; a = saturation
    f32 lift[4] = {0, 0, 0, 0};         // rgb; a = vignette
    f32 params[4] = {1, 0, 0, 0};       // contrast, time, night, unused
  } grade;
  // Unused since the isometric switch: buildings and props carry baked shadows from Blender. Kept so
  // game/lighting.cpp can still set it.
  struct Shadows {
    bool enabled = true;
    f32 shear[4] = {0.22f, 0.08f, 2.0f, 1.0f};
    f32 color[4] = {0.62f, 0.64f, 0.78f, 1.0f};  // multiplies the ground under a shadow
  } shadows;
  // Ground shading of the static layer (layout matches shaders/ground.frag). season: overall
  // dryness, dry-patch strength, winter browning, snow cover.
  f32 ground_season[4] = {0, 0.3f, 0, 0};

 private:
  struct Chunk {
    SDL_GPUBuffer* buf = nullptr;
    u32 count = 0;
    u32 ground = 0;  // leading ground instances; the rest are decorations (see build_chunk)
    u32 capacity = 0;
  };
  void rebuild_dirty_chunks(SDL_GPUCopyPass* pass, int budget);
  void ensure_targets(u32 w, u32 h);
  void upload_far_texture(SDL_GPUCopyPass* pass);
  void draw_instances(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd, SDL_GPUBuffer* buf, u32 count,
                      SDL_GPUTexture* ground = nullptr, u32 first = 0);
  void draw_visible_chunks(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd);
  void draw_far(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd);
  void push_overlay_tiles();
  [[nodiscard]] bool chunk_visible(int cx, int cy, const iso::Rectf& view) const;
  [[nodiscard]] iso::Rectf view_rect(f32 margin) const;
  void upload_ground(SDL_GPUCopyPass* pass, const std::vector<size_t>& chunks);
  void save_screenshot(SDL_GPUCommandBuffer* cmd);

  platform::Window* win_ = nullptr;
  SDL_GPUDevice* dev_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_opaque_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_translucent_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_ground_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_post_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_light_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_quad_ = nullptr;  // far zoom: the per-tile colour texture as one diamond
  SDL_GPUSampler* sampler_ = nullptr;   // atlas: linear, mipmapped
  SDL_GPUSampler* nearest_ = nullptr;   // per-tile textures drawn as pixels (far zoom), post
  SDL_GPUSampler* linear_ = nullptr;
  SDL_GPUTexture* scene_ = nullptr;
  SDL_GPUTexture* depth_ = nullptr;
  SDL_GPUTexture* final_ = nullptr;  // graded world (+ UI for screenshots); blitted to the swapchain
  SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  bool has_stencil_ = false;
  u32 target_w_ = 0, target_h_ = 0;

  Atlas atlas_;
  TerrainSprites ts_;
  Camera cam_;
  const sim::TileMap* map_ = nullptr;
  std::vector<Chunk> chunks_;
  std::vector<SpriteInstance> scratch_;
  std::vector<SpriteInstance> dynamic_, translucent_, lights_;
  DynamicBuffer dyn_buf_, trans_buf_, light_buf_;
  Sprite tile_, white_;  // fx/tile (white 128 x 64 diamond), fx/white

  SDL_GPUTexture* ground_tex_ = nullptr;  // one texel per tile: wear, luminance, vegetation, shore
  std::vector<u32> ground_scratch_;
  SDL_GPUTexture* far_tex_ = nullptr;   // one texel per tile
  std::vector<u32> far_pixels_;
  u32 far_revision_ = 0xFFFFFFFFu;
  const u32* overlay_pixels_ = nullptr;
  bool overlay_dirty_ = false;

  std::string screenshot_path_;
  bool screenshot_ui_ = true;
  FrameStats stats_;
};

}  // namespace urbs::render
