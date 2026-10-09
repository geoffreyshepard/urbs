#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include "core/log.hpp"
#include "platform/platform.hpp"

int main(int, char**) {
  urbs::core::init_logging(false);
  urbs::platform::Window win;
  if (!urbs::platform::create_window(win, "Urbs", 1600, 900)) return 1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(win.window);
  ImGui_ImplSDLGPU3_InitInfo init{};
  init.Device = win.gpu;
  init.ColorTargetFormat = win.swapchain_format;
  init.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&init);

  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL3_ProcessEvent(&e);
      if (e.type == SDL_EVENT_QUIT) running = false;
    }
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Urbs");
    ImGui::Text("%.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
    ImGui::End();
    ImGui::Render();

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(win.gpu);
    SDL_GPUTexture* swap = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win.window, &swap, nullptr, nullptr);
    if (swap) {
      ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
      SDL_GPUColorTargetInfo ct{};
      ct.texture = swap;
      ct.clear_color = {0.55f, 0.42f, 0.30f, 1.0f};
      ct.load_op = SDL_GPU_LOADOP_CLEAR;
      ct.store_op = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
      ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
      SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  SDL_WaitForGPUIdle(win.gpu);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  urbs::platform::destroy_window(win);
  return 0;
}
