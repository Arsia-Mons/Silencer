// Stand-in payload for the stub e2e. Identity and behavior come from files
// beside the binary, so one build serves as any number of fake versions:
//   build-id.txt   - the id this payload reports (required)
//   exit-code.txt  - process exit code (default 0)
// Appends its id to $SILENCER_STUB_TEST_OUT/ran.txt.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string read_line(const fs::path &p) {
  std::ifstream in(p);
  std::string s;
  std::getline(in, s);
  while (!s.empty() && (s.back() == '\r' || s.back() == ' '))
    s.pop_back();
  return s;
}

int main(int, char **argv) {
  fs::path dir = fs::absolute(fs::path(argv[0])).parent_path();
  std::string id = read_line(dir / "build-id.txt");
  std::string code = read_line(dir / "exit-code.txt");
  const char *out = std::getenv("SILENCER_STUB_TEST_OUT");
  if (out && !id.empty()) {
    std::ofstream f(fs::path(out) / "ran.txt", std::ios::app);
    f << id << "\n";
  }
  return code.empty() ? 0 : std::atoi(code.c_str());
}
