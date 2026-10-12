#include "core/log.hpp"
#include "game/app.hpp"

int main(int argc, char** argv) {
  urbs::core::init_logging(false);
  urbs::game::App app;
  auto opt = urbs::game::parse_args(argc, argv);
  if (!app.init(opt)) {
    app.shutdown();
    return 1;
  }
  app.run();
  app.shutdown();
  return app.play_failures() > 0 ? 3 : 0;  // a playtest script's failed expectations
}
