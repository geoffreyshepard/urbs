#include "render/renderer.hpp"

#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstring>

#include "core/log.hpp"
#include "core/profile.hpp"
#include "shaders/ground_frag.h"
#include "shaders/ground_vert.h"
#include "shaders/isoquad_frag.h"
#include "shaders/isoquad_vert.h"
#include "shaders/post_frag.h"
#include "shaders/post_vert.h"
#include "shaders/sprite_frag.h"
#include "shaders/sprite_vert.h"

namespace urbs::render {

bool Renderer::init(platform::Window& win, const std::string& data_root) {
  win_ = &win;
  dev_ = win.gpu;

  // Prefer a depth format with stencil; fall back to plain depth.
  auto supports = [&](SDL_GPUTextureFormat f) {
    return SDL_GPUTextureSupportsFormat(dev_, f, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
  };
  if (supports(SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT)) {
    depth_format_ = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    has_stencil_ = true;
  } else if (supports(SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT)) {
    depth_format_ = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    has_stencil_ = true;
  } else if (!supports(depth_format_)) {
    depth_format_ = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
  }

  SDL_GPUShader* vs = load_shader(dev_, SDL_GPU_SHADERSTAGE_VERTEX, sprite_vert_spv, sprite_vert_spv_size,
                                  sprite_vert_msl, sprite_vert_msl_size, 0, 1);
  SDL_GPUShader* fs = load_shader(dev_, SDL_GPU_SHADERSTAGE_FRAGMENT, sprite_frag_spv, sprite_frag_spv_size,
                                  sprite_frag_msl, sprite_frag_msl_size, 1, 0);
  SDL_GPUShader* qvs = load_shader(dev_, SDL_GPU_SHADERSTAGE_VERTEX, isoquad_vert_spv, isoquad_vert_spv_size,
                                   isoquad_vert_msl, isoquad_vert_msl_size, 0, 1);
  SDL_GPUShader* qfs = load_shader(dev_, SDL_GPU_SHADERSTAGE_FRAGMENT, isoquad_frag_spv, isoquad_frag_spv_size,
                                   isoquad_frag_msl, isoquad_frag_msl_size, 1, 0);
  SDL_GPUShader* pvs = load_shader(dev_, SDL_GPU_SHADERSTAGE_VERTEX, post_vert_spv, post_vert_spv_size, post_vert_msl,
                                   post_vert_msl_size, 0, 0);
  SDL_GPUShader* pfs = load_shader(dev_, SDL_GPU_SHADERSTAGE_FRAGMENT, post_frag_spv, post_frag_spv_size,
                                   post_frag_msl, post_frag_msl_size, 1, 1);
  SDL_GPUShader* gvs = load_shader(dev_, SDL_GPU_SHADERSTAGE_VERTEX, ground_vert_spv, ground_vert_spv_size,
                                   ground_vert_msl, ground_vert_msl_size, 0, 1);
  SDL_GPUShader* gfs = load_shader(dev_, SDL_GPU_SHADERSTAGE_FRAGMENT, ground_frag_spv, ground_frag_spv_size,
                                   ground_frag_msl, ground_frag_msl_size, 2, 1);
  if (!vs || !fs || !qvs || !qfs || !pvs || !pfs || !gvs || !gfs) return false;

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
  {
    SDL_GPUGraphicsPipelineCreateInfo gpi = pi;  // static chunk layer with ground shading
    gpi.vertex_shader = gvs;
    gpi.fragment_shader = gfs;
    pipe_ground_ = SDL_CreateGPUGraphicsPipeline(dev_, &gpi);
  }
  pi.depth_stencil_state.enable_depth_write = false;
  pipe_translucent_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);

  // Far zoom: the per-tile colour texture as one iso diamond (no vertex buffer).
  {
    SDL_GPUGraphicsPipelineCreateInfo qpi = pi;
    qpi.vertex_shader = qvs;
    qpi.fragment_shader = qfs;
    qpi.vertex_input_state = {};
    qpi.depth_stencil_state.enable_depth_write = true;
    pipe_quad_ = SDL_CreateGPUGraphicsPipeline(dev_, &qpi);
  }

  // Lights: additive, depth-tested (no writes) against the world pass, drawn on the graded frame.
  SDL_GPUGraphicsPipelineCreateInfo lpi = pi;
  SDL_GPUColorTargetDescription lctd = ctd;
  lctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  lctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  lctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
  lctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  lpi.target_info.color_target_descriptions = &lctd;
  lpi.depth_stencil_state.enable_depth_write = false;
  pipe_light_ = SDL_CreateGPUGraphicsPipeline(dev_, &lpi);

  // Post: fullscreen triangle, scene texture -> graded final texture.
  SDL_GPUGraphicsPipelineCreateInfo ppi{};
  SDL_GPUColorTargetDescription pctd{};
  pctd.format = win.swapchain_format;
  ppi.vertex_shader = pvs;
  ppi.fragment_shader = pfs;
  ppi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  ppi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  ppi.target_info.color_target_descriptions = &pctd;
  ppi.target_info.num_color_targets = 1;
  pipe_post_ = SDL_CreateGPUGraphicsPipeline(dev_, &ppi);

  for (SDL_GPUShader* sh : {vs, fs, qvs, qfs, pvs, pfs, gvs, gfs}) SDL_ReleaseGPUShader(dev_, sh);
  if (!pipe_opaque_ || !pipe_translucent_ || !pipe_quad_ || !pipe_post_ || !pipe_ground_ || !pipe_light_) {
    URBS_LOG_ERROR("pipeline creation failed: {}", SDL_GetError());
    return false;
  }

  SDL_GPUSamplerCreateInfo si{};
  si.min_filter = SDL_GPU_FILTER_NEAREST;
  si.mag_filter = SDL_GPU_FILTER_NEAREST;
  si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  nearest_ = SDL_CreateGPUSampler(dev_, &si);
  si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;  // ground texture: smooth between tile centres
  linear_ = SDL_CreateGPUSampler(dev_, &si);
  si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;  // atlas: pre-rendered art, filtered and mipmapped
  si.max_lod = 1000.0f;
  sampler_ = SDL_CreateGPUSampler(dev_, &si);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  std::vector<SDL_GPUTransferBuffer*> xfers;
  bool ok = atlas_.load(dev_, cp, data_root + "assets/atlas/", xfers);
  SDL_EndGPUCopyPass(cp);
  if (ok) atlas_.generate_mips(cmd);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_WaitForGPUIdle(dev_);
  for (auto* x : xfers) SDL_ReleaseGPUTransferBuffer(dev_, x);
  if (!ok) return false;
  ts_.resolve(atlas_);
  tile_ = atlas_.get("fx/tile");
  white_ = atlas_.get("fx/white");
  cam_.density = SDL_GetWindowPixelDensity(win.window);
  return true;
}

std::vector<std::string> Renderer::set_mod_sprites(const std::vector<std::string>& mod_dirs,
                                                   const ModSpriteConfig& cfg) {
  ModSpriteConfig c = cfg;
  c.page_w = atlas_.width();
  c.page_h = atlas_.height();
  c.first_page = atlas_.base_pages();
  c.world_scale = atlas_.scale();
  const ModSpritePack pack = pack_mod_sprites(mod_dirs, c);
  std::vector<SDL_GPUTransferBuffer*> xfers;
  if (pack.pages == 0 && atlas_.pages() == atlas_.base_pages()) {
    atlas_.set_mod_sprites(dev_, nullptr, pack, xfers);  // names only: the texture stays as it is
  } else {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
    atlas_.set_mod_sprites(dev_, cp, pack, xfers);
    SDL_EndGPUCopyPass(cp);
    atlas_.generate_mips(cmd);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(dev_);
    for (auto* x : xfers) SDL_ReleaseGPUTransferBuffer(dev_, x);
  }
  // Terrain, roads and props may be modded too: resolve them again and rebuild the static chunks.
  ts_.resolve(atlas_);
  tile_ = atlas_.get("fx/tile");
  white_ = atlas_.get("fx/white");
  if (map_) set_map(map_);
  return pack.warnings;
}

void Renderer::shutdown() {
  if (!dev_) return;
  SDL_WaitForGPUIdle(dev_);
  for (auto& c : chunks_)
    if (c.buf) SDL_ReleaseGPUBuffer(dev_, c.buf);
  chunks_.clear();
  dyn_buf_.release(dev_);
  trans_buf_.release(dev_);
  light_buf_.release(dev_);
  for (auto* t : {scene_, depth_, final_, far_tex_, ground_tex_})
    if (t) SDL_ReleaseGPUTexture(dev_, t);
  atlas_.release(dev_);
  for (auto* s : {sampler_, nearest_, linear_})
    if (s) SDL_ReleaseGPUSampler(dev_, s);
  for (auto* p : {pipe_opaque_, pipe_translucent_, pipe_quad_, pipe_post_, pipe_ground_, pipe_light_})
    if (p) SDL_ReleaseGPUGraphicsPipeline(dev_, p);
  dev_ = nullptr;
}

void Renderer::set_map(const sim::TileMap* map) {
  for (auto& c : chunks_)
    if (c.buf) SDL_ReleaseGPUBuffer(dev_, c.buf);
  chunks_.clear();
  map_ = map;
  for (auto* t : {far_tex_, ground_tex_})
    if (t) SDL_ReleaseGPUTexture(dev_, t);
  far_tex_ = ground_tex_ = nullptr;
  far_revision_ = 0xFFFFFFFFu;
  if (!map_) return;
  chunks_.resize(static_cast<size_t>(map_->chunks_x() * map_->chunks_y()));
  const_cast<sim::TileMap*>(map_)->mark_all_dirty();
  std::vector<SDL_GPUTransferBuffer*> unused;
  far_tex_ = create_texture_rgba(dev_, static_cast<u32>(map_->w), static_cast<u32>(map_->h), nullptr, nullptr, unused);
  ground_tex_ = create_texture_rgba(dev_, static_cast<u32>(map_->w), static_cast<u32>(map_->h), nullptr, nullptr, unused);
}

void Renderer::set_overlay(const u32* tile_rgba) {
  overlay_pixels_ = tile_rgba;
  overlay_dirty_ = tile_rgba != nullptr;
}

void Renderer::push_rect(f32 x, f32 y, f32 w, f32 h, f32 z, u32 color, bool translucent) {
  // Sample the middle of fx/white so filtering and mips never reach its transparent border.
  const u16 cu = static_cast<u16>(white_.u + white_.w / 2), cv = static_cast<u16>(white_.v + white_.h / 2);
  SpriteInstance si{x, y, z, w, h, cu, cv, 1, 1, color};
  (translucent ? translucent_ : dynamic_).push_back(si);
}

void Renderer::push_diamond(i32 x0, i32 y0, i32 fw, i32 fh, f32 level, f32 z, u32 color, bool translucent) {
  auto& out = translucent ? translucent_ : dynamic_;
  for (i32 y = y0; y < y0 + fh; ++y)
    for (i32 x = x0; x < x0 + fw; ++x) {
      const Vec2 a = iso::tile_anchor(x, y, level);
      out.push_back(instance(tile_, a.x, a.y, z, color));
    }
}

void Renderer::push_overlay_tiles() {
  if (!map_ || !overlay_pixels_) return;
  const iso::TileRange r = iso::tiles_in_rect(*map_, view_rect(0), 1);
  for (i32 y = r.y0; y <= r.y1; ++y)
    for (i32 x = r.x0; x <= r.x1; ++x) {
      u32 c = overlay_pixels_[static_cast<size_t>(y) * static_cast<size_t>(map_->w) + static_cast<size_t>(x)];
      const u32 a = (c >> 24) * 230 / 255;
      if (a == 0) continue;
      c = (c & 0x00FFFFFFu) | (a << 24);
      const Vec2 p = iso::tile_anchor(x, y, iso::level_at(*map_, x, y));
      translucent_.push_back(instance(tile_, p.x, p.y, 0.02f, c));
    }
}

iso::Rectf Renderer::view_rect(f32 margin) const {
  const Vec2 o = cam_.origin();
  const f32 s = cam_.scale();
  return {o.x - margin, o.y - margin, cam_.screen.x / s + 2 * margin, cam_.screen.y / s + 2 * margin};
}

bool Renderer::chunk_visible(int cx, int cy, const iso::Rectf& v) const {
  // Iso bounding box of the chunk's diamond, raised for terrain levels and tall decorations.
  constexpr f32 kAbove = sim::kMaxHeight * iso::kLevelPx + 256.0f;
  const i32 x0 = cx * sim::kChunkTiles, y0 = cy * sim::kChunkTiles;
  const i32 x1 = std::min(map_->w, x0 + sim::kChunkTiles), y1 = std::min(map_->h, y0 + sim::kChunkTiles);
  const f32 left = iso::from_tile(static_cast<f32>(x0), static_cast<f32>(y1)).x;
  const f32 right = iso::from_tile(static_cast<f32>(x1), static_cast<f32>(y0)).x;
  const f32 top = iso::from_tile(static_cast<f32>(x0), static_cast<f32>(y0)).y - kAbove;
  const f32 bottom = iso::from_tile(static_cast<f32>(x1), static_cast<f32>(y1)).y + iso::kTileH;
  return left < v.x + v.w && v.x < right && top < v.y + v.h && v.y < bottom;
}

void Renderer::ensure_targets(u32 w, u32 h) {
  if (scene_ && w == target_w_ && h == target_h_) return;
  for (auto* t : {scene_, depth_, final_})
    if (t) SDL_ReleaseGPUTexture(dev_, t);
  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = win_->swapchain_format;
  ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  ti.width = w;
  ti.height = h;
  ti.layer_count_or_depth = 1;
  ti.num_levels = 1;
  scene_ = SDL_CreateGPUTexture(dev_, &ti);
  final_ = SDL_CreateGPUTexture(dev_, &ti);
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
    u32 offset, bytes, ground;
  };
  std::vector<Pending> pending;
  scratch_.clear();
  // Rebuild visible dirty chunks first, then the rest within budget.
  const iso::Rectf vis = view_rect(256.0f);
  auto visible = [&](int cx, int cy) { return chunk_visible(cx, cy, vis); };
  for (int pass_i = 0; pass_i < 2 && budget > 0; ++pass_i)
    for (int cy = 0; cy < m.chunks_y() && budget > 0; ++cy)
      for (int cx = 0; cx < m.chunks_x() && budget > 0; ++cx) {
        size_t ci = static_cast<size_t>(cy * m.chunks_x() + cx);
        if (!m.chunk_dirty[ci]) continue;
        if (pass_i == 0 && !visible(cx, cy)) continue;
        size_t start = scratch_.size();
        const u32 ground = build_chunk(m, ts_, cx, cy, scratch_);
        m.chunk_dirty[ci] = 0;
        pending.push_back({ci, static_cast<u32>(start * sizeof(SpriteInstance)),
                           static_cast<u32>((scratch_.size() - start) * sizeof(SpriteInstance)), ground});
        --budget;
      }
  if (pending.empty()) return;
  stats_.chunks_rebuilt = static_cast<u32>(pending.size());
  {
    std::vector<size_t> rebuilt;
    rebuilt.reserve(pending.size());
    for (const Pending& p : pending) rebuilt.push_back(p.chunk);
    upload_ground(pass, rebuilt);
  }
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
    c.ground = p.ground;
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

void Renderer::upload_ground(SDL_GPUCopyPass* pass, const std::vector<size_t>& chunks) {
  if (!ground_tex_) return;
  // Wear spreads two tiles and a texel averages the four tiles at a corner, so each rebuilt chunk
  // refreshes its texels plus a 3-tile border.
  constexpr i32 kPad = 3;
  struct Region {
    i32 x0, y0, w, h;
    u32 offset;
  };
  std::vector<Region> regions;
  ground_scratch_.clear();
  std::vector<u32> block;
  for (size_t ci : chunks) {
    const i32 cx = static_cast<i32>(ci) % map_->chunks_x(), cy = static_cast<i32>(ci) / map_->chunks_x();
    const i32 x0 = std::max(0, cx * sim::kChunkTiles - kPad), y0 = std::max(0, cy * sim::kChunkTiles - kPad);
    const i32 x1 = std::min(map_->w, (cx + 1) * sim::kChunkTiles + kPad);
    const i32 y1 = std::min(map_->h, (cy + 1) * sim::kChunkTiles + kPad);
    build_ground_texels(*map_, x0, y0, x1, y1, block);
    regions.push_back({x0, y0, x1 - x0, y1 - y0, static_cast<u32>(ground_scratch_.size() * 4)});
    ground_scratch_.insert(ground_scratch_.end(), block.begin(), block.end());
  }
  SDL_GPUTransferBufferCreateInfo xi{};
  xi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  xi.size = static_cast<u32>(ground_scratch_.size() * 4);
  SDL_GPUTransferBuffer* xb = SDL_CreateGPUTransferBuffer(dev_, &xi);
  std::memcpy(SDL_MapGPUTransferBuffer(dev_, xb, false), ground_scratch_.data(), xi.size);
  SDL_UnmapGPUTransferBuffer(dev_, xb);
  for (const Region& r : regions) {
    SDL_GPUTextureTransferInfo src{};
    src.transfer_buffer = xb;
    src.offset = r.offset;
    src.pixels_per_row = static_cast<u32>(r.w);
    src.rows_per_layer = static_cast<u32>(r.h);
    SDL_GPUTextureRegion dst{};
    dst.texture = ground_tex_;
    dst.x = static_cast<u32>(r.x0);
    dst.y = static_cast<u32>(r.y0);
    dst.w = static_cast<u32>(r.w);
    dst.h = static_cast<u32>(r.h);
    dst.d = 1;
    SDL_UploadToGPUTexture(pass, &src, &dst, false);
  }
  SDL_ReleaseGPUTransferBuffer(dev_, xb);
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
}

void Renderer::draw_instances(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd, SDL_GPUBuffer* buf, u32 count,
                              SDL_GPUTexture* ground, u32 first) {
  if (!buf || count == 0) return;
  Vec2 o = cam_.origin();
  CameraUniform u{{o.x, o.y},
                  {cam_.scale(), cam_.scale()},
                  {static_cast<f32>(target_w_), static_cast<f32>(target_h_)},
                  {1.0f / static_cast<f32>(atlas_.width()), 1.0f / static_cast<f32>(atlas_.height())},
                  {static_cast<f32>(atlas_.height()), 0, 0, 0}};
  SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));
  SDL_GPUBufferBinding bb{buf, 0};
  SDL_BindGPUVertexBuffers(rp, 0, &bb, 1);
  SDL_GPUTextureSamplerBinding tsb[2] = {{atlas_.texture(), sampler_}, {ground, linear_}};
  SDL_BindGPUFragmentSamplers(rp, 0, tsb, ground ? 2 : 1);
  SDL_DrawGPUPrimitives(rp, 6, count, 0, first);
}

void Renderer::draw_visible_chunks(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd) {
  // Every chunk's ground first, then the decorations by chunk diagonal (back to front), so trees and
  // their shadows that hang over a neighbouring chunk always land on finished ground.
  const iso::Rectf vis = view_rect(0.0f);
  const int ncx = map_->chunks_x(), ncy = map_->chunks_y();
  for (int cy = 0; cy < ncy; ++cy)
    for (int cx = 0; cx < ncx; ++cx) {
      const Chunk& c = chunks_[static_cast<size_t>(cy * ncx + cx)];
      if (!c.buf || c.count == 0 || !chunk_visible(cx, cy, vis)) continue;
      draw_instances(rp, cmd, c.buf, c.ground, ground_tex_);
      ++stats_.chunks_drawn;
      stats_.chunk_instances += c.count;
    }
  for (int d = 0; d <= ncx + ncy - 2; ++d)
    for (int cx = std::max(0, d - ncy + 1); cx <= std::min(d, ncx - 1); ++cx) {
      const int cy = d - cx;
      const Chunk& c = chunks_[static_cast<size_t>(cy * ncx + cx)];
      if (!c.buf || c.count <= c.ground || !chunk_visible(cx, cy, vis)) continue;
      draw_instances(rp, cmd, c.buf, c.count - c.ground, ground_tex_, c.ground);
    }
}

void Renderer::draw_far(SDL_GPURenderPass* rp, SDL_GPUCommandBuffer* cmd) {
  const Vec2 o = cam_.origin();
  const f32 q[12] = {o.x,
                     o.y,
                     cam_.scale(),
                     cam_.scale(),
                     static_cast<f32>(target_w_),
                     static_cast<f32>(target_h_),
                     static_cast<f32>(map_->w),
                     static_cast<f32>(map_->h),
                     iso::kTileW * 0.5f,
                     iso::kTileH * 0.5f,
                     kDepthTerrain,
                     0.0f};
  SDL_BindGPUGraphicsPipeline(rp, pipe_quad_);
  SDL_PushGPUVertexUniformData(cmd, 0, q, sizeof(q));
  SDL_GPUTextureSamplerBinding tsb{far_tex_, nearest_};
  SDL_BindGPUFragmentSamplers(rp, 0, &tsb, 1);
  SDL_DrawGPUPrimitives(rp, 6, 1, 0, 0);
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
    lights_.clear();
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
  push_overlay_tiles();
  const u32 n_dyn = static_cast<u32>(dynamic_.size()), n_trans = static_cast<u32>(translucent_.size());
  dyn_buf_.upload(dev_, cp, dynamic_.data(), n_dyn * sizeof(SpriteInstance));
  trans_buf_.upload(dev_, cp, translucent_.data(), n_trans * sizeof(SpriteInstance));
  const u32 n_light = static_cast<u32>(lights_.size());
  light_buf_.upload(dev_, cp, lights_.data(), n_light * sizeof(SpriteInstance));
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
  dt.store_op = SDL_GPU_STOREOP_STORE;  // the light pass tests against it
  dt.stencil_load_op = has_stencil_ ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
  dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  dt.clear_stencil = 0;
  SDL_GPURenderPass* rp = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
  if (map_) {
    if (cam_.far()) {
      draw_far(rp, cmd);
    } else {
      SDL_BindGPUGraphicsPipeline(rp, pipe_ground_);
      const f32 gu[8] = {ground_season[0], ground_season[1], ground_season[2], ground_season[3],
                         1.0f / static_cast<f32>(map_->w * sim::kTilePx), 1.0f / static_cast<f32>(map_->h * sim::kTilePx),
                         time, (kDepthTerrain + kDepthRoad) * 0.5f};
      SDL_PushGPUFragmentUniformData(cmd, 0, gu, sizeof(gu));
      draw_visible_chunks(rp, cmd);
    }
  }
  SDL_BindGPUGraphicsPipeline(rp, pipe_opaque_);
  draw_instances(rp, cmd, dyn_buf_.buf, n_dyn);
  SDL_BindGPUGraphicsPipeline(rp, pipe_translucent_);
  draw_instances(rp, cmd, trans_buf_.buf, n_trans);
  SDL_EndGPURenderPass(rp);
  stats_.dynamic_instances = n_dyn + n_trans;
  dynamic_.clear();
  translucent_.clear();

  // --- Grade: scene -> final ---
  {
    SDL_GPUColorTargetInfo gt{};
    gt.texture = final_;
    gt.load_op = SDL_GPU_LOADOP_DONT_CARE;
    gt.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* gp = SDL_BeginGPURenderPass(cmd, &gt, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(gp, pipe_post_);
    grade.params[1] = time;
    SDL_PushGPUFragmentUniformData(cmd, 0, &grade, sizeof(grade));
    SDL_GPUTextureSamplerBinding tsb{scene_, nearest_};
    SDL_BindGPUFragmentSamplers(gp, 0, &tsb, 1);
    SDL_DrawGPUPrimitives(gp, 3, 1, 0, 0);
    SDL_EndGPURenderPass(gp);
  }
  if (n_light > 0 && !cam_.far()) {
    SDL_GPUColorTargetInfo lt{};
    lt.texture = final_;
    lt.load_op = SDL_GPU_LOADOP_LOAD;
    lt.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo ld{};
    ld.texture = depth_;
    ld.load_op = SDL_GPU_LOADOP_LOAD;
    ld.store_op = SDL_GPU_STOREOP_DONT_CARE;
    ld.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    ld.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* lp = SDL_BeginGPURenderPass(cmd, &lt, 1, &ld);
    SDL_BindGPUGraphicsPipeline(lp, pipe_light_);
    draw_instances(lp, cmd, light_buf_.buf, n_light);
    SDL_EndGPURenderPass(lp);
  }
  lights_.clear();

  // UI into the final texture when a screenshot wants it (so the PNG shows panels).
  const bool ui_in_final = draw_data && !screenshot_path_.empty() && screenshot_ui_;
  if (ui_in_final) {
    SDL_GPUColorTargetInfo ut{};
    ut.texture = final_;
    ut.load_op = SDL_GPU_LOADOP_LOAD;
    ut.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* up = SDL_BeginGPURenderPass(cmd, &ut, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, up);
    SDL_EndGPURenderPass(up);
  }
  // --- Present: blit final to swapchain, then UI on top ---
  SDL_GPUBlitInfo bi{};
  bi.source.texture = final_;
  bi.source.w = sw;
  bi.source.h = sh;
  bi.destination.texture = swap;
  bi.destination.w = sw;
  bi.destination.h = sh;
  bi.load_op = SDL_GPU_LOADOP_DONT_CARE;
  bi.filter = SDL_GPU_FILTER_NEAREST;
  SDL_BlitGPUTexture(cmd, &bi);
  if (draw_data && !ui_in_final) {
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
  src.texture = final_;
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
