#pragma once
#include <SDL3/SDL.h>

#include <string>

namespace urbs::platform {

struct Window {
  SDL_Window* window = nullptr;
  SDL_GPUDevice* gpu = nullptr;
  SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;
};

// Initializes SDL video, creates the window and the GPU device and claims the window.
bool create_window(Window& out, const char* title, int w, int h, bool vsync = true);
void destroy_window(Window& w);

// Directory for saves and settings (SDL_GetPrefPath).
std::string user_data_dir();
// Read-only data root (data/, assets/). Looks next to the executable, then the source tree.
std::string data_root();

}  // namespace urbs::platform
