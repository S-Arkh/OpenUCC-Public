#include <iostream>
#include <stdexcept>

#include "openucc/package.hpp"

// Authored minimal Core metadata for portable launcher tests, not a game file.
int generate(const std::filesystem::path& destination) {
  try {
    using namespace openucc;
    Package core;
    core.version = 133;
    core.licensee = 40;
    core.header.resize(56);
    core.names.push_back({"Object", 0, {}});
    core.names.push_back({"None", 0, {}});
    Bytes metadata;
    for (int i = 0; i < 6; ++i) index(metadata, 0);
    u32(metadata, 0xffffffff);
    u32(metadata, 0xffffffff);
    u32(metadata, 0);
    metadata.insert(metadata.end(), 22, 0);
    u32(metadata, 0x12);
    metadata.insert(metadata.end(), 16, 0);
    index(metadata, 0);
    index(metadata, 0);
    index(metadata, 0);
    index(metadata, 1);
    index(metadata, 0);
    index(metadata, 1);
    core.exports.push_back({0, 0, 0, 0, 0, metadata});
    writeNewFile(destination, core.serialize());
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
  if (argc != 2) return 2;
  return generate(std::filesystem::path(argv[1]));
}
#else
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  return generate(std::filesystem::u8path(argv[1]));
}
#endif
