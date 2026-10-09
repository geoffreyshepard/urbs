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
  const TerrainSprites& terrain_sprites() const { return ts_; }
  const FrameStats& stats() const { return stats_; }
  [[nodiscard]] f32 map_px_h() const { return map_ ? static_cast<f32>(map_->h * sim::kTilePx) : 1.0f; }

  // Renders the world and ImGui (draw_data may be null) and presents.
  void render(ImDrawData* draw_data);
  // Writes the next rendered frame to a PNG (without ImGui unless include_ui).
  void request_screenshot(const std::string& path) { screenshot_path_ = path; }

  f32 time = 0;

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
                      SDL_GPUTexture* tex, u32 tex_w, u32 tex_h);
  void save_screenshot(SDL_GPUCommandBuffer* cmd);

  platform::Window* win_ = nullptr;
  SDL_GPUDevice* dev_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_opaque_ = nullptr;
  SDL_GPUGraphicsPipeline* pipe_translucent_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUTexture* scene_ = nullptr;
  SDL_GPUTexture* depth_ = nullptr;
  SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  u32 target_w_ = 0, target_h_ = 0;

  Atlas atlas_;
  TerrainSprites ts_;
  Camera cam_;
  const sim::TileMap* map_ = nullptr;
  std::vector<Chunk> chunks_;
  std::vector<SpriteInstance> scratch_;
  std::vector<SpriteInstance> dynamic_, translucent_;
  DynamicBuffer dyn_buf_, trans_buf_, quad_buf_;

  SDL_GPUTexture* far_tex_ = nullptr;   // one texel per tile
  SDL_GPUTexture* overlay_tex_ = nullptr;
  std::vector<u32> far_pixels_;
  u32 far_revision_ = 0xFFFFFFFFu;
  const u32* overlay_pixels_ = nullptr;
  bool overlay_dirty_ = false;

  std::string screenshot_path_;
  FrameStats stats_;
};

}  // namespace urbs::render
