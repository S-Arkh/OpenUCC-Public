#include "openucc/resources.hpp"
#include <iostream>
#include <stdexcept>
using namespace openucc;
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error("Sound check failed"); }
template<class F> void rejects(F f) {
  try { f(); } catch (const std::exception&) { return; }
  throw std::runtime_error("Malformed sound accepted");
}
}
int main() {
  try {
    Package p; p.version = 133; p.licensee = 40; p.header.resize(56);
    Import cls; cls.package = p.intern("Engine"); cls.type = p.intern("Class");
    cls.name = p.intern("Sound"); p.imports.push_back(cls);
    Export e; e.type = -1; e.name = p.intern("Sample"); p.exports.push_back(e);
    ResourceBody sound; sound.kind = "Sound"; sound.soundFormat = "WAV";
    // A complete mono 16-bit PCM wave with two samples, independent of the codec.
    sound.soundData = {'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
      1,0,1,0,0x40,0x1f,0,0,0x80,0x3e,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,0x80,0xff,0x7f};
    encodeResource(sound, p, p.exports[0], [](int id) { return id; });
    auto lazy = p.exports[0].lazyOffsets.at(0);
    auto original = Package::parse(p.serialize()); auto decoded = decodeResource(original, 1);
    check(decoded.soundData == sound.soundData);
    Package moved = original; moved.names.clear();
    for (int i = 0; i < 130; ++i) moved.intern("Padding" + std::to_string(i));
    moved.imports[0].package = moved.intern("Engine"); moved.imports[0].type = moved.intern("Class");
    moved.imports[0].name = moved.intern("Sound"); moved.exports[0].name = moved.intern("Sample");
    encodeResource(decoded, moved, moved.exports[0], [](int id) { return id; });
    auto parsed = Package::parse(moved.serialize()); auto result = decodeResource(parsed, 1);
    check(result.soundFormat == "WAV" && result.soundData == sound.soundData);
    check(parsed.exports[0].sourceOffset != original.exports[0].sourceOffset);
    for (auto profile : {std::pair<int,int>{131,40}, {130,40}, {128,39}, {123,23}}) {
    moved.version = profile.first; moved.licensee = profile.second;
    encodeResource(decoded, moved, moved.exports[0], [](int id) { return id; });
    auto legacy = Package::parse(moved.serialize());
    check(decodeResource(legacy, 1).soundData == sound.soundData);
    legacy.exports[0].payload.back() ^= 1;
    check(decodeResource(legacy, 1).soundData != sound.soundData);
    }
    moved.version = 129;
    rejects([&] { encodeResource(decoded, moved, moved.exports[0], [](int id) { return id; }); });
    auto corrupt = original; corrupt.exports[0].payload[lazy.first] ^= 1;
    rejects([&] { decodeResource(corrupt, 1); });
    corrupt = original; corrupt.exports[0].payload.pop_back();
    rejects([&] { decodeResource(corrupt, 1); });
    corrupt = original; corrupt.exports[0].payload.push_back(0);
    rejects([&] { decodeResource(corrupt, 1); });
    sound.soundFormat = "Unknown";
    rejects([&] { encodeResource(sound, p, p.exports[0], [](int id) { return id; }); });
    std::cout << "Sound data, name and offset checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
