#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openucc {
struct Type {
  enum Kind {
    Void,
    Int,
    Bool,
    Byte,
    Float,
    String,
    Name,
    Object,
    Class,
    Struct,
    Enum,
    Delegate,
    Pointer
  };
  enum class Shape { Scalar, StaticArray, DynamicArray };
  Kind kind = Void;
  Type() = default;
  Type(Kind value) : kind(value) {}
  static Type named(Kind, const std::string& identity);
  static Type array(Type element,
                    int dimension = 0);  // zero denotes a dynamic array
  bool isArray() const { return shape_ != Shape::Scalar; }
  bool isDynamicArray() const { return shape_ == Shape::DynamicArray; }
  bool isInt64() const {
    return !isArray() && kind == Struct && identity_ == "core.object.int64";
  }
  int dimension() const {
    return shape_ == Shape::StaticArray ? dimension_ : 1;
  }
  Type element() const;
  const std::string& identity() const { return identity_; }
  bool operator==(const Type&) const;
  bool operator!=(const Type& other) const { return !(*this == other); }
  bool operator<(const Type&) const;

 private:
  Shape shape_ = Shape::Scalar;
  int dimension_ = 1;
  std::string identity_;
};
struct Value {
  Type type = Type::Void;
  std::int32_t value = 0;
  std::string text;
  std::map<std::string, Value> members;
  std::vector<Value> elements;
  std::int64_t wide = 0;
};
struct Variable {
  std::string name;
  Type type = Type::Void;
  int id = 0;
  std::uint32_t flags = 0;
  std::string category;
  int inner = 0;
  int access = 0;
  Type owner;
};
struct Expr {
  Type type = Type::Void;
  int ref = 0;
  bool instance = false;
  std::int32_t value = 0;  // integer or exact IEEE-754 bits for Float
  int native = 0;
  std::vector<Expr> args;
  int call = 0;
  std::string callName;
  bool finalCall = false, writable = true;
  std::vector<bool> skipArgs;
  std::string text;
  int cast = -1;
  bool indexed = false;
  int arrayOp = 0;
  int objectOp = 0;
  std::shared_ptr<Expr> context;
  bool defaultRef = false;
  bool implicitOuterCall = false;
  bool iterator = false;
};
struct Statement {
  enum Kind {
    Return,
    Assign,
    If,
    While,
    Foreach,
    Assert,
    Evaluate,
    Break,
    Continue,
    For,
    Switch,
    Case,
    Label,
    Goto,
    Stop
  } kind;
  Expr lhs, rhs;
  std::vector<Statement> body, alternative, initializer, increment;
  std::string label;
  int line = 0;
};
std::optional<Type> primitiveType(const std::string& name);
std::string propertyKind(Type);
int primitiveCast(Type from, Type to);
Expr constant(Value);
}  // namespace openucc
