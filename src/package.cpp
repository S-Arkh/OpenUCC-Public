#include "openucc/package.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace openucc {
namespace {
[[noreturn]] void fail(const std::string& text) {
  throw std::runtime_error(text);
}
void append(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }
void put32(Bytes& b, std::size_t p, std::uint32_t v) {
  if (p + 4 > b.size()) fail("Header write outside buffer");
  for (int i = 0; i < 4; ++i) b[p + i] = std::uint8_t(v >> (8 * i));
}
std::int32_t checked(std::size_t n) {
  if (n > std::size_t(INT32_MAX))
    fail("Package exceeds signed 32-bit format limit");
  return std::int32_t(n);
}
void utf8(std::string& s, std::uint32_t c) {
  if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
    fail("Invalid Unicode scalar");
  if (c < 128)
    s += char(c);
  else if (c < 2048) {
    s += char(0xc0 | (c >> 6));
    s += char(0x80 | (c & 63));
  } else if (c < 65536) {
    s += char(0xe0 | (c >> 12));
    s += char(0x80 | ((c >> 6) & 63));
    s += char(0x80 | (c & 63));
  } else {
    s += char(0xf0 | (c >> 18));
    s += char(0x80 | ((c >> 12) & 63));
    s += char(0x80 | ((c >> 6) & 63));
    s += char(0x80 | (c & 63));
  }
}
}  // namespace
Bytes Reader::take(std::size_t n) {
  if (pos > data.size() || n > data.size() - pos)
    fail("Truncated input at " + std::to_string(pos));
  Bytes result(data.begin() + pos, data.begin() + pos + n);
  pos += n;
  return result;
}
std::uint8_t Reader::byte() {
  if (pos >= data.size()) fail("Truncated input at " + std::to_string(pos));
  return data[pos++];
}
std::uint16_t Reader::u16() {
  auto a = byte();
  return std::uint16_t(a | (std::uint16_t(byte()) << 8));
}
std::uint32_t Reader::u32() {
  auto a = u16();
  return std::uint32_t(a) | (std::uint32_t(u16()) << 16);
}
std::int32_t Reader::i32() {
  auto v = u32();
  std::int32_t n;
  std::memcpy(&n, &v, 4);
  return n;
}
std::int32_t Reader::index() {
  auto first = byte();
  std::uint64_t value = first & 63;
  unsigned shift = 6;
  bool more = (first & 64) != 0;
  for (int i = 0; more && i < 4; ++i) {
    auto b = byte();
    if (i == 3 && (b & 0xe0)) fail("Compact index overflow");
    value |= std::uint64_t(b & 127) << shift;
    shift += 7;
    more = (b & 128) != 0;
  }
  if (more || value > ((first & 128) ? 0x80000000ull : 0x7fffffffull))
    fail("Compact index overflow");
  return std::int32_t((first & 128) ? -std::int64_t(value)
                                    : std::int64_t(value));
}
std::string Reader::string() {
  auto n = std::int64_t(index());
  auto count = std::size_t(n < 0 ? -n : n);
  if (count > (data.size() - pos) / (n < 0 ? 2 : 1)) fail("Truncated string");
  std::string s;
  for (std::size_t i = 0; i < count; ++i) {
    std::uint32_t c = n < 0 ? u16() : byte();
    if (i + 1 == count) {
      if (c) fail("Unterminated string");
      break;
    }
    if (!c) fail("Embedded null in name");
    if (n < 0 && c >= 0xd800 && c <= 0xdbff) {
      if (i + 2 >= count) fail("Truncated surrogate");
      auto lo = u16();
      ++i;
      if (lo < 0xdc00 || lo > 0xdfff) fail("Invalid surrogate");
      c = 0x10000 + ((c - 0xd800) << 10) + lo - 0xdc00;
    }
    utf8(s, c);
  }
  return s;
}
void u16(Bytes& b, std::uint16_t v) {
  b.push_back(std::uint8_t(v));
  b.push_back(std::uint8_t(v >> 8));
}
void u32(Bytes& b, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(std::uint8_t(v >> (8 * i)));
}
void index(Bytes& b, std::int32_t v) {
  auto n = std::uint32_t(v < 0 ? -std::int64_t(v) : v);
  auto first = std::uint8_t((v < 0 ? 128 : 0) | (n & 63));
  n >>= 6;
  b.push_back(first | (n ? 64 : 0));
  while (n) {
    auto c = std::uint8_t(n & 127);
    n >>= 7;
    b.push_back(c | (n ? 128 : 0));
  }
}
std::u16string utf16(const std::string& s) {
  std::u16string result;
  for (std::size_t i = 0; i < s.size();) {
    unsigned c = static_cast<unsigned char>(s[i++]), n = 0, min = 0;
    if (c >= 0xc2 && c <= 0xdf) {
      c &= 31;
      n = 1;
      min = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
      c &= 15;
      n = 2;
      min = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
      c &= 7;
      n = 3;
      min = 0x10000;
    } else if (c >= 128)
      fail("Invalid UTF-8 lead byte");
    for (unsigned j = 0; j < n; ++j) {
      if (i == s.size()) fail("Truncated UTF-8");
      auto b = static_cast<unsigned char>(s[i++]);
      if ((b & 0xc0) != 0x80) fail("Invalid UTF-8 continuation");
      c = (c << 6) | (b & 63);
    }
    if (!c || c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      fail("Invalid Unicode scalar");
    if (c < 65536)
      result += char16_t(c);
    else {
      c -= 0x10000;
      result += char16_t(0xd800 + (c >> 10));
      result += char16_t(0xdc00 + (c & 1023));
    }
  }
  return result;
}
void string(Bytes& b, const std::string& s) {
  auto wide = utf16(s);
  bool ascii =
      std::all_of(s.begin(), s.end(), [](unsigned char c) { return c < 128; });
  if (ascii) {
    index(b, checked(s.size() + 1));
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
  } else {
    index(b, -checked(wide.size() + 1));
    for (auto c : wide) u16(b, std::uint16_t(c));
    u16(b, 0);
  }
}
std::string lower(std::string s) {
  for (auto& c : s)
    if (c >= 'A' && c <= 'Z') c += 32;
  return s;
}
Bytes readFile(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) fail("Cannot open " + path.u8string());
  auto n = f.tellg();
  if (n < 0 || n > 1024 * 1024 * 1024) fail("Input exceeds 1 GiB limit");
  Bytes b(static_cast<std::size_t>(n));
  f.seekg(0);
  if (!b.empty() && !f.read(reinterpret_cast<char*>(b.data()), n))
    fail("Read failed");
  return b;
}
void writeNewFile(const std::filesystem::path& path, const Bytes& b) {
  if (std::filesystem::exists(path))
    fail("Output already exists: " + path.u8string());
  std::ofstream f(path, std::ios::binary);
  if (!f) fail("Cannot create " + path.u8string());
  f.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
  f.close();
  if (!f) fail("Output write failed");
}
bool supported(std::uint16_t v, std::uint16_t l) {
  static const std::set<int> legacy = {0,12,14,15,16,18,19,20,22,23,25,28,31,33,35,36,37,38};
  return (v == 118 && l == 0) || (v == 123 && legacy.count(l)) || (v == 126 && l == 0) ||
         (v == 125 && (l == 38 || l == 39)) || (v >= 126 && v <= 129 && l == 39) ||
         ((v >= 130 && v <= 133) && l == 40);
}
Package Package::load(const std::filesystem::path& path) {
  return parseFile(readFile(path), path);
}
Package Package::parseFile(Bytes data, const std::filesystem::path& path) {
  Bytes prefix;
  for (char c : std::string("Lineage2Ver121")) {
    prefix.push_back(std::uint8_t(c));
    prefix.push_back(0);
  }
  if (data.size() >= prefix.size() &&
      std::equal(prefix.begin(), prefix.end(), data.begin())) {
    std::uint8_t key = 0;
    for (unsigned char c : path.filename().u8string()) {
      if (c >= 128) fail("Ver121 requires a verified ASCII filename");
      if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
      key = std::uint8_t(key + c);
    }
    data.erase(data.begin(), data.begin() + prefix.size());
    for (auto& c : data) c ^= key;
    auto result = parse(data);
    result.envelope = "Lineage2Ver121";
    return result;
  }
  return parse(data);
}
Package Package::parse(const Bytes& raw) {
  Package p;
  Bytes data = raw;
  for (const std::string marker : {"Lineage2Ver111", "GamekitData111"}) {
    Bytes prefix;
    for (char c : marker) {
      prefix.push_back(std::uint8_t(c));
      prefix.push_back(0);
    }
    if (data.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), data.begin())) {
      data.erase(data.begin(), data.begin() + prefix.size());
      for (auto& c : data) c ^= 0xac;
      p.envelope = marker;
      break;
    }
  }
  Reader r{data};
  if (r.u32() != 0x9e2a83c1) fail("Unknown envelope or package signature");
  p.version = r.u16();
  p.licensee = r.u16();
  if (!supported(p.version, p.licensee))
    fail("Unsupported profile " + std::to_string(p.version) + "/" +
         std::to_string(p.licensee));
  p.flags = r.u32();
  auto nc = r.i32(), no = r.i32(), ec = r.i32(), eo = r.i32(), ic = r.i32(),
       io = r.i32();
  r.take(16);
  auto generations = r.i32();
  if (generations < 0 || std::size_t(generations) > (data.size() - r.pos) / 8)
    fail("Invalid generations");
  r.take(std::size_t(generations) * 8);
  p.header = Bytes(data.begin(), data.begin() + r.pos);
  using Span = std::pair<std::size_t, std::size_t>;
  std::vector<Span> spans{{0, r.pos}};
  for (auto table : {std::pair<int, int>{nc, no}, {ec, eo}, {ic, io}}) {
    if (table.first < 0 || table.first > 2000000 ||
        std::size_t(table.first) > data.size() || table.second < 0 ||
        std::size_t(table.second) > data.size())
      fail("Invalid table bounds");
  }
  r.pos = std::size_t(no);
  for (int i = 0; i < nc; ++i) {
    auto start = r.pos;
    auto text = r.string();
    auto end = r.pos;
    auto flags = r.u32();
    p.names.push_back(
        {text, flags, Bytes(data.begin() + start, data.begin() + end)});
  }
  if (nc) spans.push_back({std::size_t(no), r.pos});
  r.pos = std::size_t(io);
  for (int i = 0; i < ic; ++i) {
    Import e;
    e.package = r.index();
    e.type = r.index();
    e.outer = r.i32();
    e.name = r.index();
    p.imports.push_back(e);
  }
  if (ic) spans.push_back({std::size_t(io), r.pos});
  r.pos = std::size_t(eo);
  for (int i = 0; i < ec; ++i) {
    Export e;
    e.type = r.index();
    e.super = r.index();
    e.outer = r.i32();
    e.name = r.index();
    e.flags = r.u32();
    auto size = r.index();
    auto offset = size ? r.index() : 0;
    if (size < 0 || offset < 0 || std::size_t(offset) > data.size() ||
        std::size_t(size) > data.size() - std::size_t(offset))
      fail("Invalid export bounds");
    e.payload = Bytes(data.begin() + offset, data.begin() + offset + size);
    e.sourceOffset = std::size_t(offset);
    p.exports.push_back(std::move(e));
    if (size)
      spans.push_back(
          {std::size_t(offset), std::size_t(offset) + std::size_t(size)});
  }
  if (ec) spans.push_back({std::size_t(eo), r.pos});
  std::sort(spans.begin(), spans.end());
  for (std::size_t i = 1; i < spans.size(); ++i)
    if (spans[i].first < spans[i - 1].second)
      fail("Overlapping package regions");
  p.validate();
  return p;
}
std::string Package::name(std::int32_t i) const {
  if (i < 0 || std::size_t(i) >= names.size()) fail("Invalid name reference");
  return names[i].text;
}
std::string Package::ref(std::int32_t id) const {
  if (!id) return "None";
  std::set<std::int32_t> seen;
  std::vector<std::string> parts;
  while (id) {
    if (!seen.insert(id).second || seen.size() > 256)
      fail("Cyclic or excessive outer chain");
    if (id > 0) {
      if (std::size_t(id) > exports.size()) fail("Invalid export reference");
      auto& e = exports[id - 1];
      parts.push_back(name(e.name));
      id = e.outer;
    } else {
      auto i = -std::int64_t(id) - 1;
      if (std::size_t(i) >= imports.size()) fail("Invalid import reference");
      auto& e = imports[std::size_t(i)];
      parts.push_back(name(e.name));
      id = e.outer;
    }
  }
  std::string s;
  for (auto i = parts.rbegin(); i != parts.rend(); ++i) {
    if (!s.empty()) s += '.';
    s += *i;
  }
  return s;
}
std::string Package::kind(std::int32_t id) const {
  if (id <= 0 || std::size_t(id) > exports.size()) fail("Expected export");
  auto type = exports[id - 1].type;
  if (!type) return "Class";
  auto path = ref(type);
  auto dot = path.rfind('.');
  return dot == path.npos ? path : path.substr(dot + 1);
}
void Package::validate() const {
  if (!supported(version, licensee)) fail("Unsupported target profile");
  for (std::size_t i = 0; i < imports.size(); ++i) {
    auto& e = imports[i];
    name(e.package);
    name(e.type);
    ref(-std::int32_t(i) - 1);
  }
  for (std::size_t i = 0; i < exports.size(); ++i) {
    auto& e = exports[i];
    ref(std::int32_t(i) + 1);
    ref(e.type);
    ref(e.super);
  }
}
std::int32_t Package::intern(const std::string& text) {
  auto key = lower(text);
  if (indexedNames) {
    auto found = nameIndex.find(key);
    if (found != nameIndex.end()) return found->second;
    auto id = checked(names.size());
    names.push_back({text, 0x00070010, {}});
    nameIndex.emplace(std::move(key), id);
    return id;
  }
  for (std::size_t i = 0; i < names.size(); ++i)
    if (lower(names[i].text) == key) return checked(i);
  names.push_back({text, 0x00070010, {}});
  return checked(names.size() - 1);
}
void Package::indexNames(bool enabled) {
  nameIndex.clear();
  indexedNames = enabled;
  if (enabled)
    for (std::size_t i = 0; i < names.size(); ++i)
      nameIndex.emplace(lower(names[i].text), checked(i));
}
Bytes Package::serialize() const {
  validate();
  if (header.size() < 56) fail("Missing package summary");
  Bytes out = header;
  put32(out, 0, 0x9e2a83c1);
  put32(out, 4, std::uint32_t(version) | (std::uint32_t(licensee) << 16));
  put32(out, 8, flags);
  auto no = checked(out.size());
  for (auto& n : names) {
    if (n.encoded.empty())
      string(out, n.text);
    else {
      Reader r{n.encoded};
      if (r.string() != n.text || r.pos != n.encoded.size())
        fail("Stale encoded name");
      append(out, n.encoded);
    }
    u32(out, n.flags);
  }
  auto io = checked(out.size());
  for (auto& e : imports) {
    index(out, e.package);
    index(out, e.type);
    u32(out, std::uint32_t(e.outer));
    index(out, e.name);
  }
  // Payloads precede the export table: offsets are final, so no fixed-point
  // loop.
  std::vector<std::int32_t> offsets;
  for (auto& e : exports) {
    offsets.push_back(checked(out.size()));
    append(out, e.payload);
    for (const auto& fixup : e.lazyOffsets) {
      if (fixup.first > e.payload.size() ||
          e.payload.size() - fixup.first < 4 ||
          fixup.second > e.payload.size())
        fail("Invalid resource lazy-array relocation");
      put32(out, std::size_t(offsets.back()) + fixup.first,
            checked(std::size_t(offsets.back()) + fixup.second));
    }
  }
  auto eo = checked(out.size());
  for (std::size_t i = 0; i < exports.size(); ++i) {
    auto& e = exports[i];
    index(out, e.type);
    index(out, e.super);
    u32(out, std::uint32_t(e.outer));
    index(out, e.name);
    u32(out, e.flags);
    index(out, checked(e.payload.size()));
    if (!e.payload.empty()) index(out, offsets[i]);
  }
  put32(out, 12, checked(names.size()));
  put32(out, 16, no);
  put32(out, 20, checked(exports.size()));
  put32(out, 24, eo);
  put32(out, 28, checked(imports.size()));
  put32(out, 32, io);
  return out;
}
}  // namespace openucc
