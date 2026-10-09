#include "platform/platform.hpp"

#include <filesystem>

#include "core/log.hpp"

namespace urbs::platform {

bool create_window(Window& out, const char* title, int w, int h) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    URBS_LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
    return false;
  }
  out.window = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!out.window) {
    URBS_LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
    return false;
  }
  const bool debug_gpu =
#ifdef NDEBUG
      false;
#else
      true;
#endif
  out.gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
                                debug_gpu, nullptr);
  if (!out.gpu) {
    URBS_LOG_ERROR("SDL_CreateGPUDevice failed: {}", SDL_GetError());
    return false;
  }
  if (!SDL_ClaimWindowForGPUDevice(out.gpu, out.window)) {
    URBS_LOG_ERROR("SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError());
    return false;
  }
  SDL_SetGPUSwapchainParameters(out.gpu, out.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
  out.swapchain_format = SDL_GetGPUSwapchainTextureFormat(out.gpu, out.window);
  URBS_LOG_INFO("GPU driver: {}", SDL_GetGPUDeviceDriver(out.gpu));
  return true;
}

void destroy_window(Window& w) {
  if (w.gpu) {
    SDL_WaitForGPUIdle(w.gpu);
    if (w.window) SDL_ReleaseWindowFromGPUDevice(w.gpu, w.window);
    SDL_DestroyGPUDevice(w.gpu);
  }
  if (w.window) SDL_DestroyWindow(w.window);
  SDL_Quit();
  w = {};
}

std::string user_data_dir() {
  char* p = SDL_GetPrefPath("urbs", "urbs");
  std::string s = p ? p : "./";
  SDL_free(p);
  return s;
}

std::string data_root() {
  namespace fs = std::filesystem;
  const char* base = SDL_GetBasePath();
  if (base) {
    for (const char* rel : {"", "../Resources/", "../../", "../../../"}) {
      fs::path p = fs::path(base) / rel;
      if (fs::exists(p / "data" / "defs")) return fs::weakly_canonical(p).string() + "/";
    }
  }
#ifdef URBS_SOURCE_DIR
  return std::string(URBS_SOURCE_DIR) + "/";
#else
  return "./";
#endif
}

}  // namespace urbs::platform
