#pragma once
#include <memory>
#include <string>

#include "sim/world.hpp"

namespace urbs::script {

// Lua 5.4 mission/event scripting (sandboxed: no io, os, package, debug).
// Scripts run only at event points: on_start, on_day, choice callbacks, request outcomes.
// Persistent script state lives in the `urbs.state` table (numbers, strings, booleans), which is
// saved with the game; functions are re-created by re-running the script on load.
class LuaHost : public sim::World::ScriptHost {
 public:
  LuaHost(sim::World& w, std::string source, std::string chunk_name);
  ~LuaHost() override;
  bool start();  // runs the chunk; returns false and logs on error
  void on_start();
  void on_day() override;
  void on_choice(const std::string& callback, i32 option) override;
  void on_request(u16 id, bool fulfilled) override;
  [[nodiscard]] std::string export_state() const override;
  void import_state(const std::string& s) override;
  [[nodiscard]] const std::string& source() const { return source_; }
  [[nodiscard]] const std::string& last_error() const { return error_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  sim::World& w_;
  std::string source_, name_, error_;
};

}  // namespace urbs::script
