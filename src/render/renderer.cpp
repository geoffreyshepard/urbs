#include "render/renderer.hpp"

#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstring>

#include "core/log.hpp"
#include "core/profile.hpp"
#include "shaders/sprite_frag.h"
#include "shaders/sprite_vert.h"

namespace urbs::render {

bool Renderer::init(platform::Window& win, const std::string& data_root) {
  win_ = &win;
  dev_ = win.gpu;

  if (!SDL_GPUTextureSupportsFormat(dev_, depth_format_, SDL_GPU_TEXTURETYPE_2D,
                                    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    depth_format_ = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

  SDL_GPUShader* vs = load_shader(dev_, SDL_GPU_SHADERSTAGE_VERTEX, sprite_vert_spv, sprite_vert_spv_size,
                                  sprite_vert_msl, sprite_vert_msl_size, 0, 1);
  SDL_GPUShader* fs = load_shader(dev_, SDL_GPU_SHADERSTAGE_FRAGMENT, sprite_frag_spv, sprite_frag_spv_size,
                                  sprite_frag_msl, sprite_frag_msl_size, 1, 0);
  if (!vs || !fs) return false;

  SDL_GPUVertexBufferDescription vbd{};
  vbd.slot = 0;
  vbd.pitch = sizeof(SpriteInstance);
  vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
  SDL_GPUVertexAttribute attrs[4] = {
      {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
      {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 12},
      {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT4, 20},
      {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 28},
  };
  SDL_GPUColorTargetDescription ctd{};
  ctd.format = win.swapchain_format;
  ctd.blend_state.enable_blend = true;
  ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  ctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  ctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

  SDL_GPUGraphicsPipelineCreateInfo pi{};
  pi.vertex_shader = vs;
  pi.fragment_shader = fs;
  pi.vertex_input_state.vertex_buffer_descriptions = &vbd;
  pi.vertex_input_state.num_vertex_buffers = 1;
  pi.vertex_input_state.vertex_attributes = attrs;
  pi.vertex_input_state.num_vertex_attributes = 4;
  pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pi.depth_stencil_state.enable_depth_test = true;
  pi.depth_stencil_state.enable_depth_write = true;
  pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  pi.target_info.color_target_descriptions = &ctd;
  pi.target_info.num_color_targets = 1;
  pi.target_info.has_depth_stencil_target = true;
  pi.target_info.depth_stencil_format = depth_format_;
  pipe_opaque_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
  pi.depth_stencil_state.enable_depth_write = false;
  pipe_translucent_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
  SDL_ReleaseGPUShader(dev_, vs);
  SDL_ReleaseGPUShader(dev_, fs);
  if (!pipe_opaque_ || !pipe_translucent_) {
    URBS_LOG_ERROR("pipeline creation failed: {}", SDL_GetError());
    return false;
  }

  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = SDL_GPU_FILTER_NEAREST;
  si.mag_filter = SDL_GPU_FILTER_NEAREST;
  si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_ = SDL_CreateGPUSampler(dev_, &si);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xfers;
  bool ok = atlas_.load(dev_, cp, data_root + "assets/atlas/main.png", data_root + "assets/atlas/main.json", xfers);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev_);
  for (auto* x : xfers) SDL_ReleaseGPUTransferBuffer(dev_, x);
  if (!ok) return false;
  ts_.resolve(atlas_);
  cam_.density = SDL_GetWindowPixelDensity(win.window);
  return true;
}

void Renderer::shutdown() {
  if (!dev_) return;
  SDL_WaitForGPUIdle(dev_);
  for (auto& c : chunks_)
    if (c.buf) SDL_ReleaseGPUBuffer(dev_, c.buf);
  chunks_.clear();
  dyn_buf_.release(dev_);
  trans_buf_.release(dev_);
  quad_buf_.release(dev_);
  for (auto* t : {scene_, depth_, far_tex_, overlay_tex_})
    if (t) SDL_ReleaseGPUTexture(dev_, t);
  atlas_.release(dev_);
  if (sampler_) SDL_ReleaseGPUSampler(dev_, sampler_);
  if (pipe_opaque_) SDL_ReleaseGPUGraphicsPipeline(dev_, pipe_opaque_);
  if (pipe_translucent_) SDL_ReleaseGPUGraphicsPipeline(dev_, pipe_translucent_);
  dev_ = nullptr;
}

void Renderer::set_map(const sim::TileMap* map) {
  for (auto& c : chunks_)
    if (c.buf) SDL_ReleaseGPUBuffer(dev_, c.buf);
  chunks_.clear();
  map_ = map;
  if (far_tex_) SDL_ReleaseGPUTexture(dev_, far_tex_);
  if (overlay_tex_) SDL_ReleaseGPUTexture(dev_, overlay_tex_);
  far_tex_ = overlay_tex_ = nullptr;
  far_revision_ = 0xFFFFFFFFu;
  if (!map_) return;
  chunks_.resize(static_cast<size_t>(map_->chunks_x() * map_->chunks_y()));
  const_cast<sim::TileMap*>(map_)->mark_all_dirty();
  std::vector<SDL_GPUTransferBuffer*> unused;
  far_tex_ = create_texture_rgba(dev_, static_cast<u32>(map_->w), static_cast<u32>(map_->h), nullptr, nullptr, unused);
  overlay_tex_ = create_texture_rgba(dev_, static_cast<u32>(map_->w), static_cast<u32>(map_->h), nullptr, nullptr, unused);
}

void Renderer::set_overlay(const u32* tile_rgba) {
  overlay_pixels_ = tile_rgba;
  overlay_dirty_ = tile_rgba != nullptr;
}

void Renderer::push_sprite(const Sprite& s, f32 wx, f32 wy, f32 z, u32 color) {
  dynamic_.push_back(SpriteInstance{wx - static_cast<f32>(s.ax), wy - static_cast<f32>(s.ay), z, static_cast<f32>(s.w),
                                    static_cast<f32>(s.h), s.u, s.v, s.w, s.h, color});
}

void Renderer::push_rect(f32 x, f32 y, f32 w, f32 h, f32 z, u32 color, bool translucent) {
  Sprite white = ts_.base[0][0];  // replaced below if available
  static Sprite cached;
  if (!cached.valid) cached = atlas_.find("fx/white");
  if (cached.valid) white = cached;
  SpriteInstance si{x, y, z, w, h, static_cast<u16>(white.u + 1), static_cast<u16>(white.v + 1), 1, 1, color};
  (translucent ? translucent_ : dynamic_).push_back(si);
}

void Renderer::ensure_targets(u32 w, u32 h) {
  if (scene_ && w == target_w_ && h == target_h_) return;
  if (scene_) SDL_ReleaseGPUTexture(dev_, scene_);
  if (depth_) SDL_ReleaseGPUTexture(dev_, depth_);
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = win_->swapchain_format;
  ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = w;
  ti.height = h;
  ti.layer_count_or_depth = 1;
  ti.num_levels = 1;
  scene_ = SDL_CreateGPUTexture(dev_, &ti);
  ti.format = depth_format_;
  ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_ = SDL_CreateGPUTexture(dev_, &ti);
  target_w_ = w;
  target_h_ = h;
}

void Renderer::rebuild_dirty_chunks(SDL_GPUCopyPass* pass, int budget) {
  URBS_ZONE("rebuild_chunks");
  if (!map_) return;
  sim::TileMap& m = const_cast<sim::TileMap&>(*map_);
  struct Pending {
    size_t chunk;
    u32 offset, bytes;
  };
  std::vector<Pending> pending;
  scratch_.clear();
  // Rebuild visible dirty chunks first, then the rest within budget.
  const Recti vis = cam_.visible_world(64);
  auto visible = [&](int cx, int cy) {
    Recti cr{cx * sim::kChunkTiles * sim::kTilePx, cy * sim::kChunkTiles * sim::kTilePx, sim::kChunkTiles * sim::kTilePx,
             sim::kChunkTiles * sim::kTilePx};
    return cr.intersects(vis);
  };
  for (int pass_i = 0; pass_i < 2 && budget > 0; ++pass_i)
    for (int cy = 0; cy < m.chunks_y() && budget > 0; ++cy)
      for (int cx = 0; cx < m.chunks_x() && budget > 0; ++cx) {
        size_t ci = static_cast<size_t>(cy * m.chunks_x() + cx);
        if (!m.chunk_dirty[ci]) continue;
        if (pass_i == 0 && !visible(cx, cy)) continue;
        size_t start = scratch_.size();
        build_chunk(m, ts_, cx, cy, scratch_);
        m.chunk_dirty[ci] = 0;
        pending.push_back({ci, static_cast<u32>(start * sizeof(SpriteInstance)),
                           static_cast<u32>((scratch_.size() - start) * sizeof(SpriteInstance))});
        --budget;
      }
  if (pending.empty()) return;
  stats_.chunks_rebuilt = static_cast<u32>(pending.size());
  const u32 total = static_cast<u32>(scratch_.size() * sizeof(SpriteInstance));
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xi.size = std::max<u32>(total, 4);
  SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev_, &xi);
  void* map = SDL_MapGPUTransferBuffer(dev_, xb, false);
  std::memcpy(map, scratch_.data(), total);
  SDL_UnmapGPUTransferBuffer(dev_, xb);
  for (const Pending& p : pending) {
    Chunk& c = chunks_[p.chunk];
    c.count = p.bytes / sizeof(SpriteInstance);
    if (p.bytes == 0) continue;
    if (p.bytes > c.capacity) {
      if (c.buf) SDL_ReleaseGPUBuffer(dev_, c.buf);
      SDL_GPUBufferCreateInfo bi{};
      bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
      bi.size = p.bytes + p.bytes / 4;
      c.buf = SDL_CreateGPUBuffer(dev_, &bi);
      c.capacity = bi.size;
    }
    SDL_GPUTransferBufferLocation src{xb, p.offset};
    SDL_GPUBufferRegion dst{c.buf, 0, p.bytes};
    SDL_UploadToGPUBuffer(pass, &src, &dst, false);
  }
  SDL_ReleaseGPUTransferBuffer(dev_, xb);  // deferred until the GPU is done with it
}

void Renderer::upload_far_texture(SDL_GPUCopyPass* pass) {
  if (!map_ || !far_tex_) return;
  auto upload = [&](SDL_GPUTexture* tex, const u32* px) {
    const u32 bytes = static_cast<u32>(map_->w * map_->h * 4);
    SDL_GPUTransferBufferCreateInfo xi{};
    xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xi.size = bytes;
    SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev_, &xi);
    void* m = SDL_MapGPUTransferBuffer(dev_, xb, false);
    std::memcpy(m, px, bytes);
    SDL_UnmapGPUTransferBuffer(dev_, xb);
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = xb;
    SDL_GPUTextureRegion dst{};
    dst.texture = tex;
    dst.w = static_cast<u32>(map_->w);
    dst.h = static_cast<u32>(map_->h);
    dst.d = 1;
    SDL_UploadToGPUTexture(pass, &src, &dst, false);
    SDL_ReleaseGPUTransferBuffer(dev_, xb);
  };
  if (far_revision_ != map_->revision && cam_.far()) {
    build_tile_colors(*map_, ts_, far_pixels_);
    upload(far_tex_, far_pixels_.data());
    far_revision_ = map_->revision;
  }
  if (overlay_dirty_ && overlay_pixels_) {
    upload(overlay_tex_, overlay_pixels_);
    overlay_dirty_ = false;
  }
}

void Renderer::draw_instances(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd, SDL_GPUBuffer* buf, u32 count,
                              SDL_GPUTexture* tex, u32 tex_w, u32 tex_h) {
  if (!buf || count == 0) return;
  Vec2 o = cam_.origin();
  CameraUniform u{{o.x, o.y},
                  {cam_.scale(), cam_.scale()},
                  {static_cast<f32>(target_w_), static_cast<f32>(target_h_)},
                  {1.0f / static_cast<f32>(tex_w), 1.0f / static_cast<f32>(tex_h)}};
  SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));
  SDL_GPUBufferBinding bb{buf, 0};
  SDL_BindGPUVertexBuffers(rp, 0, &bb, 1);
  SDL_GPUTextureSamplerBinding tsb{tex, sampler_};
  SDL_BindGPUFragmentSamplers(rp, 0, &tsb, 1);
  SDL_DrawGPUPrimitives(rp, 6, count, 0, 0);
}

void Renderer::render(ImDrawData* draw_data) {
  URBS_ZONE("render");
  stats_ = {};
  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
  SDL_GPUTexture* swap = nullptr;
  u32 sw = 0, sh = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win_->window, &swap, &sw, &sh) || !swap) {
    SDL_SubmitGPUCommandBuffer(cmd);
    dynamic_.clear();
    translucent_.clear();
    return;
  }
  ensure_targets(sw, sh);
  cam_.screen = {static_cast<f32>(sw), static_cast<f32>(sh)};
  cam_.density = SDL_GetWindowPixelDensity(win_->window);

  // --- Uploads ---
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  bool first = map_ && std::all_of(chunks_.begin(), chunks_.end(), [](const Chunk& c) { return c.buf == nullptr; });
  rebuild_dirty_chunks(cp, first ? 1 << 30 : 24);
  upload_far_texture(cp);
  const u32 n_dyn = static_cast<u32>(dynamic_.size()), n_trans = static_cast<u32>(translucent_.size());
  dyn_buf_.upload(dev_, cp, dynamic_.data(), n_dyn * sizeof(SpriteInstance));
  trans_buf_.upload(dev_, cp, translucent_.data(), n_trans * sizeof(SpriteInstance));
  SpriteInstance quads[2];
  if (map_) {
    f32 mw = static_cast<f32>(map_->w * sim::kTilePx), mh = static_cast<f32>(map_->h * sim::kTilePx);
    quads[0] = {0, 0, kDepthTerrain, mw, mh, 0, 0, static_cast<u16>(map_->w), static_cast<u16>(map_->h), kWhite};
    quads[1] = {0, 0, 0.02f, mw, mh, 0, 0, static_cast<u16>(map_->w), static_cast<u16>(map_->h), rgba(255, 255, 255, 150)};
    quad_buf_.upload(dev_, cp, quads, sizeof(quads));
  }
  SDL_EndGPUCopyPass(cp);
  if (draw_data) ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);

  // --- World pass ---
  SDL_GPUColorTargetInfo ct{};
  ct.texture = scene_;
  ct.clear_color = {0.08f, 0.10f, 0.14f, 1.0f};
  ct.load_op = SDL_GPU_LOADOP_CLEAR;
  ct.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo dt{};
  dt.texture = depth_;
  dt.clear_depth = 1.0f;
  dt.load_op = SDL_GPU_LOADOP_CLEAR;
  dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
  dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  SDL_GPURenderPass* rp = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
  SDL_BindGPUGraphicsPipeline(rp, pipe_opaque_);
  if (map_) {
    if (cam_.far()) {
      draw_instances(rp, cmd, quad_buf_.buf, 1, far_tex_, static_cast<u32>(map_->w), static_cast<u32>(map_->h));
    } else {
      const Recti vis = cam_.visible_world(64);
      const i32 cpx = sim::kChunkTiles * sim::kTilePx;
      int cx0 = std::max(0, vis.x / cpx), cy0 = std::max(0, vis.y / cpx);
      int cx1 = std::min(map_->chunks_x() - 1, (vis.x + vis.w) / cpx);
      int cy1 = std::min(map_->chunks_y() - 1, (vis.y + vis.h) / cpx);
      for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
          const Chunk& c = chunks_[static_cast<size_t>(cy * map_->chunks_x() + cx)];
          if (!c.buf || c.count == 0) continue;
          draw_instances(rp, cmd, c.buf, c.count, atlas_.texture(), atlas_.width(), atlas_.height());
          ++stats_.chunks_drawn;
          stats_.chunk_instances += c.count;
        }
    }
  }
  draw_instances(rp, cmd, dyn_buf_.buf, n_dyn, atlas_.texture(), atlas_.width(), atlas_.height());
  SDL_BindGPUGraphicsPipeline(rp, pipe_translucent_);
  draw_instances(rp, cmd, trans_buf_.buf, n_trans, atlas_.texture(), atlas_.width(), atlas_.height());
  if (map_ && overlay_pixels_) {
    // Second quad instance (offset 32 bytes) is the overlay.
    Vec2 o = cam_.origin();
    CameraUniform u{{o.x, o.y},
                    {cam_.scale(), cam_.scale()},
                    {static_cast<f32>(target_w_), static_cast<f32>(target_h_)},
                    {1.0f / static_cast<f32>(map_->w), 1.0f / static_cast<f32>(map_->h)}};
    SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));
    SDL_GPUBufferBinding bb{quad_buf_.buf, sizeof(SpriteInstance)};
    SDL_BindGPUVertexBuffers(rp, 0, &bb, 1);
    SDL_GPUTextureSamplerBinding tsb{overlay_tex_, sampler_};
    SDL_BindGPUFragmentSamplers(rp, 0, &tsb, 1);
    SDL_DrawGPUPrimitives(rp, 6, 1, 0, 0);
  }
  SDL_EndGPURenderPass(rp);
  stats_.dynamic_instances = n_dyn + n_trans;
  dynamic_.clear();
  translucent_.clear();

  // --- Present: blit scene to swapchain, then UI on top ---
  SDL_GPUBlitInfo bi{};
  bi.source.texture = scene_;
  bi.source.w = sw;
  bi.source.h = sh;
  bi.destination.texture = swap;
  bi.destination.w = sw;
  bi.destination.h = sh;
  bi.load_op = SDL_GPU_LOADOP_DONT_CARE;
  bi.filter = SDL_GPU_FILTER_NEAREST;
  SDL_BlitGPUTexture(cmd, &bi);
  if (draw_data) {
    SDL_GPUColorTargetInfo ut{};
    ut.texture = swap;
    ut.load_op = SDL_GPU_LOADOP_LOAD;
    ut.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* up = SDL_BeginGPURenderPass(cmd, &ut, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, up);
    SDL_EndGPURenderPass(up);
  }
  if (!screenshot_path_.empty()) {
    save_screenshot(cmd);
  } else {
    SDL_SubmitGPUCommandBuffer(cmd);
  }
}

void Renderer::save_screenshot(SDL_GPUCommandBuffer* cmd) {
  const u32 bytes = target_w_ * target_h_ * 4;
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  xi.size = bytes;
  SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev_, &xi);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureRegion src{};
  src.texture = scene_;
  src.w = target_w_;
  src.h = target_h_;
  src.d = 1;
  SDL_GPUTextureTransferInfo dst{};
  dst.transfer_buffer = xb;
  SDL_DownloadFromGPUTexture(cp, &src, &dst);
  SDL_EndGPUCopyPass(cp);
  SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
  SDL_WaitForGPUFences(dev_, true, &fence, 1);
  SDL_ReleaseGPUFence(dev_, fence);
  auto* px = static_cast<u8*>(SDL_MapGPUTransferBuffer(dev_, xb, false));
  std::vector<u8> out(px, px + bytes);
  SDL_UnmapGPUTransferBuffer(dev_, xb);
  SDL_ReleaseGPUTransferBuffer(dev_, xb);
  const SDL_GPUTextureFormat f = win_->swapchain_format;
  if (f == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || f == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
    for (size_t i = 0; i < out.size(); i += 4) std::swap(out[i], out[i + 2]);
  for (size_t i = 3; i < out.size(); i += 4) out[i] = 255;
  stbi_write_png(screenshot_path_.c_str(), static_cast<int>(target_w_), static_cast<int>(target_h_), 4, out.data(),
                 static_cast<int>(target_w_ * 4));
  URBS_LOG_INFO("screenshot: {}", screenshot_path_);
  screenshot_path_.clear();
}

}  // namespace urbs::render
