#include "openucc/metadata.hpp"

#include <set>
#include <stdexcept>
namespace openucc {
std::vector<std::string> enumNames(const Package& p, int id) {
  if (id <= 0 || std::size_t(id) > p.exports.size() || p.kind(id) != "Enum")
    throw std::runtime_error("Invalid enum export");
  Reader r{p.exports[id - 1].payload};
  if (p.name(r.index()) != "None")
    throw std::runtime_error("Unsupported enum object defaults");
  p.ref(r.index());
  p.ref(r.index());
  auto count = r.index();
  if (count < 1 || count > 256) throw std::runtime_error("Invalid enum size");
  std::vector<std::string> result;
  std::set<std::string> seen;
  for (int i = 0; i < count; ++i) {
    auto name = p.name(r.index());
    if (!seen.insert(lower(name)).second)
      throw std::runtime_error("Duplicate enum value");
    result.push_back(std::move(name));
  }
  if (r.pos != r.data.size()) throw std::runtime_error("Trailing enum data");
  return result;
}
namespace {
const Export& entry(const Package& p, int id) {
  if (id <= 0 || std::size_t(id) > p.exports.size())
    throw std::runtime_error("Invalid metadata export");
  return p.exports[id - 1];
}
struct ScriptReader {
  const Package& p;
  Reader& r;
  int pc = 0;
  std::size_t count = 0;
  int byte() {
    ++pc;
    return r.byte();
  }
  int word() {
    pc += 2;
    return r.u16();
  }
  int integer() {
    pc += 4;
    return r.i32();
  }
  int ref(bool name = false) {
    pc += 4;
    auto v = r.index();
    if (name)
      p.name(v);
    else
      p.ref(v);
    return v;
  }
  int expression(int depth = 0) {
    if (depth > 128 || ++count > 1000000)
      throw std::runtime_error("Bytecode complexity limit");
    auto op = byte();
    auto child = [&]() { return expression(depth + 1); };
    auto args = [&]() {
      while (child() != 0x16) {
      }
    };
    if (op >= 0x60) {
      if (op < 0x70) byte();
      args();
      return op;
    }
    switch (op) {
      case 0:
      case 1:
      case 2:
      case 0x29:
        ref();
        break;
      case 4:
      case 0x0d:
      case 0x0e:
      case 0x37:
      case 0x2d:
        child();
        break;
      case 0x0f:
      case 0x14:
      case 0x10:
      case 0x1a:
      case 0x45:
        child();
        child();
        break;
      case 0x39:
        byte();
        child();
        break;
      case 0x18:
      case 9:
        word();
        child();
        break;
      case 0x40:
      case 0x41:
        child();
        child();
        child();
        break;
      case 0x11:
        child();
        child();
        child();
        child();
        break;
      case 0x32:
      case 0x33:
        ref();
        child();
        child();
        break;
      case 0x36:
      case 0x13:
      case 0x2e:
        ref();
        child();
        break;
      case 0x2f:
        child();
        word();
        break;
      case 0x0c: {
        if (pc & 3) throw std::runtime_error("Unaligned state label table");
        for (;;) {
          auto name = p.name(ref(true));
          integer();
          if (name == "None") break;
        }
        break;
      }
      case 6:
        word();
        break;
      case 7:
        word();
        child();
        break;
      case 10:
        if (word() != 65535) child();
        break;
      case 5:
        byte();
        child();
        break;
      case 0x1c:
        ref();
        args();
        break;
      case 0x1b:
      case 0x38:
        ref(true);
        args();
        break;
      case 0x19:
      case 0x12:
        child();
        word();
        byte();
        child();
        break;
      case 0x1d:
      case 0x1e:
        integer();
        break;
      case 0x24:
      case 0x2c:
        byte();
        break;
      case 0x46:
        if (p.version != 133 || p.licensee != 40)
          throw std::runtime_error("INT64 opcode outside target profile");
        integer();
        integer();
        break;
      case 0x47:
        if (p.version != 133 || p.licensee != 40)
          throw std::runtime_error("Sort opcode outside target profile");
        child();
        child();
        break;
      case 0x1f:
        while (byte()) {
        }
        break;
      case 0x34:
        while (word()) {
        }
        break;
      case 0x20:
        ref();
        break;
      case 0x21:
        ref(true);
        break;
      case 0x22:
      case 0x23:
        integer();
        integer();
        integer();
        break;
      case 0x35:
        integer();
        integer();
        break;
      case 0x43:
        ref();
        ref(true);
        args();
        break;
      case 0x44:
        ref(true);
        break;
      case 0x42:
        integer();
        integer();
        integer();
        while (byte()) {
        }
        break;
      case 8:
      case 11:
      case 22:
      case 23:
      case 37:
      case 38:
      case 39:
      case 40:
      case 42:
      case 48:
      case 49:
        break;
      default:
        throw std::runtime_error("Unsupported bytecode token " +
                                 std::to_string(op) + " at PC " +
                                 std::to_string(pc - 1));
    }
    return op;
  }
  void script(int size) {
    if (size < 0 || std::size_t(size) > r.data.size() * 4)
      throw std::runtime_error("Invalid script memory size");
    while (pc < size) expression();
    if (pc != size) throw std::runtime_error("Script memory size mismatch");
  }
};
}  // namespace
PropertyInfo propertyInfo(const Package& p, int id) {
  Reader r{entry(p, id).payload};
  PropertyInfo prop;
  prop.id = id;
  prop.kind = p.kind(id);
  if (p.name(r.index()) != "None")
    throw std::runtime_error("Tagged property metadata unsupported");
  p.ref(r.index());
  prop.next = r.index();
  p.ref(prop.next);
  prop.dimension = r.i32();
  prop.flags = r.u32();
  p.name(r.index());
  if (prop.flags & 0x20) prop.replicationOffset = r.u16();
  if (prop.kind == "ByteProperty" || prop.kind == "ObjectProperty" ||
      prop.kind == "ClassProperty" || prop.kind == "StructProperty" ||
      prop.kind == "ArrayProperty" || prop.kind == "DelegateProperty") {
    prop.reference = r.index();
    p.ref(prop.reference);
  }
  if (prop.kind == "ClassProperty") {
    prop.metaclass = r.index();
    p.ref(prop.metaclass);
  }
  if (prop.dimension < 1)
    throw std::runtime_error("Invalid property dimension");
  return prop;
}
NativeInfo functionInfo(const Package& p, int id) {
  auto& e = entry(p, id);
  if (p.kind(id) != "Function")
    throw std::runtime_error("Expected function export");
  if (e.flags & 0x02000000)
    throw std::runtime_error("Serialized execution stack in function metadata");
  Reader r{e.payload};
  if (p.name(r.index()) != "None")
    throw std::runtime_error("Tagged function properties unsupported");
  p.ref(r.index());
  p.ref(r.index());
  p.ref(r.index());
  auto child = r.index();
  p.ref(child);
  auto friendly = r.index();
  p.ref(r.index());
  r.take(8);
  auto size = r.i32();
  ScriptReader reader{p, r};
  reader.script(size);
  NativeInfo f;
  f.id = id;
  f.symbol = p.name(friendly);
  f.native = r.u16();
  f.precedence = r.byte();
  f.flags = r.u32();
  if (f.flags & 0x40) f.replicationOffset = r.u16();
  if (r.pos != r.data.size())
    throw std::runtime_error("Trailing function metadata");
  std::set<int> seen;
  while (child) {
    if (!seen.insert(child).second)
      throw std::runtime_error("Cyclic property chain");
    if (entry(p, child).outer != id)
      throw std::runtime_error("Property owner mismatch");
    auto prop = propertyInfo(p, child);
    child = prop.next;
    if (prop.flags & 0x400) {
      if (f.result) throw std::runtime_error("Duplicate return property");
      f.result = prop;
    } else if (prop.flags & 0x80)
      f.parameters.push_back(prop);
  }
  return f;
}
StructInfo structInfo(const Package& p, int id) {
  if (p.kind(id) != "Struct")
    throw std::runtime_error("Expected struct export");
  Reader r{entry(p, id).payload};
  if (p.name(r.index()) != "None")
    throw std::runtime_error("Tagged struct properties unsupported");
  StructInfo result;
  result.super = r.index();
  result.next = r.index();
  p.ref(result.super);
  p.ref(result.next);
  p.ref(r.index());
  auto child = r.index();
  p.name(r.index());
  p.ref(r.index());
  r.take(8);
  auto size = r.i32();
  ScriptReader script{p, r};
  script.script(size);
  if (r.pos != r.data.size())
    throw std::runtime_error("Trailing struct metadata");
  std::set<int> seen;
  while (child) {
    if (!seen.insert(child).second)
      throw std::runtime_error("Cyclic struct field chain");
    if (entry(p, child).outer != id)
      throw std::runtime_error("Struct field owner mismatch");
    if (p.kind(child) == "Struct" || p.kind(child) == "Enum" ||
        p.kind(child) == "Const") {
      Reader nested{entry(p, child).payload};
      nested.index();
      nested.index();
      child = nested.index();
    } else {
      auto field = propertyInfo(p, child);
      result.fields.push_back(field);
      child = field.next;
    }
  }
  return result;
}
std::vector<NativeInfo> nativeOperators(const Package& p, int owner) {
  std::vector<NativeInfo> out;
  for (std::size_t i = 0; i < p.exports.size(); ++i) {
    int id = int(i) + 1;
    auto& e = p.exports[i];
    if (e.outer != owner || p.kind(id) != "Function") continue;
    auto f = functionInfo(p, id);
    if (!(f.flags & 0x1000) || !(f.flags & 0x400) || !f.native) continue;
    if (f.native >= 4096)
      throw std::runtime_error("Native index exceeds extended opcode range");
    if (f.result) out.push_back(std::move(f));
  }
  return out;
}
StateInfo stateInfo(const Package& p, int id) {
  bool cls = p.kind(id) == "Class";
  if (!cls && p.kind(id) != "State")
    throw std::runtime_error("Expected state export");
  Reader r{entry(p, id).payload};
  if (!cls && p.name(r.index()) != "None")
    throw std::runtime_error("Tagged state properties unsupported");
  StateInfo info;
  info.super = r.index();
  info.next = r.index();
  p.ref(r.index());
  info.children = r.index();
  p.name(r.index());
  p.ref(r.index());
  r.take(8);
  auto size = r.i32();
  ScriptReader script{p, r};
  script.script(size);
  auto low = r.u32();
  info.probe = low | (std::uint64_t(r.u32()) << 32);
  low = r.u32();
  info.ignore = low | (std::uint64_t(r.u32()) << 32);
  info.labelOffset = r.u16();
  info.flags = r.u32();
  if (!cls && r.pos != r.data.size())
    throw std::runtime_error("Trailing state metadata");
  return info;
}
namespace {
ClassInfo classHeader(const Package& p, int id, Reader& r) {
  if (p.kind(id) != "Class") throw std::runtime_error("Expected class export");
  for (int i = 0; i < 4; ++i) p.ref(r.index());
  p.name(r.index());
  p.ref(r.index());
  r.take(8);
  auto size = r.i32();
  ScriptReader script{p, r};
  script.script(size);
  r.take(8 + 8 + 2 + 4);
  ClassInfo info;
  info.flags = r.u32();
  r.take(16);
  auto count = r.index();
  if (count < 0 || count > 1000000)
    throw std::runtime_error("Invalid class dependency count");
  for (int i = 0; i < count; ++i) {
    p.ref(r.index());
    r.take(8);
  }
  count = r.index();
  if (count < 0 || count > 100000)
    throw std::runtime_error("Invalid class package imports");
  while (count--) p.name(r.index());
  info.within = r.index();
  p.ref(info.within);
  info.config = p.name(r.index());
  count = r.index();
  if (count < 0 || count > 100000)
    throw std::runtime_error("Invalid hidden categories");
  while (count--) p.name(r.index());
  return info;
}
}  // namespace
ClassInfo classInfo(const Package& p, int id) {
  Reader r{entry(p, id).payload};
  return classHeader(p, id, r);
}
std::vector<DefaultTag> classDefaults(const Package& p, int id) {
  Reader r{entry(p, id).payload};
  classHeader(p, id, r);
  auto tags = defaultTags(p, r);
  if (r.pos != r.data.size())
    throw std::runtime_error("Trailing class defaults");
  return tags;
}
std::vector<DefaultTag> defaultTags(const Package& p, Reader& r) {
  std::vector<DefaultTag> tags;
  for (;;) {
    auto name = p.name(r.index());
    if (lower(name) == "none") break;
    if (tags.size() >= 1000000) throw std::runtime_error("Default tag limit");
    auto info = r.byte();
    DefaultTag tag;
    tag.name = name;
    tag.kind = info & 15;
    if (tag.kind == 10) tag.structName = p.name(r.index());
    int bytes = 0;
    switch ((info >> 4) & 7) {
      case 0:
        bytes = 1;
        break;
      case 1:
        bytes = 2;
        break;
      case 2:
        bytes = 4;
        break;
      case 3:
        bytes = 12;
        break;
      case 4:
        bytes = 16;
        break;
      case 5:
        bytes = r.byte();
        break;
      case 6:
        bytes = r.u16();
        break;
      case 7:
        bytes = r.i32();
        break;
    }
    if ((info & 128) && tag.kind != 3) {
      int b = r.byte();
      if (b < 128)
        tag.subscript = b;
      else if (b < 192)
        tag.subscript = ((b & 63) << 8) | r.byte();
      else {
        tag.subscript = (b & 63) << 24;
        tag.subscript |= int(r.byte()) << 16;
        tag.subscript |= int(r.byte()) << 8;
        tag.subscript |= r.byte();
      }
    }
    if (bytes < 0) throw std::runtime_error("Negative default payload size");
    if (tag.kind != 3)
      tag.data = r.take(std::size_t(bytes));
    else
      tag.data.push_back((info & 128) ? 1 : 0);
    tags.push_back(std::move(tag));
  }
  return tags;
}
}  // namespace openucc
