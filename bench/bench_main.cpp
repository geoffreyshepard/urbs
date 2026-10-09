// Benchmarks: run the stress scenario headless and print JSON. Compare with bench/baseline.json.
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  std::string exe = argc > 0 ? argv[0] : "./bench";
  std::string dir = exe.substr(0, exe.find_last_of('/') + 1);
  std::string cmd = dir + "urbs_headless --scenario " URBS_SOURCE_DIR "/tests/scenarios/stress_50k.json --ticks 2400";
  return std::system(cmd.c_str());
}
