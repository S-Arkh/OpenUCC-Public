#include "openucc/ir.hpp"

#include <stdexcept>
#include <tuple>

#include "openucc/package.hpp"
namespace openucc {
Type Type::named(Kind kind, const std::string& identity) {
  if ((kind != Object && kind != Class && kind != Struct && kind != Enum &&
       kind != Delegate) ||
      identity.empty())
    throw std::runtime_error("Invalid named type identity");
  Type t(kind);
  t.identity_ = lower(identity);
  return t;
}
Type Type::array(Type element, int dimension) {
  if (element.isArray() || element.kind == Void || dimension < 0 ||
      dimension == 1 || dimension > 65535)
    throw std::runtime_error("Invalid array element or dimension");
  element.shape_ = dimension ? Shape::StaticArray : Shape::DynamicArray;
  element.dimension_ = dimension ? dimension : 1;
  return element;
}
Type Type::element() const {
  if (!isArray()) return *this;
  Type t = *this;
  t.shape_ = Shape::Scalar;
  t.dimension_ = 1;
  return t;
}
bool Type::operator==(const Type& other) const {
  return std::tie(kind, shape_, dimension_, identity_) ==
         std::tie(other.kind, other.shape_, other.dimension_, other.identity_);
}
bool Type::operator<(const Type& other) const {
  return std::tie(kind, shape_, dimension_, identity_) <
         std::tie(other.kind, other.shape_, other.dimension_, other.identity_);
}
std::optional<Type> primitiveType(const std::string& text) {
  auto n = lower(text);
  if (n == "int") return Type::Int;
  if (n == "int64") return Type::named(Type::Struct, "Core.Object.INT64");
  if (n == "bool") return Type::Bool;
  if (n == "byte") return Type::Byte;
  if (n == "float") return Type::Float;
  if (n == "string") return Type::String;
  if (n == "name") return Type::Name;
  if (n == "pointer") return Type::Pointer;
  return {};
}
std::string propertyKind(Type t) {
  if (t.isDynamicArray()) return "ArrayProperty";
  switch (t.kind) {
    case Type::Int:
      return "IntProperty";
    case Type::Bool:
      return "BoolProperty";
    case Type::Byte:
      return "ByteProperty";
    case Type::Float:
      return "FloatProperty";
    case Type::String:
      return "StrProperty";
    case Type::Name:
      return "NameProperty";
    case Type::Object:
      return "ObjectProperty";
    case Type::Class:
      return "ClassProperty";
    case Type::Struct:
      return "StructProperty";
    case Type::Enum:
      return "ByteProperty";
    case Type::Delegate:
      return "DelegateProperty";
    case Type::Pointer:
      return "PtrProperty";
    default:
      throw std::runtime_error("Void has no property representation");
  }
}
int primitiveCast(Type from, Type to) {
  if (from.isArray() || to.isArray())
    throw std::runtime_error("Array primitive conversion is not supported");
  if (from == to) return -1;
  const auto vector = Type::named(Type::Struct, "Core.Object.Vector");
  const auto rotator = Type::named(Type::Struct, "Core.Object.Rotator");
  if (from == rotator && to == vector) return 0x39;
  if (from == vector && to == rotator) return 0x50;
  if (from == vector && to == Type::String) return 0x58;
  if (from == rotator && to == Type::String) return 0x59;
  if (from == Type::String && to == vector) return 0x4d;
  if (from == Type::String && to == rotator) return 0x4e;
  if (from.isInt64() || to.isInt64()) {
    const Type types[] = {Type::Byte, Type::Int, Type::Bool, Type::Float,
                          Type::String};
    for (int i = 0; i < 5; ++i) {
      if (to.isInt64() && from == types[i]) return 0x5a + i;
      if (from.isInt64() && to == types[i]) return 0x5f + i;
    }
    throw std::runtime_error("Unsupported INT64 conversion");
  }
  if (from.kind == Type::Object || from.kind == Type::Class)
    from = Type::Object;
  struct Cast {
    Type from, to;
    int token;
  };
  static const Cast casts[] = {
      {Type::Object, Type::Bool, 0x47},  {Type::Object, Type::String, 0x56},
      {Type::Byte, Type::Int, 0x3a},     {Type::Byte, Type::Bool, 0x3b},
      {Type::Byte, Type::Float, 0x3c},   {Type::Int, Type::Byte, 0x3d},
      {Type::Int, Type::Bool, 0x3e},     {Type::Int, Type::Float, 0x3f},
      {Type::Bool, Type::Byte, 0x40},    {Type::Bool, Type::Int, 0x41},
      {Type::Bool, Type::Float, 0x42},   {Type::Float, Type::Byte, 0x43},
      {Type::Float, Type::Int, 0x44},    {Type::Float, Type::Bool, 0x45},
      {Type::Name, Type::Bool, 0x48},    {Type::String, Type::Byte, 0x49},
      {Type::String, Type::Int, 0x4a},   {Type::String, Type::Bool, 0x4b},
      {Type::String, Type::Float, 0x4c}, {Type::Byte, Type::String, 0x52},
      {Type::Int, Type::String, 0x53},   {Type::Bool, Type::String, 0x54},
      {Type::Float, Type::String, 0x55}, {Type::Name, Type::String, 0x57}};
  for (auto& c : casts)
    if (c.from == from && c.to == to) return c.token;
  throw std::runtime_error("Unsupported primitive conversion");
}
Expr constant(Value v) {
  Expr e;
  e.writable = false;
  e.type = v.type;
  e.value = v.value;
  e.text = std::move(v.text);
  if (v.type.isInt64()) {
    e.cast = 0x5e;
    e.args.push_back(constant({Type::String, 0, std::to_string(v.wide)}));
  }
  if (v.type.kind == Type::Object) {
    e.objectOp = v.value ? 0x20 : 0x2a;
    e.ref = v.value;
  }
  if (v.type.kind == Type::Class && !e.text.empty()) e.objectOp = 0x20;
  if (v.type.identity() == "core.object.vector" ||
      v.type.identity() == "core.object.rotator") {
    bool vector = v.type.identity() == "core.object.vector";
    e.objectOp = vector ? 0x23 : 0x22;
    for (auto& name : vector ? std::vector<std::string>{"X", "Y", "Z"}
                             : std::vector<std::string>{"Pitch", "Yaw", "Roll"})
      e.args.push_back(constant(v.members.at(name)));
  }
  return e;
}
}  // namespace openucc
