#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace openucc {
using Bytes = std::vector<std::uint8_t>;
struct Reader {
  const Bytes& data;
  std::size_t pos = 0;
  Bytes take(std::size_t size);
  std::uint8_t byte();
  std::uint16_t u16();
  std::uint32_t u32();
  std::int32_t i32();
  std::int32_t index();
  std::string string();
};
void u16(Bytes&, std::uint16_t);
void u32(Bytes&, std::uint32_t);
void index(Bytes&, std::int32_t);
void string(Bytes&, const std::string&);
std::u16string utf16(const std::string&);
Bytes readFile(const std::filesystem::path&);
void writeNewFile(const std::filesystem::path&, const Bytes&);
std::string lower(std::string);
struct Name {
  std::string text;
  std::uint32_t flags = 0;
  Bytes encoded;
};
struct Import {
  std::int32_t package = 0, type = 0, outer = 0, name = 0;
};
struct Export {
  std::int32_t type = 0, super = 0, outer = 0, name = 0;
  std::uint32_t flags = 0;
  Bytes payload;
  std::size_t sourceOffset = 0;
  // (word position, target position), both relative to this payload.
  std::vector<std::pair<std::size_t, std::size_t>> lazyOffsets;
};
// The host never constructs target UObjects. Indices are target identifiers.
struct Package {
  std::uint16_t version = 0, licensee = 0;
  std::uint32_t flags = 0;
  std::string envelope = "plain";
  Bytes header;
  std::vector<Name> names;
  std::vector<Import> imports;
  std::vector<Export> exports;
  static Package parse(const Bytes&);
  static Package load(const std::filesystem::path&);
  // Parse a previously read file snapshot, using its filename for Ver121.
  static Package parseFile(Bytes, const std::filesystem::path&);
  Bytes serialize() const;
  void validate() const;
  std::string name(std::int32_t) const;
  std::string ref(std::int32_t) const;
  std::string kind(std::int32_t) const;
  std::int32_t intern(const std::string&);
  // Construction-only acceleration. Do not edit names directly while enabled.
  void indexNames(bool enabled);

 private:
  bool indexedNames = false;
  std::map<std::string, std::int32_t> nameIndex;
};
bool supported(std::uint16_t version, std::uint16_t licensee);
}  // namespace openucc
