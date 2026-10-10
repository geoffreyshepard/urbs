#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "platform/platform.hpp"
#include "render/atlas.hpp"
#include "render/camera.hpp"
#include "render/gpu.hpp"
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

  // Per-frame dynamic sprites (buildings, agents, effects). Cleared every frame.
  void push(const SpriteInstance& s) { dynamic_.push_back(s); }
  void push_translucent(const SpriteInstance& s) { translucent_.push_back(s); }
  void push_sprite(const Sprite& s, f32 wx, f32 wy, f32 z, u32 color = kWhite);  // anchored
  void push_rect(f32 x, f32 y, f32 w, f32 h, f32 z, u32 color, bool translucent = true);

  // Per-tile overlay (RGBA8, map-sized). Pass nullptr to hide.
  void set_overlay(const u32* tile_rgba);

  Camera& camera() { return cam_; }
  const Atlas& atlas() const { return atlas_; }
  SDL_GPUDevice* device() const { return dev_; }
  const TerrainSprites& terrain_sprites() const { return ts_; }
  const FrameStats& stats() const { return stats_; }
  [[nodiscard]] f32 map_px_h() const { return map_ ? static_cast<f32>(map_->h * sim::kTilePx) : 1.0f; }

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
  // Cast shadows (see shaders/shadow.vert). shear: x/y offset per px of height, then constant x/y.
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
    u32 capacity = 0;
  };
  void rebuild_dirty_chunks(SDL_GPUCopyPass* pass, int budget);
  void ensure_targets(u32 w, u32 h);
  void upload_far_texture(SDL_GPUCopyPass* pass);
  void draw_instances(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd, SDL_GPUBuffer* buf, u32 count,
                      SDL_GPUTexture* tex, u32 tex_w, u32 tex_h, SDL_GPUTexture* ground = nullptr);
  // main_pass: bind the ground texture and count stats (false for the shadow pass).
  void draw_visible_chunks(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd, bool main_pass);
  void upload_ground(SDL_GPUCopyPass* pass, const std::vector<size_t>& chunks);
  void save_screenshot(SDL_GPUCommandBuffer* cmd);

  platform::Window* win_ = nullptr;
  SDL_GPUDevice* dev_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_opaque_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_translucent_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_shadow_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_ground_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_post_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUSampler* linear_ = nullptr;
  SDL_GPUTexture* scene_ = nullptr;
  SDL_GPUTexture* depth_ = nullptr;
  SDL_GPUTexture* final_ = nullptr;  // graded world (+ UI for screenshots); blitted to the swapchain
  SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  bool has_stencil_ = false;  // stencil keeps overlapping shadows from darkening twice
  u32 target_w_ = 0, target_h_ = 0;

  Atlas atlas_;
  TerrainSprites ts_;
  Camera cam_;
  const sim::TileMap* map_ = nullptr;
  std::vector<Chunk> chunks_;
  std::vector<SpriteInstance> scratch_;
  std::vector<SpriteInstance> dynamic_, translucent_;
  DynamicBuffer dyn_buf_, trans_buf_, quad_buf_;

  SDL_GPUTexture* ground_tex_ = nullptr;  // one texel per tile: wear, luminance, vegetation, shore
  std::vector<u32> ground_scratch_;
  SDL_GPUTexture* far_tex_ = nullptr;   // one texel per tile
  SDL_GPUTexture* overlay_tex_ = nullptr;
  std::vector<u32> far_pixels_;
  u32 far_revision_ = 0xFFFFFFFFu;
  const u32* overlay_pixels_ = nullptr;
  bool overlay_dirty_ = false;

  std::string screenshot_path_;
  bool screenshot_ui_ = true;
  FrameStats stats_;
};

}  // namespace urbs::render
