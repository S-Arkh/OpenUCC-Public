#include "openucc/compiler.hpp"
#include "openucc/resources.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "openucc/ir.hpp"
#include "openucc/metadata.hpp"

namespace openucc {
namespace {
struct Token {
  std::string text;
  int line = 1, column = 1;
  char quote = 0;
  std::size_t offset = 0;
};
struct Lexer {
  std::string file;
  std::shared_ptr<const std::string> storage;
  std::string_view source;
  Lexer() = default;
  Lexer(std::string name, std::string text)
      : file(std::move(name)),
        storage(std::make_shared<const std::string>(std::move(text))),
        source(*storage) {}
  std::size_t at = 0;
  int line = 1, column = 1;
  Token token;
  [[noreturn]] void error(const std::string& text) const {
    throw std::runtime_error(file + ":" + std::to_string(token.line) + ":" +
                             std::to_string(token.column) + ": " + text);
  }
  char get() {
    auto c = source[at++];
    if (c == '\n') {
      ++line;
      column = 1;
    } else
      ++column;
    return c;
  }
  bool starts(const char* s) const {
    return source.compare(at, std::char_traits<char>::length(s), s) == 0;
  }
  void next() {
    for (;;) {
      while (at < source.size() && (source[at] == ' ' || source[at] == '\t' ||
                                    source[at] == '\r' || source[at] == '\n'))
        get();
      if (starts("//")) {
        while (at < source.size() && get() != '\n') {
        }
        continue;
      }
      if (starts("/*")) {
        get();
        get();
        int depth = 1;
        while (depth && at < source.size()) {
          if (starts("/*")) {
            get();
            get();
            ++depth;
          } else if (starts("*/")) {
            get();
            get();
            --depth;
          } else
            get();
        }
        if (depth) error("Unterminated comment");
        continue;
      }
      break;
    }
    token = {"", line, column};
    token.offset = at;
    if (at == source.size()) return;
    auto c = get();
    if (c == '"' || c == '\'') {
      token.quote = c;
      bool closed = false;
      while (at < source.size()) {
        char v = get();
        if (v == c) {
          closed = true;
          break;
        }
        if (v == '\n' || v == '\r') error("Newline in literal");
        if (v == '\\') {
          if (at == source.size()) error("Unterminated escape");
          v = get();
          switch (v) {
            case 'n':
              v = '\n';
              break;
            case 'r':
              v = '\r';
              break;
            case 't':
              v = '\t';
              break;
            case '\\':
            case '"':
            case '\'':
              break;
            default:
              error("Unsupported escape");
          }
        }
        token.text += v;
      }
      if (!closed) error("Unterminated literal");
      utf16(token.text);
      return;
    }
    token.text += c;
    auto ident = [](char x) {
      return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_';
    };
    if (ident(c))
      while (at < source.size() &&
             (ident(source[at]) || (source[at] >= '0' && source[at] <= '9')))
        token.text += get();
    else if ((c >= '0' && c <= '9') ||
             (c == '.' && at < source.size() && source[at] >= '0' &&
              source[at] <= '9')) {
      if (c == '0' && at < source.size() &&
          (source[at] == 'x' || source[at] == 'X')) {
        token.text += get();
        while (at < source.size() &&
               ((source[at] >= '0' && source[at] <= '9') ||
                (source[at] >= 'a' && source[at] <= 'f') ||
                (source[at] >= 'A' && source[at] <= 'F')))
          token.text += get();
      } else {
        while (at < source.size() && source[at] >= '0' && source[at] <= '9')
          token.text += get();
        if (c != '.' && at < source.size() && source[at] == '.') {
          token.text += get();
          while (at < source.size() && source[at] >= '0' && source[at] <= '9')
            token.text += get();
        }
        if (at < source.size() && (source[at] == 'e' || source[at] == 'E')) {
          token.text += get();
          if (at < source.size() && (source[at] == '+' || source[at] == '-'))
            token.text += get();
          while (at < source.size() && source[at] >= '0' && source[at] <= '9')
            token.text += get();
        }
        if (at < source.size() && (source[at] == 'f' || source[at] == 'F'))
          token.text += get();
      }
    } else if (std::string(";:{}()[]=,-+*/<>!&|^~%.$@").find(c) ==
               std::string::npos)
      error("Unsupported character or operator: " + token.text);
    else {
      for (const std::string op :
           {">>>", "<<", ">>", "==", "!=", "~=", "<=", ">=", "&&", "||", "^^",
            "++", "--", "+=", "-=", "*=", "/=", "**"})
        if (token.text == op.substr(0, 1) &&
            source.compare(at, op.size() - 1, op.substr(1)) == 0) {
          for (std::size_t i = 1; i < op.size(); ++i) token.text += get();
          break;
        }
    }
  }
  bool is(const std::string& s) const {
    return !token.quote && lower(token.text) == s;
  }
  bool end() const { return token.text.empty() && !token.quote; }
  bool accept(const std::string& s) {
    if (!is(s)) return false;
    next();
    return true;
  }
  void expect(const std::string& s) {
    if (!accept(s)) error("Expected '" + s + "', found '" + token.text + "'");
  }
  std::string identifier() {
    auto s = token.text;
    if (token.quote || s.empty() ||
        !((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') ||
          s[0] == '_'))
      error("Expected identifier");
    if (s.size() > 63) error("Identifier exceeds target 63-character limit");
    next();
    return s;
  }
};
struct Function {
  std::string name, friendly;
  Type type = Type::Void;
  bool isStatic = false;
  int id = 0;
  std::vector<Variable> params, locals;
  std::vector<Statement> body;
  std::optional<Lexer> bodyStart;
  int super = 0, native = 0, precedence = 0;
  bool isFinal = false;
  std::uint32_t inheritedFlags = 0;
  std::uint32_t flags = 0;
  bool defined = true;
  int returnId = 0, returnInner = 0;
  Type owner;
  int state = 0;
  bool stateScript = false;
  int delegateStorage = 0;
};
struct State {
  std::string name, baseName;
  int id = 0, super = 0;
  std::optional<Symbol> externalSuper;
  std::uint32_t flags = 0;
  std::uint64_t probe = 0, ignore = UINT64_MAX;
  std::vector<std::string> ignores;
  Function script;
};
struct Replication {
  Lexer start;
  Expr condition;
  bool reliable = false;
  std::vector<std::string> names;
  std::vector<int> members;
};
int probeBit(const std::string& name) {
  static const char* names[] = {"Spawned",
                                "Destroyed",
                                "GainedChild",
                                "LostChild",
                                "",
                                "",
                                "Trigger",
                                "UnTrigger",
                                "Timer",
                                "HitWall",
                                "Falling",
                                "Landed",
                                "PhysicsVolumeChange",
                                "Touch",
                                "UnTouch",
                                "Bump",
                                "BeginState",
                                "EndState",
                                "BaseChange",
                                "Attach",
                                "Detach",
                                "ActorEntered",
                                "ActorLeaving",
                                "ZoneChange",
                                "AnimEnd",
                                "EndedRotation",
                                "InterpolateEnd",
                                "EncroachingOn",
                                "EncroachedBy",
                                "NotifyTurningInPlace",
                                "HeadVolumeChange",
                                "PostTouch",
                                "PawnEnteredVolume",
                                "MayFall",
                                "CheckDirectionChange",
                                "PawnLeavingVolume",
                                "Tick",
                                "PlayerTick",
                                "ModifyVelocity",
                                "CheckMovementTransition",
                                "SeePlayer",
                                "EnemyNotVisible",
                                "HearNoise",
                                "UpdateEyeHeight",
                                "SeeMonster",
                                "SeeFriend",
                                "SpecialHandling",
                                "BotDesireability",
                                "NotifyBump",
                                "NotifyPhysicsVolumeChange",
                                "AIHearSound",
                                "NotifyHeadVolumeChange",
                                "NotifyLanded",
                                "NotifyHitWall",
                                "PostNetReceive",
                                "PreBeginPlay",
                                "BeginPlay",
                                "PostBeginPlay",
                                "",
                                "PhysicsChangedFor",
                                "ActorEnteredVolume",
                                "ActorLeavingVolume",
                                "PrepareForMove",
                                ""};
  for (int i = 0; i < 64; ++i)
    if (*names[i] && lower(name) == lower(names[i])) return i;
  return -1;
}
struct Constant {
  std::string name, text;
  Value value;
  int id = 0;
};
struct Enumeration {
  std::string name, path;
  Type type;
  int id = 0, outer = 0;
  std::vector<std::string> names;
};
struct Structure {
  std::string name, path;
  Type type, base;
  int id = 0, outer = 0;
  std::vector<Variable> fields;
  std::vector<Type> nested;
  bool complete = false;
};
struct DefaultValues {
  std::map<std::string, Value> defaults;
  std::map<std::string, Variable> defaultFields;
  std::map<std::string, int> defaultIndices;
  std::map<std::string, std::vector<Value>> arrayDefaults;
  std::set<std::string> arrayDefaultElements;
};
struct DefaultObject : DefaultValues {
  std::string name, path;
  Type type;
  int id = 0, outer = 0;
  std::optional<Lexer> body;
};
struct Class : DefaultValues {
  std::string name;
  int id = 0;
  std::vector<Variable> vars;
  std::vector<Function> funcs;
  std::vector<State> states;
  std::vector<Replication> replication;
  std::vector<Constant> constants;
  std::vector<Enumeration> enums;
  std::vector<Type> structs;
  Symbol base;
  int super = 0;
  const Class* parent = nullptr;
  std::vector<DefaultObject> objects;
  std::uint32_t flags = 0x12;
  std::string config = "None";
  int within = 0;
  Type withinType;
  bool nativeClass = false;
  std::uint64_t probe = 0;
};
bool number(const Token& t) {
  return !t.quote && !t.text.empty() &&
         ((t.text[0] >= '0' && t.text[0] <= '9') ||
          (t.text[0] == '.' && t.text.size() > 1));
}
Value literal(Lexer& l) {
  if (l.token.quote) {
    Value v;
    v.type = l.token.quote == '"' ? Type::String : Type::Name;
    v.text = l.token.text;
    l.next();
    return v;
  }
  if (l.accept("true")) return {Type::Bool, 1};
  if (l.accept("false")) return {Type::Bool, 0};
  if (l.accept("none")) return {Type::Object, 0};
  bool neg = l.accept("-");
  auto s = l.token.text;
  if (!number(l.token)) l.error("Expected scalar literal");
  bool hex = s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
  bool floatSuffix = !hex && !s.empty() && (s.back() == 'f' || s.back() == 'F');
  if (floatSuffix) s.pop_back();
  if (!hex && (floatSuffix || s.find_first_of(".eE") != s.npos)) {
    std::istringstream in((neg ? "-" : "") + s);
    in.imbue(std::locale::classic());
    float f = 0;
    in >> f;
    if (!in || !in.eof() || !std::isfinite(f))
      l.error("Invalid or overflowing float literal");
    std::int32_t bits;
    std::memcpy(&bits, &f, 4);
    l.next();
    return {Type::Float, bits};
  }
  std::uint64_t n = 0;
  for (std::size_t i = hex ? 2 : 0; i < s.size(); ++i) {
    char c = s[i];
    unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0')
                                          : unsigned((c | 32) - 'a' + 10);
    if (digit >= unsigned(hex ? 16 : 10)) l.error("Invalid integer literal");
    const auto limit =
        neg ? (std::uint64_t(1) << 63) : std::uint64_t(INT64_MAX);
    if (n > (limit - digit) / (hex ? 16 : 10))
      l.error("Integer literal overflow");
    n = n * (hex ? 16 : 10) + digit;
  }
  if (s == "0x" || s == "0X") l.error("Empty hexadecimal literal");
  l.next();
  if (n > (hex && !neg ? 0xffffffffull : neg ? 2147483648ull : 2147483647ull)) {
    Value result{Type::named(Type::Struct, "Core.Object.INT64")};
    result.wide =
        neg ? (n == (std::uint64_t(1) << 63) ? INT64_MIN : -std::int64_t(n))
            : std::int64_t(n);
    return result;
  }
  auto value = std::uint32_t(neg ? -std::int64_t(n) : std::int64_t(n));
  std::int32_t bits;
  std::memcpy(&bits, &value, 4);
  return {Type::Int, bits};
}
std::string sourceText(const Bytes& b) {
  if (b.size() >= 2 &&
      ((b[0] == 0xff && b[1] == 0xfe) || (b[0] == 0xfe && b[1] == 0xff))) {
    bool le = b[0] == 0xff;
    if (b.size() % 2) throw std::runtime_error("Truncated UTF-16 source");
    Bytes encoded;
    index(encoded, -std::int32_t(b.size() / 2));
    for (std::size_t i = 2; i < b.size(); i += 2) {
      encoded.push_back(b[i + (le ? 0 : 1)]);
      encoded.push_back(b[i + (le ? 1 : 0)]);
    }
    u16(encoded, 0);
    Reader reader{encoded};
    return reader.string();
  }
  auto start =
      b.size() >= 3 && b[0] == 0xef && b[1] == 0xbb && b[2] == 0xbf ? 3 : 0;
  return std::string(b.begin() + start, b.end());
}
struct Parser {
  Lexer l;
  Class c;
  Function* f = nullptr;
  int nextId;
  const std::vector<NativeInfo>& operators;
  int loopDepth = 0;
  bool iteratorHead = false;
  bool staticArrayArgument = false;
  int iteratorDepth = 0;
  int switchDepth = 0;
  const TargetModel& target;
  TargetImports& imports;
  const std::map<std::string, std::string>& sourceBases;
  const std::map<std::string, const Class*>* sourceClasses = nullptr;
  const Class* sourceParent = nullptr;
  std::map<Type, Structure>& structures;
  std::map<Type, Enumeration>& enumerations;
  bool defaultsEnabled = false;
  int currentState = 0;
  std::string packageName;
  Type expectedEnum;
  std::function<void(const std::string&)> progress;
  bool cacheConstants = false;
  std::map<std::string, std::optional<Value>> constantCache;
  std::vector<Type> structScope;
  const Variable* parentVariable(const std::string& name) const {
    for (auto p = c.parent; p; p = p->parent)
      for (auto& v : p->vars)
        if (lower(v.name) == lower(name)) return &v;
    return nullptr;
  }
  const Function* parentFunction(const std::string& name) const {
    for (auto p = c.parent; p; p = p->parent)
      for (auto& fn : p->funcs)
        if (!fn.state && lower(fn.name) == lower(name)) return &fn;
    return nullptr;
  }
  const State* sourceState(int id) const {
    for (auto owner = &c; owner; owner = owner->parent)
      for (const auto& state : owner->states)
        if (state.id == id) return &state;
    return nullptr;
  }
  std::optional<Symbol> externalStateFunction(Symbol state,
                                              const std::string& name) const {
    std::set<Symbol> seen;
    while (seen.insert(state).second) {
      const auto& package = target.packages[state.package].data;
      for (std::size_t i = 0; i < package.exports.size(); ++i)
        if (package.exports[i].outer == state.id &&
            package.kind(int(i) + 1) == "Function" &&
            lower(package.name(package.exports[i].name)) == lower(name))
          return Symbol{state.package, int(i) + 1};
      auto base =
          target.resolve({state.package, stateInfo(package, state.id).super});
      if (!base) break;
      state = *base;
    }
    return {};
  }
  std::optional<Function> stateFunction(const std::string& name, int id,
                                        bool skipCurrent = false) {
    auto state = sourceState(id);
    std::set<int> seen;
    while (state && seen.insert(state->id).second) {
      if (!skipCurrent)
        for (const Class* owner = &c; owner; owner = owner->parent)
          for (const auto& function : owner->funcs)
            if (function.state == state->id &&
                lower(function.name) == lower(name))
              return function;
      skipCurrent = false;
      if (state->externalSuper) {
        auto inherited = externalStateFunction(*state->externalSuper, name);
        if (inherited) return importedFunction(*inherited);
      }
      state = sourceState(state->super);
    }
    return {};
  }
  Parser(std::string file, const Bytes& bytes, int first,
         const std::vector<NativeInfo>& ops, const TargetModel& target,
         TargetImports& imports,
         const std::map<std::string, std::string>& sourceBases,
         std::map<Type, Structure>& structures,
         std::map<Type, Enumeration>& enumerations)
      : l{std::move(file), sourceText(bytes)},
        nextId(first),
        operators(ops),
        target(target),
        imports(imports),
        sourceBases(sourceBases),
        structures(structures),
        enumerations(enumerations) {
    l.next();
  }
  std::optional<Type> classType(const std::string& name) const {
    auto local = lower(name);
    auto prefix = lower(packageName) + ".";
    if (local.rfind(prefix, 0) == 0) local.erase(0, prefix.size());
    if (sourceBases.count(local))
      return Type::named(Type::Object, "@local." + local);
    auto symbol = target.findClass(name);
    if (!symbol) return {};
    return Type::named(Type::Object, target.identity(*symbol));
  }
  Type parseType() {
    if (l.accept("enum")) return parseEnum();
    if (l.accept("class")) {
      Type bound = *classType("Core.Object");
      if (l.accept("<")) {
        bound = parseType();
        l.expect(">");
        if (bound.kind != Type::Object || bound.isArray())
          l.error("class<T> requires an object class");
      }
      return Type::named(Type::Class, bound.identity());
    }
    if (auto t = primitiveType(l.token.text); t && !l.token.quote) {
      l.next();
      if (t->isInt64()) loadStruct(*t);
      return *t;
    }
    auto name = l.identifier();
    while (l.accept(".")) name += "." + l.identifier();
    auto t = structType(name);
    if (!t) t = enumType(name);
    if (!t) t = classType(name);
    if (!t) l.error("Unknown type: " + name);
    return *t;
  }
  int objectDistance(Type from, Type to) const {
    if (!objectLike(from) || !objectLike(to)) return -1;
    if (from.kind == Type::Object && from.identity().empty()) return 1;
    if (to.kind == Type::Class) {
      if (from.kind != Type::Class) return -1;
      from = Type::named(Type::Object, from.identity());
      to = Type::named(Type::Object, to.identity());
    } else if (from.kind == Type::Class && to.identity() == "core.object") {
      // UClass is an intrinsic UObject even when Core.u has no Class export.
      return 1;
    } else if (from.kind == Type::Class)
      from = Type::named(Type::Object, "Core.Class");
    std::set<std::string> seen;
    int distance = 0;
    while (from.identity().rfind("@local.", 0) == 0) {
      if (from == to) return distance;
      if (!seen.insert(from.identity()).second)
        throw std::runtime_error("Cyclic source type ancestry");
      auto base = classType(sourceBases.at(from.identity().substr(7)));
      if (!base) return -1;
      from = *base;
      ++distance;
    }
    auto symbol = target.findClass(from.identity());
    if (!symbol) return -1;
    for (auto ancestor : target.ancestry(*symbol)) {
      if (classType(target.identity(ancestor)) == std::optional<Type>{to})
        return distance;
      ++distance;
    }
    return -1;
  }
  static bool objectLike(Type t) {
    return !t.isArray() && (t.kind == Type::Object || t.kind == Type::Class);
  }
  Value valueLiteral() {
    if (auto value = qualifiedEnumValue()) return *value;
    if (!l.token.quote) {
      if (auto value = namedConstant(l.token.text)) {
        l.next();
        return *value;
      }
    }
    if (l.accept("class")) {
      if (l.token.quote != '\'')
        l.error("Class literal requires quoted class name");
      auto cls = classType(l.token.text);
      if (!cls) l.error("Unknown class literal: " + l.token.text);
      l.next();
      return {Type::named(Type::Class, cls->identity()), 0, cls->identity()};
    }
    return constantLiteral();
  }
  std::optional<Value> namedConstant(const std::string& name) {
    if (!cacheConstants) return findNamedConstant(name);
    auto key = lower(name);
    auto found = constantCache.find(key);
    if (found != constantCache.end()) return found->second;
    auto value = findNamedConstant(name);
    constantCache.emplace(key, value);
    return value;
  }
  std::optional<Value> findNamedConstant(const std::string& name) {
    for (const auto* owner = &c; owner; owner = owner->parent) {
      std::optional<Value> match;
      for (const auto& item : owner->enums)
        for (std::size_t i = 0; i < item.names.size(); ++i)
          if (lower(item.names[i]) == lower(name)) {
            if (match)
              l.error("Ambiguous enum value; qualify its enum type: " + name);
            match = Value{item.type, int(i)};
          }
      if (match) return match;
    }
    for (auto owner : target.ancestry(c.base)) {
      const auto& package = target.packages[owner.package].data;
      std::optional<Value> match;
      const auto& byOwner = target.packages[owner.package].enums;
      auto found = byOwner.find(owner.id);
      if (found != byOwner.end())
        for (auto id : found->second) {
          auto names = enumNames(package, id);
          for (std::size_t j = 0; j < names.size(); ++j)
            if (lower(names[j]) == lower(name)) {
              if (match) l.error("Ambiguous imported enum value: " + name);
              match = Value{
                  Type::named(Type::Enum, target.identity({owner.package, id})),
                  int(j)};
            }
        }
      if (match) return match;
    }
    for (const auto* owner = &c; owner; owner = owner->parent)
      for (const auto& item : owner->constants)
        if (lower(item.name) == lower(name)) return item.value;
    auto inherited = target.member(c.base, name);
    if (inherited) {
      const auto& package = target.packages[inherited->package].data;
      if (package.kind(inherited->id) == "Const") {
        return importedConstant(*inherited);
      }
    }
    return {};
  }
  Value importedConstant(Symbol symbol) {
    const auto& package = target.packages[symbol.package].data;
    Reader reader{package.exports[symbol.id - 1].payload};
    if (package.name(reader.index()) != "None")
      l.error("Unsupported const object defaults");
    package.ref(reader.index());
    package.ref(reader.index());
    auto text = reader.string();
    if (reader.pos != reader.data.size()) l.error("Trailing const metadata");
    auto saved = l;
    l = Lexer{package.ref(symbol.id), text};
    l.next();
    auto result = constantLiteral();
    if (!l.end()) l.error("Unsupported external constant expression");
    l = std::move(saved);
    return result;
  }
  std::optional<Type> enumType(const std::string& name) {
    for (auto scope = structScope.rbegin(); scope != structScope.rend();
         ++scope) {
      auto type = Type::named(
          Type::Enum,
          "@enum." +
              lower(c.name + "." + structures.at(*scope).path + "." + name));
      if (enumerations.count(type)) return type;
    }
    for (auto identity :
         {"@enum." + lower(c.name + "." + name), "@enum." + lower(name)}) {
      auto type = Type::named(Type::Enum, identity);
      if (enumerations.count(type)) return type;
    }
    for (const auto* owner = &c; owner; owner = owner->parent)
      for (const auto& item : owner->enums)
        if (lower(item.name) == lower(name) ||
            lower(owner->name + "." + item.name) == lower(name))
          return item.type;
    auto owner = c.base;
    auto member = name;
    auto dot = name.rfind('.');
    if (dot != name.npos) {
      auto cls = target.findClass(name.substr(0, dot));
      if (!cls) return {};
      owner = *cls;
      member = name.substr(dot + 1);
    }
    auto symbol = target.member(owner, member);
    if (symbol &&
        target.packages[symbol->package].data.kind(symbol->id) == "Enum")
      return Type::named(Type::Enum, target.identity(*symbol));
    return {};
  }
  Value constantLiteral() {
    if (l.accept("class")) {
      if (l.token.quote != '\'')
        l.error("Class constant requires a quoted class name");
      auto type = classType(l.token.text);
      if (!type) l.error("Unknown class constant: " + l.token.text);
      l.next();
      return {Type::named(Type::Class, type->identity()), 0, type->identity()};
    }
    if (l.is("vect") || l.is("rot")) {
      bool vector = l.accept("vect");
      if (!vector) l.expect("rot");
      auto type =
          structType(vector ? "Core.Object.Vector" : "Core.Object.Rotator");
      if (!type) l.error("Missing intrinsic literal struct type");
      Value value{*type};
      value.text = loadStruct(*type).name;
      l.expect("(");
      auto names = vector ? std::vector<std::string>{"X", "Y", "Z"}
                          : std::vector<std::string>{"Pitch", "Yaw", "Roll"};
      for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) l.expect(",");
        value.members[names[i]] =
            defaultValue(literal(l), vector ? Type::Float : Type::Int);
      }
      l.expect(")");
      return value;
    }
    if (!l.token.quote && !number(l.token) && !l.is("-") && !l.is("true") &&
        !l.is("false") && !l.is("none")) {
      auto saved = l;
      auto name = l.identifier();
      while (l.accept(".")) name += "." + l.identifier();
      if (l.token.quote == '\'') {
        if (lower(name) == "enum") {
          auto path = l.token.text;
          l.next();
          auto type = Type::named(Type::Object, "Core.Object");
          if (lower(path) == "none") return {type};
          auto prefix = packageName + ".";
          if (!packageName.empty() && lower(path).rfind(lower(prefix), 0) == 0)
            path = path.substr(prefix.size());
          auto enumValue = enumType(path);
          if (!enumValue) l.error("Unresolved enum object literal: " + path);
          auto local = enumerations.find(*enumValue);
          if (local != enumerations.end() && local->second.id)
            return {type, local->second.id};
          auto symbol = structSymbol(enumValue->identity(), "Enum");
          if (!symbol)
            l.error("Unresolved external enum object literal: " + path);
          return {type, imports.symbol(*symbol)};
        }
        auto type = classType(name);
        if (!type) l.error("Unknown object literal type: " + name);
        auto path = lower(l.token.text);
        l.next();
        if (path == "none") return {*type};
        if (sourceClasses) {
          const DefaultObject* local = nullptr;
          std::set<const Class*> owners;
          auto addOwner = [&](const std::string& candidate) {
            auto found =
                sourceClasses->find(candidate.substr(0, candidate.find('.')));
            if (found != sourceClasses->end()) owners.insert(found->second);
          };
          addOwner(path);
          auto prefix = lower(packageName) + ".";
          if (path.rfind(prefix, 0) == 0) addOwner(path.substr(prefix.size()));
          auto current = sourceClasses->find(lower(c.name));
          if (current != sourceClasses->end()) owners.insert(current->second);
          for (const auto* source : owners)
            for (const auto& object : source->objects) {
              auto full = lower(object.path);
              if (path != full && path != lower(packageName) + "." + full &&
                  lower(c.name) + "." + path != full)
                continue;
              if (objectDistance(object.type, *type) < 0)
                l.error("Default subobject literal type mismatch: " + path);
              if (local)
                l.error("Ambiguous default subobject literal: " + path);
              local = &object;
            }
          if (local) return {*type, local->id};
        }
        std::optional<Symbol> match;
        bool sawPath = false;
        for (std::size_t i = 0; i < target.packages.size(); ++i) {
          const auto& package = target.packages[i].data;
          const auto& targetPackage = target.packages[i];
          bool own = targetPackage.referenceOnly &&
                     lower(targetPackage.name) == lower(packageName);
          std::set<int> candidates;
          auto addCandidates = [&](const std::string& key) {
            auto found = targetPackage.exports.find(key);
            if (found != targetPackage.exports.end())
              candidates.insert(found->second.begin(), found->second.end());
          };
          if (own) addCandidates(path);
          auto prefix = lower(targetPackage.name) + ".";
          if (path.rfind(prefix, 0) == 0)
            addCandidates(path.substr(prefix.size()));
          for (int id : candidates) {
            auto j = std::size_t(id - 1);
            sawPath = true;
            auto actual = target.resolve({i, package.exports[j].type});
            if (!actual &&
                target.isIntrinsicClass({i, package.exports[j].type}))
              actual = Symbol{i, package.exports[j].type};
            auto actualType = actual ? classType(target.identity(*actual)) :
                                       std::optional<Type>{};
            if (!actualType || objectDistance(*actualType, *type) < 0)
              continue;
            if (match) l.error("Ambiguous typed object literal: " + path);
            match = Symbol{i, int(j) + 1};
          }
        }
        if (match) return {*type, imports.symbol(*match)};
        for (std::size_t i = 0; i < target.packages.size(); ++i) {
          const auto& reference = target.packages[i];
          if (!reference.referenceOnly ||
              lower(reference.name) != lower(packageName))
            continue;
          const auto& package = reference.data;
          for (std::size_t j = 0; j < package.imports.size(); ++j) {
            int id = -int(j) - 1;
            if (lower(package.ref(id)) != path) continue;
            sawPath = true;
            const auto& entry = package.imports[j];
            auto actualType = classType(package.name(entry.package) + "." +
                                        package.name(entry.type));
            if (!actualType || objectDistance(*actualType, *type) < 0) continue;
            return {*type, imports.referenceObject({i, id})};
          }
        }
        l.error((sawPath ? "Object literal type mismatch: "
                         : "Unresolved object literal: ") +
                path);
      }
      l = std::move(saved);
    }
    auto result = literal(l);
    if (result.type.isInt64()) loadStruct(result.type);
    return result;
  }
  std::optional<Symbol> structSymbol(const std::string& identity,
                                     const std::string& kind = "Struct") const {
    auto key = lower(identity);
    auto dot = key.find('.');
    if (dot == key.npos) return {};
    for (std::size_t i = 0; i < target.packages.size(); ++i) {
      const auto& package = target.packages[i];
      if (lower(package.name) != key.substr(0, dot)) continue;
      auto found = package.exports.find(key.substr(dot + 1));
      if (found == package.exports.end()) return {};
      std::optional<Symbol> match;
      for (auto id : found->second)
        if (package.data.kind(id) == kind) {
          if (match) l.error("Ambiguous " + kind + " identity: " + identity);
          match = Symbol{i, id};
        }
      return match;
    }
    return {};
  }
  Structure& loadStruct(Type type) {
    auto found = structures.find(type);
    if (found != structures.end()) {
      if (!found->second.complete)
        l.error("Recursive or incomplete struct: " + type.identity());
      return found->second;
    }
    auto symbol = structSymbol(type.identity());
    if (!symbol) l.error("Unknown struct: " + type.identity());
    const auto& package = target.packages[symbol->package].data;
    auto info = structInfo(package, symbol->id);
    Structure result;
    result.type = type;
    result.name = package.name(package.exports[symbol->id - 1].name);
    result.id = imports.symbol(*symbol);
    structures.emplace(type, result);
    if (info.super) {
      auto base = target.resolve({symbol->package, info.super});
      if (!base) l.error("Unresolved struct superclass");
      result.base = Type::named(Type::Struct, target.identity(*base));
      loadStruct(result.base);
    }
    for (auto& field : info.fields) {
      auto t = importedType(package, field);
      if (t == Type::Void) l.error("Unsupported struct field type");
      Variable value{package.name(package.exports[field.id - 1].name), t,
                     imports.symbol({symbol->package, field.id}), field.flags};
      value.access = importedAccess({symbol->package, field.id});
      value.owner = memberOwner({symbol->package, field.id});
      result.fields.push_back(value);
    }
    result.complete = true;
    structures[type] = std::move(result);
    return structures.at(type);
  }
  std::optional<Type> structType(const std::string& name) {
    auto key = lower(name);
    if (lower(packageName) == "core") {
      for (auto identity : {key, "core." + key, "core.object." + key}) {
        auto intrinsic = Type::named(Type::Struct, identity);
        if (structures.count(intrinsic)) return intrinsic;
      }
    }
    auto prefix = lower(packageName) + ".";
    if (key.rfind(prefix, 0) == 0) key.erase(0, prefix.size());
    auto localType =
        Type::named(Type::Struct, "@struct." + lower(c.name) + "." + key);
    if (structures.count(localType)) return localType;
    if (key.find('.') != key.npos) {
      auto qualified = Type::named(Type::Struct, "@struct." + key);
      if (structures.count(qualified)) return qualified;
    }
    for (auto scope = structScope.rbegin(); scope != structScope.rend();
         ++scope) {
      auto& owner = structures.at(*scope);
      auto nestedType = Type::named(
          Type::Struct,
          "@struct." + lower(c.name + "." + owner.path + "." + name));
      if (structures.count(nestedType)) return nestedType;
      for (auto nested : owner.nested) {
        auto& item = structures.at(nested);
        if (lower(item.name) == key) return nested;
      }
    }
    for (const auto* owner = &c; owner; owner = owner->parent)
      for (auto type : owner->structs) {
        const auto& item = structures.at(type);
        if (lower(item.name) == key || lower(item.path) == key ||
            lower(owner->name + "." + item.path) == key)
          return type;
      }
    if (sourceClasses)
      for (auto& owner : *sourceClasses)
        for (auto type : owner.second->structs)
          if (lower(owner.first + "." + structures.at(type).path) == key)
            return type;
    auto symbol = structSymbol(name);
    if (!symbol) {
      auto dot = name.find('.');
      if (dot != name.npos) {
        auto owner = target.findClass(name.substr(0, dot));
        if (owner)
          for (auto base : target.ancestry(*owner)) {
            symbol = structSymbol(target.identity(base) + name.substr(dot));
            if (symbol) break;
          }
      }
    }
    if (!symbol) {
      for (auto owner : target.ancestry(c.base)) {
        symbol = structSymbol(target.identity(owner) + "." + name);
        if (symbol) break;
      }
    }
    if (!symbol) {
      auto member = target.member(c.base, name);
      if (member &&
          target.packages[member->package].data.kind(member->id) == "Struct")
        symbol = member;
    }
    if (!symbol) return {};
    auto type = Type::named(Type::Struct, target.identity(*symbol));
    loadStruct(type);
    return type;
  }
  const Variable* structField(Type type, const std::string& name) {
    std::set<Type> seen;
    while (type != Type::Void) {
      if (!seen.insert(type).second) l.error("Cyclic struct inheritance");
      auto& item = loadStruct(type);
      for (auto& field : item.fields)
        if (lower(field.name) == lower(name)) return &field;
      type = item.base;
    }
    return nullptr;
  }
  void parseStruct() {
    Structure item;
    item.name = l.identifier();
    item.outer =
        structScope.empty() ? c.id : structures.at(structScope.back()).id;
    item.path = structScope.empty()
                    ? item.name
                    : structures.at(structScope.back()).path + "." + item.name;
    item.type =
        Type::named(Type::Struct, "@struct." + lower(c.name + "." + item.path));
    if (lower(packageName) == "core" && lower(c.name) == "object")
      item.type = Type::named(Type::Struct, "core.object." + lower(item.path));
    if (std::find(c.structs.begin(), c.structs.end(), item.type) !=
        c.structs.end())
      l.error("Duplicate struct: " + item.path);
    if (structScope.empty()) unique(item.name);
    item.id = nextId++;
    if (l.accept("extends")) {
      auto name = l.identifier();
      while (l.accept(".")) name += "." + l.identifier();
      auto base = structType(name);
      if (!base) l.error("Unknown struct superclass: " + name);
      if (base->identity().rfind("@struct.", 0) != 0) loadStruct(*base);
      item.base = *base;
    }
    if (!structScope.empty())
      structures.at(structScope.back()).nested.push_back(item.type);
    c.structs.push_back(item.type);
    structures[item.type] = item;
    structScope.push_back(item.type);
    l.expect("{");
    while (!l.accept("}")) {
      if (l.accept("struct")) {
        parseStruct();
        continue;
      }
      l.expect("var");
      std::string category;
      std::uint32_t flags = 0;
      int access = 0;
      bool sawAccess = false;
      if (l.accept("(")) {
        flags |= 1;
        category = l.is(")") ? item.name : l.identifier();
        l.expect(")");
      }
      for (;;) {
        if (l.is("private") || l.is("protected") || l.is("public")) {
          if (sawAccess) l.error("Duplicate struct field access modifier");
          sawAccess = true;
          access = l.is("private") ? 1 : l.is("protected") ? 2 : 0;
          l.next();
          continue;
        }
        if (l.accept("const"))
          flags |= 2;
        else if (l.accept("transient"))
          flags |= 0x2000;
        else if (l.accept("native"))
          flags |= 0x1000;
        else if (l.accept("localized"))
          flags |= 0x8000;
        else if (l.accept("config"))
          flags |= 0x4000;
        else if (l.accept("globalconfig"))
          flags |= 0x40000;
        else if (l.accept("editconst"))
          flags |= 0x20000;
        else if (l.accept("input"))
          flags |= 4;
        else if (l.accept("export"))
          flags |= 8;
        else if (l.accept("travel"))
          flags |= 0x10000;
        else if (l.accept("editconstarray"))
          flags |= 0x40;
        else if (l.accept("editinline"))
          flags |= 0x4000000;
        else if (l.accept("editinlineuse"))
          flags |= 0x14000000;
        else if (l.accept("editinlinenotify"))
          flags |= 0x44000000;
        else if (l.accept("edfindable"))
          flags |= 0x8000000;
        else if (l.accept("deprecated"))
          flags |= 0x20000000;
        else
          break;
      }
      auto type = variableType();
      if (type.kind == Type::Struct &&
          type.identity().rfind("@struct.", 0) != 0)
        loadStruct(type.element());
      do {
        auto name = l.identifier();
        for (auto& old : structures.at(item.type).fields)
          if (lower(old.name) == lower(name))
            l.error("Duplicate struct field: " + name);
        if (item.base != Type::Void && structures.at(item.base).complete &&
            structField(item.base, name))
          l.error("Inherited struct field redeclaration: " + name);
        structures.at(item.type).fields.push_back(
            declaration(name, type, flags, category, access));
      } while (l.accept(","));
      l.expect(";");
    }
    l.expect(";");
    structScope.pop_back();
    structures.at(item.type).complete = true;
  }
  std::optional<Value> qualifiedEnumValue() {
    if (l.token.quote) return {};
    auto saved = l;
    auto name = l.token.text;
    auto type = enumType(name);
    l.next();
    while (!type && l.accept(".")) {
      name += "." + l.identifier();
      type = enumType(name);
    }
    if (!type || !l.accept(".")) {
      l = std::move(saved);
      return {};
    }
    auto value = l.identifier();
    std::vector<std::string> names;
    if (enumerations.count(*type)) names = enumerations.at(*type).names;
    for (const auto* owner = &c; owner; owner = owner->parent)
      for (const auto& item : owner->enums)
        if (item.type == *type) names = item.names;
    if (names.empty()) {
      auto dot = type->identity().rfind('.');
      auto owner = target.findClass(type->identity().substr(0, dot));
      if (!owner) l.error("Unresolved enum owner");
      auto symbol = target.member(*owner, type->identity().substr(dot + 1));
      if (!symbol) l.error("Unresolved enum member");
      names = enumNames(target.packages[symbol->package].data, symbol->id);
    }
    if (lower(value) == "enumcount") {
      if (names.size() > 255) l.error("EnumCount exceeds byte range");
      return Value{Type::Byte, int(names.size())};
    }
    for (std::size_t i = 0; i < names.size(); ++i)
      if (lower(names[i]) == lower(value)) return Value{*type, int(i)};
    l.error("Unknown enum value: " + value);
  }
  Type parseEnum() {
    Enumeration item;
    item.name = l.identifier();
    item.path = structScope.empty()
                    ? item.name
                    : structures.at(structScope.back()).path + "." + item.name;
    item.outer =
        structScope.empty() ? c.id : structures.at(structScope.back()).id;
    if (structScope.empty()) unique(item.name);
    item.type =
        Type::named(Type::Enum, "@enum." + lower(c.name + "." + item.path));
    for (const auto& old : c.enums)
      if (old.type == item.type) l.error("Duplicate enum declaration");
    item.id = nextId++;
    l.expect("{");
    do {
      auto name = l.identifier();
      for (auto& old : item.names)
        if (lower(old) == lower(name)) l.error("Duplicate enum value: " + name);
      if (item.names.size() == 256) l.error("Enum exceeds 256 values");
      item.names.push_back(name);
    } while (l.accept(",") && !l.is("}"));
    l.expect("}");
    auto type = item.type;
    enumerations[type] = item;
    c.enums.push_back(std::move(item));
    return type;
  }
  void unique(const std::string& s) {
    auto key = lower(s);
    if (key == "none" || key == "returnvalue")
      l.error("Reserved declaration name: " + s);
    for (auto& v : c.vars)
      if (lower(v.name) == key) l.error("Duplicate member: " + s);
    for (auto& x : c.funcs)
      if (x.state == currentState && lower(x.name) == key)
        l.error("Duplicate member: " + s);
    for (auto& x : c.constants)
      if (lower(x.name) == key) l.error("Duplicate member: " + s);
    for (auto type : c.structs)
      if (structures.at(type).outer == c.id &&
          lower(structures.at(type).name) == key)
        l.error("Duplicate member: " + s);
    for (auto& x : c.enums) {
      if (lower(x.name) == key) l.error("Duplicate member: " + s);
    }
  }
  void localUnique(const std::string& s) {
    if (lower(s) == "returnvalue") l.error("Reserved return parameter name");
    for (auto* vs : {&f->params, &f->locals})
      for (auto& v : *vs)
        if (lower(v.name) == lower(s))
          l.error("Duplicate parameter/local: " + s);
  }
  int dimension(Type t) {
    if (!l.accept("[")) return 1;
    auto v = valueLiteral();
    if ((v.type != Type::Int && v.type != Type::Byte) || v.value < 2 ||
        v.value > 65535)
      l.error("Static array dimension must be in 2..65535");
    if (t == Type::Bool) l.error("Bool arrays are not allowed");
    l.expect("]");
    return v.value;
  }
  Type variableType() {
    bool dynamic = l.accept("array");
    if (dynamic) l.expect("<");
    auto t = parseType();
    if (dynamic) {
      l.expect(">");
      t = Type::array(t);
    }
    return t;
  }
  Variable declaration(const std::string& name, Type t, std::uint32_t flags = 0,
                       const std::string& category = {}, int access = 0) {
    auto size = dimension(t);
    if (t.isDynamicArray() && size != 1)
      l.error("Static arrays of dynamic arrays are not supported");
    if (size != 1) t = Type::array(t, size);
    Variable v{name, t, nextId++, flags, category};
    v.access = access;
    v.owner = *classType(c.name);
    if (t.isDynamicArray()) v.inner = nextId++;
    return v;
  }
  void checkAccess(int access, Type owner) {
    if (!access || owner == Type::Void) return;
    auto current = *classType(c.name);
    if ((access == 1 && current != owner) ||
        (access == 2 && objectDistance(current, owner) < 0))
      l.error("Member access denied: " + owner.identity());
  }
  int importedAccess(Symbol symbol) const {
    auto flags =
        target.packages[symbol.package].data.exports[symbol.id - 1].flags;
    return flags & 0x80 ? 1 : flags & 4 ? 0 : 2;
  }
  Type memberOwner(Symbol symbol) const {
    std::set<Symbol> seen;
    while (seen.insert(symbol).second) {
      const auto& package = target.packages[symbol.package].data;
      if (package.kind(symbol.id) == "Class")
        return Type::named(Type::Object, target.identity(symbol));
      auto owner = target.resolve(
          {symbol.package, package.exports[symbol.id - 1].outer});
      if (!owner) break;
      symbol = *owner;
    }
    l.error("Unresolved member owner");
  }
  Expr element(Expr e) {
    bool dynamic = e.type.isDynamicArray();
    int size = e.type.dimension();
    if (dynamic && l.accept(".")) {
      auto member = lower(l.identifier());
      if (member == "sort") {
        const auto& core =
            target.packages[target.findClass("Core.Object")->package].data;
        if (core.version != 133 || core.licensee != 40)
          l.error("Array.Sort requires client profile 133/40");
        if (!e.writable) l.error("Array.Sort requires a writable array");
        auto elementType = e.type.element();
        l.expect("(");
        auto comparator = expression();
        l.expect(")");
        if (comparator.type.kind != Type::Delegate)
          l.error("Array.Sort requires a delegate comparator");
        auto signature = delegateFunction(comparator.type);
        if (signature.type != Type::Int || signature.params.size() != 2)
          l.error(
              "Array.Sort comparator must return int and take two elements");
        for (const auto& param : signature.params)
          if (param.type != elementType || (param.flags & 0x110))
            l.error("Array.Sort comparator parameter mismatch");
        Expr result;
        result.type = Type::Void;
        result.writable = false;
        result.arrayOp = 0x47;
        result.args = {std::move(e), std::move(comparator)};
        return result;
      }
      if (member == "length") {
        Expr result;
        result.type = Type::Int;
        result.ref = e.ref;
        result.writable = e.writable;
        result.arrayOp = 0x37;
        result.args.push_back(std::move(e));
        return result;
      }
      if (member != "insert" && member != "remove")
        l.error("Unknown dynamic array member: " + member);
      if (!e.writable) l.error("Array operation requires writable variable");
      Expr result;
      result.type = Type::Void;
      result.arrayOp = member == "insert" ? 0x40 : 0x41;
      result.writable = false;
      result.args.push_back(std::move(e));
      l.expect("(");
      result.args.push_back(convert(expression(), Type::Int));
      l.expect(",");
      result.args.push_back(convert(expression(), Type::Int));
      l.expect(")");
      return result;
    }
    if (size == 1 && !dynamic) {
      if (l.is("[")) l.error("Indexing requires an array");
      return e;
    }
    if (dynamic && !l.is("[")) return e;
    l.expect("[");
    auto subscript = convert(expression(), Type::Int);
    l.expect("]");
    Expr result;
    result.type = e.type.element();
    result.ref = e.ref;
    result.writable = e.writable;
    result.indexed = true;
    result.arrayOp = dynamic ? 0x10 : 0;
    result.args.push_back(std::move(subscript));
    result.args.push_back(std::move(e));
    return result;
  }
  Type delegateType(const Function& function) const {
    auto owner = function.owner.identity();
    return Type::named(
        Type::Delegate,
        owner.rfind("@local.", 0) == 0
            ? "@delegate." + owner.substr(7) + "." + lower(function.name)
            : owner + "." + lower(function.name));
  }
  Function delegateFunction(Type type) {
    auto identity = type.identity();
    auto dot = identity.rfind('.');
    auto ownerName = identity.substr(0, dot), name = identity.substr(dot + 1);
    if (ownerName.rfind("@delegate.", 0) == 0) {
      ownerName = ownerName.substr(10);
      const Class* owner = lower(c.name) == ownerName ? &c : nullptr;
      if (!owner && sourceClasses) {
        auto found = sourceClasses->find(ownerName);
        if (found != sourceClasses->end()) owner = found->second;
      }
      if (owner) {
        for (auto current = owner; current; current = current->parent)
          for (const auto& fn : current->funcs)
            if (!fn.state && lower(fn.name) == name) return fn;
        auto external = target.member(owner->base, name);
        if (external) return importedFunction(*external);
      }
    }
    auto owner = target.findClass(ownerName);
    if (owner) {
      auto fn = target.member(*owner, name);
      if (fn) return importedFunction(*fn);
    }
    l.error("Unresolved delegate signature: " + identity);
  }
  Expr functionValue(const Function& function) {
    auto visibility = function.flags | function.inheritedFlags;
    checkAccess(visibility & 0x40000   ? 1
                : visibility & 0x80000 ? 2
                                       : 0,
                function.owner);
    if (f->isStatic) l.error("Delegate binding in static function");
    Expr value;
    value.type = delegateType(function);
    if (function.delegateStorage) {
      value.ref = function.delegateStorage;
      value.instance = true;
    } else {
      value.objectOp = 0x44;
      value.text = function.name;
      value.writable = false;
    }
    return value;
  }
  Type contextPropertyType(Type type, const std::string& name, Type owner,
                           Type receiver) const {
    if (lower(name) == "class" && type.kind == Type::Class && !type.isArray() &&
        classType("Core.Object") == std::optional<Type>{owner})
      return Type::named(Type::Class, receiver.identity());
    return type;
  }
  Expr variable(bool instanceOnly = false, bool defaults = false) {
    auto start = l;
    auto s = l.identifier();
    if (!instanceOnly)
      for (auto* vs : {&f->locals, &f->params})
        for (auto& v : *vs)
          if (lower(v.name) == lower(s)) return {v.type, v.id, false, 0};
    for (auto& v : c.vars)
      if (lower(v.name) == lower(s)) {
        if (f->isStatic && !defaults)
          l.error("Instance member in static function: " + s);
        Expr e{v.type, v.id, true, 0};
        e.type = contextPropertyType(v.type, s, v.owner, *classType(c.name));
        e.defaultRef = defaults;
        e.writable = !(v.flags & 2);
        return e;
      }
    if (auto v = parentVariable(s)) {
      checkAccess(v->access, v->owner);
      if (f->isStatic && !defaults)
        l.error("Instance member in static function: " + s);
      Expr e{v->type, v->id, true, 0};
      e.type = contextPropertyType(v->type, s, v->owner, *classType(c.name));
      e.defaultRef = defaults;
      e.writable = !(v->flags & 2);
      return e;
    }
    for (const Class* owner = &c; owner; owner = owner->parent)
      for (const auto& fn : owner->funcs)
        if (!fn.state && lower(fn.name) == lower(s)) return functionValue(fn);
    auto inherited = target.member(c.base, s);
    if (!inherited) {
      if (!defaults && c.withinType != Type::Void &&
          c.withinType.identity() != "core.object") {
        if (f->isStatic) l.error("Outer member in static function");
        return outerMember(std::move(start));
      }
      l.error("Unknown variable: " + s);
    }
    auto& p = target.packages[inherited->package].data;
    auto kind = p.kind(inherited->id);
    if (kind == "Function") return functionValue(importedFunction(*inherited));
    if (f->isStatic && !defaults)
      l.error("Instance member in static function: " + s);
    auto prop = propertyInfo(p, inherited->id);
    checkAccess(importedAccess(*inherited), memberOwner(*inherited));
    auto t = importedType(p, prop);
    t = contextPropertyType(t, s, memberOwner(*inherited), *classType(c.name));
    if (lower(s) == "outer" && c.withinType != Type::Void) t = c.withinType;
    if (t == Type::Void)
      l.error("Unsupported inherited variable type: " + kind);
    Expr e{t, imports.symbol(*inherited), true, 0};
    e.defaultRef = defaults;
    e.writable = !(prop.flags & 2);
    return e;
  }
  Expr outerExpression() {
    auto root = target.findClass("Core.Object");
    auto field = target.member(*root, "Outer");
    if (!field) l.error("Target has no Object.Outer property");
    Expr e;
    e.type = c.withinType;
    e.ref = imports.symbol(*field);
    e.instance = true;
    e.writable = false;
    return e;
  }
  Type withinOf(Type type) const {
    if (type.identity() == "@local." + lower(c.name)) return c.withinType;
    if (type.identity().rfind("@local.", 0) == 0 && sourceClasses) {
      auto found = sourceClasses->find(type.identity().substr(7));
      if (found != sourceClasses->end()) return found->second->withinType;
    }
    auto owner = target.findClass(type.identity());
    if (!owner) return Type::Void;
    auto info = classInfo(target.packages[owner->package].data, owner->id);
    auto within = info.within ? target.resolve({owner->package, info.within})
                              : std::optional<Symbol>{};
    return within ? Type::named(Type::Object, target.identity(*within))
                  : Type(Type::Void);
  }
  Expr outerMember(Lexer start) {
    auto name = start.token.text;
    auto receiver = outerExpression();
    std::set<Type> seen;
    while (receiver.type != Type::Void && seen.insert(receiver.type).second) {
      bool found = false;
      std::optional<Symbol> external;
      if (receiver.type.identity().rfind("@local.", 0) == 0 && sourceClasses) {
        auto owner = sourceClasses->at(receiver.type.identity().substr(7));
        for (auto current = owner; current; current = current->parent) {
          for (auto& field : current->vars)
            found |= lower(field.name) == lower(name);
          for (auto& fn : current->funcs)
            found |= lower(fn.name) == lower(name);
        }
        external = target.member(owner->base, name);
      } else {
        auto owner = target.findClass(receiver.type.identity());
        if (owner) external = target.member(*owner, name);
      }
      if (found || external) {
        l = std::move(start);
        auto result = member(std::move(receiver));
        result.implicitOuterCall = !result.callName.empty();
        return result;
      }
      auto within = withinOf(receiver.type);
      if (within == Type::Void || within == receiver.type) break;
      auto next = outerExpression();
      next.type = within;
      next.context = std::make_shared<Expr>(std::move(receiver));
      receiver = std::move(next);
    }
    l.error("Unknown outer member: " + name);
  }
  Type propertyType(const PropertyInfo& p,
                    const Package* package = nullptr) const {
    if (p.kind == "ObjectProperty" || p.kind == "ClassProperty" ||
        p.kind == "StructProperty" ||
        (p.kind == "ByteProperty" && p.reference)) {
      if (!package) {
        auto root = target.findClass("Core.Object");
        package = &target.packages[root->package].data;
      }
      for (std::size_t i = 0; i < target.packages.size(); ++i)
        if (&target.packages[i].data == package) {
          auto symbol = target.resolve(
              {i, p.kind == "ClassProperty" ? p.metaclass : p.reference});
          if (!symbol &&
              (p.kind == "ObjectProperty" || p.kind == "ClassProperty")) {
            Symbol reference{
                i, p.kind == "ClassProperty" ? p.metaclass : p.reference};
            symbol = target.findClass(target.identity(reference));
          }
          if (!symbol)
            throw std::runtime_error("Unresolved object property class");
          return Type::named(p.kind == "ClassProperty"    ? Type::Class
                             : p.kind == "ByteProperty"   ? Type::Enum
                             : p.kind == "StructProperty" ? Type::Struct
                                                          : Type::Object,
                             target.identity(*symbol));
        }
      throw std::runtime_error("Object property package outside target graph");
    }
    for (auto t : {Type::Int, Type::Bool, Type::Byte, Type::Float, Type::String,
                   Type::Name, Type::Pointer})
      if (p.kind == propertyKind(t)) {
        if (t == Type::Byte && p.reference) return Type::Void;
        return t;
      }
    return Type::Void;
  }
  Type importedType(const Package& p, const PropertyInfo& prop) {
    if (prop.kind == "DelegateProperty") {
      for (std::size_t i = 0; i < target.packages.size(); ++i)
        if (&target.packages[i].data == &p) {
          auto signature = target.resolve({i, prop.reference});
          if (!signature) l.error("Unresolved delegate property signature");
          auto type = Type::named(Type::Delegate, target.identity(*signature));
          return prop.dimension == 1 ? type : Type::array(type, prop.dimension);
        }
    }
    bool dynamic = prop.kind == "ArrayProperty";
    auto t = propertyType(dynamic ? propertyInfo(p, prop.reference) : prop, &p);
    if (t == Type::Void) return t;
    if (t.kind == Type::Struct) loadStruct(t);
    if (dynamic) t = Type::array(t);
    if (prop.dimension != 1) t = Type::array(t, prop.dimension);
    return t;
  }
  static bool numeric(Type t) {
    return t == Type::Int || t == Type::Byte || t == Type::Float;
  }
  static bool literalExpr(const Expr& e) {
    return !e.ref && !e.call && !e.native && e.cast < 0;
  }
  int conversionCost(const Expr& e, Type to, std::uint32_t flags = 0) const {
    if (e.type.isArray()) return -1;
    if (e.type == to) return 0;
    if (flags & 0x100) return -1;
    if (e.type.isInt64() || to.isInt64()) {
      if (to.isInt64() && (e.type == Type::Byte || e.type == Type::Int))
        return 1;
      if (e.type.isInt64() && to == Type::Float) return 104;
      if ((flags & 0x800) && to == Type::String) return 8;
      return -1;
    }
    if (e.type.kind == Type::Struct && to.kind == Type::Struct &&
        !to.isArray()) {
      auto from = e.type;
      int distance = 0;
      std::set<Type> seen;
      while (from != Type::Void && seen.insert(from).second) {
        if (from == to) return distance;
        from = structures.at(from).base;
        ++distance;
      }
      return -1;
    }
    if (e.type.kind == Type::Enum && !to.isArray()) {
      if (to == Type::Byte) return 1;
      if (to == Type::Int) return 2;
      if (to == Type::Float) return 3;
    }
    if (objectLike(e.type) && objectLike(to)) return objectDistance(e.type, to);
    if (numeric(e.type) && numeric(to)) {
      if (to == Type::Byte && e.type == Type::Int && literalExpr(e))
        return e.value >= 0 && e.value <= 255 ? 1 : -1;
      if (e.type == Type::Byte) return to == Type::Int ? 1 : 2;
      if (e.type == Type::Int) return to == Type::Float ? 1 : 2;
      return to == Type::Int ? 2 : 3;
    }
    if ((flags & 0x800) && to == Type::String && e.type != Type::Void) return 8;
    return -1;
  }
  Expr convert(Expr e, Type to, bool explicitCast = false,
               std::uint32_t flags = 0) {
    if (e.type.isArray() || to.isArray()) {
      if (!explicitCast && !(flags & 0x100) && e.type.isDynamicArray() &&
          to.isDynamicArray() && e.type.element().kind == Type::Object &&
          to.element().kind == Type::Object &&
          objectDistance(e.type.element(), to.element()) >= 0) {
        e.type = to;
        return e;
      }
      if (e.type != to || explicitCast) l.error("Array type mismatch");
      return e;
    }
    if (e.type == to) return e;
    if (to.kind == Type::Delegate) {
      if (explicitCast) l.error("Explicit delegate casts are unsupported");
      if (e.type.kind == Type::Object && e.type.identity().empty() &&
          !e.value && !e.ref) {
        e.type = to;
        e.objectOp = 0x44;
        e.text = "None";
        return e;
      }
      if (e.type.kind != Type::Delegate)
        l.error("Delegate assignment requires a function or delegate");
      auto from = delegateFunction(e.type), dest = delegateFunction(to);
      if (from.type != dest.type || from.params.size() != dest.params.size())
        l.error("Delegate signature mismatch");
      for (std::size_t i = 0; i < from.params.size(); ++i)
        if (from.params[i].type != dest.params[i].type ||
            ((from.params[i].flags ^ dest.params[i].flags) & 0x100))
          l.error("Delegate parameter mismatch");
      e.type = to;
      return e;
    }
    if (!explicitCast && conversionCost(e, to, flags) < 0)
      l.error("Type mismatch in conversion");
    if ((e.type.kind == Type::Struct || to.kind == Type::Struct) &&
        !e.type.isInt64() && !to.isInt64()) {
      if (e.type.kind == Type::Struct && to.kind == Type::Struct &&
          conversionCost(e, to, flags) >= 0) {
        e.type = to;
        return e;
      }
      if ((explicitCast || ((flags & 0x800) && to == Type::String)) &&
          e.type != to) {
        int token = -1;
        try {
          token = primitiveCast(e.type, to);
        } catch (const std::exception&) {
          l.error("Unsupported struct conversion");
        }
        Expr cast;
        cast.type = to;
        cast.cast = token;
        cast.args.push_back(std::move(e));
        return cast;
      }
      if (conversionCost(e, to, flags) < 0) l.error("Struct type mismatch");
      e.type = to;
      return e;
    }
    if (to.kind == Type::Enum) {
      if (!explicitCast || (e.type != Type::Byte && e.type != Type::Int &&
                            e.type.kind != Type::Enum))
        l.error("Enum conversion requires an explicit byte/int cast");
      if (e.type == Type::Int && literalExpr(e) && e.value >= 0 &&
          e.value <= 255)
        e.type = Type::Byte;
      else
        e = convert(std::move(e), Type::Byte, true);
      e.type = to;
      return e;
    }
    if (e.type.kind == Type::Enum) {
      e.type = Type::Byte;
      return convert(std::move(e), to, explicitCast, flags);
    }
    if (objectLike(e.type) && objectLike(to)) {
      if (objectDistance(e.type, to) >= 0) {
        e.type = to;
        return e;
      }
      if (!explicitCast ||
          (to.kind != Type::Class && objectDistance(to, e.type) < 0))
        l.error("Unrelated object types in cast");
      Expr result;
      result.type = to;
      result.objectOp = to.kind == Type::Class ? 0x13 : 0x2e;
      result.args.push_back(std::move(e));
      return result;
    }
    if (!explicitCast && to == Type::Byte && e.type == Type::Int &&
        literalExpr(e)) {
      e.type = Type::Byte;
      return e;
    }
    Expr cast;
    cast.type = to;
    try {
      cast.cast = primitiveCast(e.type, to);
    } catch (const std::exception&) {
      l.error("Unsupported primitive conversion");
    }
    cast.args.push_back(std::move(e));
    return cast;
  }
  Value defaultValue(Value v, Type to) {
    if (to.isInt64()) {
      loadStruct(to);
      if (v.type == Type::Int || v.type == Type::Byte)
        v.wide = v.value;
      else if (!v.type.isInt64())
        l.error("INT64 default requires an integer literal");
      v.type = to;
      v.text = loadStruct(to).name;
      if (v.members.empty()) {
        auto bits = std::uint64_t(v.wide);
        for (auto name : {"Left", "Right"}) {
          auto field = structField(to, name);
          if (!field || field->type != Type::Int)
            l.error("Unsupported INT64 storage layout");
          auto word =
              std::uint32_t(std::string(name) == "Left" ? bits >> 32 : bits);
          std::int32_t signedWord;
          std::memcpy(&signedWord, &word, 4);
          v.members[field->name] = {Type::Int, signedWord};
        }
      }
      return v;
    }
    if (objectLike(v.type) && objectLike(to) &&
        objectDistance(v.type, to) >= 0) {
      v.type = to;
      return v;
    }
    if (v.type == to) return v;
    if (!numeric(v.type) || !numeric(to))
      l.error("Default property type mismatch");
    double n = v.value;
    if (v.type == Type::Float) {
      float value;
      std::memcpy(&value, &v.value, 4);
      n = value;
    }
    if (to == Type::Byte) {
      if (n < 0 || n > 255) l.error("Byte default outside 0..255");
      v.value = std::int32_t(n);
    } else if (to == Type::Int) {
      if (n < double(INT32_MIN) || n > double(INT32_MAX))
        l.error("Int default overflow");
      v.value = std::int32_t(n);
    } else {
      float value = static_cast<float>(n);
      std::memcpy(&v.value, &value, 4);
    }
    v.type = to;
    return v;
  }
  Value compoundDefault(Type type, int depth = 0) {
    if (depth > 64) l.error("Default nesting limit");
    if (l.accept("__floatbits")) {
      if (type != Type::Float) l.error("__FloatBits requires a float default");
      l.expect("(");
      if (l.token.quote != '"' || l.token.text.size() != 8)
        l.error("__FloatBits requires eight quoted hexadecimal digits");
      std::uint32_t bits = 0;
      for (char c : lower(l.token.text)) {
        int digit = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                           : -1;
        if (digit < 0) l.error("Invalid __FloatBits hexadecimal digit");
        bits = (bits << 4) | std::uint32_t(digit);
      }
      l.next();
      l.expect(")");
      Value value{Type::Float};
      std::memcpy(&value.value, &bits, 4);
      return value;
    }
    if (type.isInt64() && !l.is("(")) return defaultValue(valueLiteral(), type);
    if (type.isDynamicArray()) {
      Value value{type};
      l.expect("(");
      if (!l.is(")")) do {
          value.elements.push_back(compoundDefault(type.element(), depth + 1));
        } while (l.accept(","));
      l.expect(")");
      return value;
    }
    if (type.kind == Type::Enum && !l.token.quote) {
      std::vector<std::string> names;
      auto local = enumerations.find(type);
      if (local != enumerations.end()) names = local->second.names;
      else if (auto symbol = structSymbol(type.identity(), "Enum"))
        names = enumNames(target.packages[symbol->package].data, symbol->id);
      for (std::size_t i = 0; i < names.size(); ++i)
        if (lower(names[i]) == lower(l.token.text)) {
          l.next();
          return Value{type, int(i)};
        }
    }
    if (type.kind != Type::Struct) return defaultValue(valueLiteral(), type);
    Value value{type};
    value.text = loadStruct(type).name;
    l.expect("(");
    if (!l.is(")")) do {
        auto name = l.identifier();
        auto field = structField(type, name);
        if (!field) l.error("Unknown struct default field: " + name);
        int subscript = -1;
        if (l.is("[") || l.is("(")) {
          bool square = l.accept("[");
          if (!square) l.expect("(");
          auto index = valueLiteral();
          if (index.type != Type::Int || index.value < 0)
            l.error("Invalid struct default index");
          subscript = index.value;
          l.expect(square ? "]" : ")");
        }
        if (field->type.dimension() > 1 && subscript < 0) subscript = 0;
        if ((field->type.dimension() > 1) != (subscript >= 0) ||
            subscript >= field->type.dimension())
          l.error("Struct default dimension mismatch");
        l.expect("=");
        auto key = field->name +
                   (subscript < 0 ? "" : "(" + std::to_string(subscript) + ")");
        if (!value.members
                 .emplace(key, compoundDefault(field->type.isDynamicArray()
                                                   ? field->type
                                                   : field->type.element(),
                                               depth + 1))
                 .second)
          l.error("Duplicate struct default: " + key);
      } while (l.accept(","));
    l.expect(")");
    return value;
  }
  static void mergeDefault(Value& target, Value patch) {
    if (target.type != patch.type || patch.type.kind != Type::Struct ||
        patch.type.isArray()) {
      target = std::move(patch);
      return;
    }
    target.text = patch.text;
    for (auto& member : patch.members) {
      auto found = target.members.find(member.first);
      if (found == target.members.end())
        target.members.emplace(member.first, std::move(member.second));
      else
        mergeDefault(found->second, std::move(member.second));
    }
  }
  Value readDefaultValue(Type type, const Package& package, std::size_t owner,
                         Reader& data, int depth = 0) {
    if (depth > 64) l.error("Imported default nesting limit");
    Value value{type};
    if (type.isDynamicArray()) {
      auto count = data.index();
      if (count < 0 || count > 1000000) l.error("Imported default array limit");
      for (int i = 0; i < count; ++i)
        value.elements.push_back(
            readDefaultValue(type.element(), package, owner, data, depth + 1));
      return value;
    }
    switch (type.kind) {
      case Type::Byte:
      case Type::Enum:
      case Type::Bool:
        value.value = data.byte();
        break;
      case Type::Int:
      case Type::Float:
        value.value = data.i32();
        break;
      case Type::String:
        value.text = data.string();
        break;
      case Type::Name:
        value.text = package.name(data.index());
        break;
      case Type::Object:
      case Type::Class: {
        auto reference = data.index();
        value.value = reference ? imports.symbol({owner, reference}) : 0;
        break;
      }
      case Type::Struct: {
        value.text = loadStruct(type).name;
        auto id = type.identity();
        if (id == "core.object.vector" || id == "core.object.rotator" ||
            id == "core.object.color") {
          auto fields = id == "core.object.vector"
                            ? std::vector<std::string>{"X", "Y", "Z"}
                        : id == "core.object.rotator"
                            ? std::vector<std::string>{"Pitch", "Yaw", "Roll"}
                            : std::vector<std::string>{"B", "G", "R", "A"};
          for (auto& name : fields)
            value.members[structField(type, name)->name] = readDefaultValue(
                structField(type, name)->type, package, owner, data, depth + 1);
        } else {
          for (auto& tag : defaultTags(package, data)) {
            auto field = structField(type, tag.name);
            if (!field)
              l.error("Unknown imported struct default field: " + tag.name);
            if (field->type.dimension() > 1 && tag.subscript < 0)
              tag.subscript = 0;
            if ((field->type.dimension() > 1) != (tag.subscript >= 0) ||
                tag.subscript >= field->type.dimension())
              l.error("Imported struct default dimension mismatch");
            Reader nested{tag.data};
            auto key =
                field->name + (tag.subscript < 0
                                   ? ""
                                   : "(" + std::to_string(tag.subscript) + ")");
            value.members[key] = readDefaultValue(
                field->type.isDynamicArray() ? field->type
                                             : field->type.element(),
                package, owner, nested, depth + 1);
            if (nested.pos != nested.data.size())
              l.error("Trailing imported struct default data");
          }
        }
        break;
      }
      default:
        l.error("Unsupported imported default type");
    }
    return value;
  }
  Value inheritedStructDefault(const std::string& name, int subscript,
                               Type type) {
    Value value{type};
    value.text = loadStruct(type).name;
    auto chain = target.ancestry(c.base);
    for (auto owner = chain.rbegin(); owner != chain.rend(); ++owner) {
      auto& package = target.packages[owner->package].data;
      for (auto& tag : classDefaults(package, owner->id))
        if (lower(tag.name) == lower(name) &&
            (tag.subscript == subscript ||
             (subscript == 0 && tag.subscript < 0))) {
          Reader data{tag.data};
          mergeDefault(value,
                       readDefaultValue(type, package, owner->package, data));
          if (data.pos != data.data.size())
            l.error("Trailing inherited struct default data");
        }
    }
    std::vector<const Class*> parents;
    for (auto owner = c.parent; owner; owner = owner->parent)
      parents.push_back(owner);
    auto key = lower(name) +
               (subscript < 0 ? "" : "(" + std::to_string(subscript) + ")");
    for (auto owner = parents.rbegin(); owner != parents.rend(); ++owner) {
      auto found = (*owner)->defaults.find(key);
      if (found != (*owner)->defaults.end()) mergeDefault(value, found->second);
    }
    return value;
  }
  std::vector<Value> inheritedArrayDefaults(const std::string& name,
                                            Type type) {
    for (auto owner : target.ancestry(c.base)) {
      auto& package = target.packages[owner.package].data;
      for (auto& tag : classDefaults(package, owner.id))
        if (lower(tag.name) == lower(name)) {
          if (tag.kind != 9 || tag.subscript != -1)
            l.error("Invalid inherited dynamic array default");
          Reader data{tag.data};
          int count = data.index();
          if (count < 0 || count > 1000000)
            l.error("Inherited dynamic array default exceeds supported size");
          std::vector<Value> values;
          for (int i = 0; i < count; ++i) {
            Value value{type, 0};
            switch (type.kind) {
              case Type::Struct:
                value = readDefaultValue(type, package, owner.package, data);
                break;
              case Type::Byte:
              case Type::Enum:
                value.value = data.byte();
                break;
              case Type::Int:
              case Type::Float:
                value.value = data.i32();
                break;
              case Type::String:
                value.text = data.string();
                break;
              case Type::Name:
                value.text = package.name(data.index());
                break;
              case Type::Object: {
                auto reference = data.index();
                value.value =
                    reference ? imports.symbol({owner.package, reference}) : 0;
                break;
              }
              case Type::Class: {
                auto reference = data.index();
                value.value =
                    reference ? imports.symbol({owner.package, reference}) : 0;
                break;
              }
              default:
                l.error("Unsupported array default element");
            }
            values.push_back(std::move(value));
          }
          if (data.pos != data.data.size())
            l.error("Inherited array default size mismatch");
          return values;
        }
    }
    return {};
  }
  Function importedFunction(Symbol symbol) {
    auto& p = target.packages[symbol.package].data;
    if (p.kind(symbol.id) != "Function")
      l.error("Referenced member is not a function");
    auto info = functionInfo(p, symbol.id);
    Function fn;
    fn.owner = memberOwner(symbol);
    fn.name = p.name(p.exports[symbol.id - 1].name);
    fn.id = imports.symbol(symbol);
    fn.isStatic = (info.flags & 0x2000) != 0;
    fn.isFinal = (info.flags & 1) != 0;
    fn.native = info.native;
    fn.inheritedFlags = info.flags;
    if (info.flags & 0x100000) {
      auto cls = target.findClass(fn.owner.identity());
      auto storage = cls ? target.member(*cls, "__" + fn.name + "__Delegate")
                         : std::optional<Symbol>{};
      if (!storage) l.error("Missing imported delegate storage");
      fn.delegateStorage = imports.symbol(*storage);
    }
    if (info.result) {
      fn.type = importedType(p, *info.result);
      if (fn.type == Type::Void || info.result->dimension != 1)
        l.error("Unsupported imported return type: " + info.result->kind);
    }
    for (auto& param : info.parameters) {
      auto t = importedType(p, param);
      if (t == Type::Void || param.dimension != 1 || (param.flags & 0x200))
        l.error("Unsupported imported parameter: " + param.kind);
      Variable v{p.name(p.exports[param.id - 1].name), t, 0, param.flags};
      fn.params.push_back(std::move(v));
    }
    return fn;
  }
  bool usable(const NativeInfo& op) const {
    if (!op.result || propertyType(*op.result) == Type::Void) return false;
    for (std::size_t i = 0; i < op.parameters.size(); ++i) {
      auto& p = op.parameters[i];
      if (propertyType(p) == Type::Void || p.dimension != 1 || (p.flags & 0x10))
        return false;
      if ((p.flags & 0x200) &&
          (i + 1 != op.parameters.size() || propertyType(p) != Type::Bool ||
           (op.symbol != "&&" && op.symbol != "||")))
        return false;
    }
    return true;
  }
  Expr operation(const std::string& symbol, std::vector<Expr> args,
                 bool prefix) {
    const NativeInfo* found = nullptr;
    int best = 1000000;
    bool ambiguous = false;
    for (auto& op : operators) {
      if (op.symbol != symbol || bool(op.flags & 0x10) != prefix ||
          op.parameters.size() != args.size() || !usable(op))
        continue;
      bool match = true;
      int score = 0;
      for (std::size_t i = 0; i < args.size(); ++i) {
        int cost = conversionCost(args[i], propertyType(op.parameters[i]),
                                  op.parameters[i].flags);
        if (cost < 0)
          match = false;
        else
          score += cost;
      }
      if (match && score < best) {
        best = score;
        found = &op;
        ambiguous = false;
      } else if (match && score == best)
        ambiguous = true;
    }
    if (ambiguous) l.error("Ambiguous native operator: " + symbol);
    if (!found && !prefix && args.size() == 2 &&
        (symbol == "==" || symbol == "!=") &&
        args[0].type.kind == Type::Struct && !args[0].type.isArray()) {
      if (args[0].type != args[1].type)
        l.error("Struct comparison type mismatch");
      Expr result;
      result.type = Type::Bool;
      result.objectOp = symbol == "==" ? 0x32 : 0x33;
      result.writable = false;
      result.args = std::move(args);
      return result;
    }
    if (!found)
      l.error("Unsupported operator/signature in target Core: " + symbol);
    Expr e;
    e.type = propertyType(*found->result);
    e.writable = false;
    if (e.type.kind == Type::Struct) loadStruct(e.type);
    e.native = found->native;
    for (std::size_t i = 0; i < args.size(); ++i) {
      auto& param = found->parameters[i];
      if ((param.flags & 0x100) && (!args[i].ref || !args[i].writable))
        l.error("Operator requires writable variable: " + symbol);
      args[i] =
          convert(std::move(args[i]), propertyType(param), false, param.flags);
      e.skipArgs.push_back((param.flags & 0x200) != 0);
    }
    e.args = std::move(args);
    return e;
  }
  Expr constant(Value value) {
    if (value.type != Type::String || value.text.empty())
      return openucc::constant(std::move(value));
    // UE2 SerializeExpr tests only the high byte of Unicode code units.
    // Keep Latin-1 runs in ANSI literals and concatenate with Unicode runs.
    std::vector<std::string> runs;
    bool previous = false;
    for (std::size_t i = 0; i < value.text.size();) {
      auto lead = static_cast<unsigned char>(value.text[i]);
      bool ansi = lead < 0xc4;
      auto length = lead < 0x80 ? 1u : lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
      if (runs.empty() || ansi != previous) runs.emplace_back();
      runs.back().append(value.text, i, length);
      previous = ansi;
      i += length;
    }
    Expr result = openucc::constant({Type::String, 0, runs.front()});
    for (std::size_t i = 1; i < runs.size(); ++i)
      result = operation(
          "$",
          {std::move(result), openucc::constant({Type::String, 0, runs[i]})},
          false);
    return result;
  }
  Expr atom() {
    if (l.is("vect") || l.is("rot")) {
      auto next = l;
      next.next();
      if (next.is("(")) return constant(constantLiteral());
    }
    if (auto value = qualifiedEnumValue()) return constant(*value);
    if (!l.token.quote && !l.token.text.empty() &&
        ((l.token.text[0] >= 'A' && l.token.text[0] <= 'Z') ||
         (l.token.text[0] >= 'a' && l.token.text[0] <= 'z') ||
         l.token.text[0] == '_')) {
      auto saved = l;
      auto name = l.identifier();
      while (l.accept(".")) name += "." + l.identifier();
      if (l.token.quote == '\'') {
        l = std::move(saved);
        return constant(constantLiteral());
      }
      if (name.find('.') != name.npos && l.is("(")) {
        auto type = enumType(name);
        if (!type) type = classType(name);
        if (type) {
          l.next();
          auto value = expression();
          l.expect(")");
          return convert(std::move(value), *type, true);
        }
      }
      l = std::move(saved);
    }
    if (!l.token.quote) {
      bool shadowed = false;
      auto lookahead = l;
      lookahead.next();
      if (lookahead.is("(")) shadowed = true;
      for (const auto* vars : {&f->params, &f->locals})
        for (const auto& v : *vars)
          if (lower(v.name) == lower(l.token.text)) shadowed = true;
      for (const Class* owner = &c; owner; owner = owner->parent)
        for (const auto& v : owner->vars)
          if (lower(v.name) == lower(l.token.text)) shadowed = true;
      if (!lookahead.is("("))
        if (expectedEnum.kind == Type::Enum) {
          std::vector<std::string> names;
          auto local = enumerations.find(expectedEnum);
          if (local != enumerations.end())
            names = local->second.names;
          else {
            auto symbol = structSymbol(expectedEnum.identity(), "Enum");
            if (!symbol)
              l.error("Unresolved expected enum: " + expectedEnum.identity());
            names =
                enumNames(target.packages[symbol->package].data, symbol->id);
          }
          for (std::size_t i = 0; i < names.size(); ++i)
            if (lower(names[i]) == lower(l.token.text)) {
              l.next();
              return constant(Value{expectedEnum, int(i)});
            }
        }
      if (!shadowed)
        if (auto value = namedConstant(l.token.text)) {
          l.next();
          return constant(*value);
        }
    }
    if (l.accept("default")) {
      l.expect(".");
      return variable(true, true);
    }
    if (l.is("class")) {
      auto saved = l;
      l.next();
      bool quoted = l.token.quote == '\'';
      bool cast = l.is("<") || l.is("(");
      l = std::move(saved);
      if (quoted) {
        auto e = constant(valueLiteral());
        e.objectOp = 0x20;
        return e;
      }
      if (cast) {
        auto t = parseType();
        l.expect("(");
        auto value = expression();
        l.expect(")");
        return convert(std::move(value), t, true);
      }
    }
    if (l.accept("new")) {
      Expr e;
      e.objectOp = 0x11;
      e.writable = false;
      e.args.resize(3);
      Type suppliedOuter;
      if (l.accept("(")) {
        std::size_t i = 0;
        while (!l.is(")")) {
          if (i >= 3) l.error("Too many new arguments");
          if (!l.is(",")) {
            auto value = expression();
            if (i == 0) suppliedOuter = value.type;
            e.args[i] =
                convert(std::move(value),
                        i == 0 ? *classType("Core.Object")
                               : Type(i == 1 ? Type::String : Type::Int));
          }
          ++i;
          if (!l.accept(",")) break;
        }
        l.expect(")");
      }
      auto factory = postfix(atom());
      if (factory.type.kind != Type::Class || factory.type.isArray())
        l.error("new requires a class value");
      e.type = Type::named(Type::Object, factory.type.identity());
      auto requiredOuter = withinOf(e.type);
      if (requiredOuter != Type::Void && suppliedOuter != Type::Void &&
          objectDistance(suppliedOuter, requiredOuter) < 0)
        l.error("new outer is incompatible with within class");
      e.args.push_back(std::move(factory));
      return e;
    }
    if (l.accept("(")) {
      auto e = expression();
      l.expect(")");
      return e;
    }
    if (l.is("-") || l.is("!") || l.is("~") || l.is("++") || l.is("--")) {
      auto op = l.token.text;
      if (op == "-") {
        auto saved = l;
        l.next();
        bool numericLiteral = number(l.token);
        l = std::move(saved);
        if (numericLiteral) return constant(constantLiteral());
      }
      l.next();
      return operation(op, {expression(257)}, true);
    }
    if (l.token.quote || l.is("true") || l.is("false") || l.is("none") ||
        number(l.token))
      return constant(constantLiteral());
    auto saved = l;
    auto name = l.identifier();
    if (lower(name) == "self" && !l.is(".")) {
      if (f->isStatic) l.error("self in static function");
      Expr e;
      e.type = *classType(c.name);
      e.objectOp = 0x17;
      e.writable = false;
      return e;
    }
    bool superCall = false, explicitSelf = false, globalCall = false;
    std::string superOwner;
    if (lower(name) == "super" && l.accept("(")) {
      superOwner = l.identifier();
      l.expect(")");
      l.expect(".");
      superCall = true;
      name = l.identifier();
    }
    if ((lower(name) == "super" || lower(name) == "self" ||
         lower(name) == "global") &&
        l.accept(".")) {
      superCall = lower(name) == "super";
      globalCall = lower(name) == "global";
      explicitSelf = lower(name) == "self";
      if (explicitSelf && f->isStatic) l.error("self in static function");
      saved = l;
      name = l.identifier();
    }
    if (!l.accept("(")) {
      if (superCall) l.error("super requires a function call");
      l = std::move(saved);
      return variable(explicitSelf);
    }
    if (!superCall && !explicitSelf && !globalCall) {
      auto t = primitiveType(name);
      if (!t && (lower(name) == "vector" || lower(name) == "rotator"))
        t = structType("Core.Object." + name);
      if (!t) t = enumType(name);
      if (!t && expectedEnum.kind == Type::Enum &&
          lower(name) == expectedEnum.identity().substr(
                             expectedEnum.identity().rfind('.') + 1))
        t = expectedEnum;
      if (!t) {
        bool callable = parentFunction(name) != nullptr;
        for (const auto& fn : c.funcs)
          if (!fn.state && lower(fn.name) == lower(name)) callable = true;
        if (f->state && stateFunction(name, f->state)) callable = true;
        auto member = target.member(c.base, name);
        if (member && target.packages[member->package].data.kind(member->id) ==
                          "Function")
          callable = true;
        if (!callable) t = classType(name);
      }
      if (t) {
        if (t->isInt64()) loadStruct(*t);
        auto arg = expression();
        l.expect(")");
        return convert(std::move(arg), *t, true);
      }
    }
    const Function* callee = nullptr;
    Function inherited;
    if (!superOwner.empty()) {
      bool found = false;
      for (auto parent = c.parent; parent; parent = parent->parent) {
        if (lower(parent->name) == lower(superOwner)) found = true;
        if (found)
          for (const auto& fn : parent->funcs)
            if (!fn.state && lower(fn.name) == lower(name) && !callee)
              callee = &fn;
      }
      if (!callee) {
        for (auto ancestor : target.ancestry(c.base)) {
          if (ancestor.id <= 0) continue;
          const auto& package = target.packages[ancestor.package].data;
          if (lower(package.name(package.exports[ancestor.id - 1].name)) ==
              lower(superOwner))
            found = true;
          if (!found) continue;
          auto symbol = target.member(ancestor, name);
          if (symbol) {
            inherited = importedFunction(*symbol);
            callee = &inherited;
            break;
          }
        }
      }
      if (!found || !callee) l.error("Unknown qualified super function");
    }
    if (f->state && !globalCall && superOwner.empty()) {
      auto scoped = stateFunction(name, f->state, superCall);
      if (scoped) {
        inherited = *scoped;
        callee = &inherited;
      }
      if (!callee && !superCall)
        for (auto& fn : c.funcs)
          if (!fn.state && lower(fn.name) == lower(name)) callee = &fn;
    }
    if (!superCall) {
      auto shadows = [&](const std::vector<Variable>& variables) {
        return std::any_of(variables.begin(), variables.end(),
                           [&](const Variable& variable) {
                             return lower(variable.name) == lower(name);
                           });
      };
      if (shadows(c.vars) || parentVariable(name) ||
          (!explicitSelf && (shadows(f->locals) || shadows(f->params))))
        l.error("Variable shadows function: " + name);
    }
    if (!superCall && !callee)
      for (auto& fn : c.funcs)
        if (!fn.state && lower(fn.name) == lower(name)) callee = &fn;
    if (!callee) callee = parentFunction(name);
    if (!callee) {
      auto symbol = target.member(c.base, name);
      if (!symbol) {
        if (!superCall && c.withinType != Type::Void &&
            c.withinType.identity() != "core.object") {
          if (f->isStatic) l.error("Outer call in static function");
          return outerMember(std::move(saved));
        }
        l.error("Unknown function: " + name);
      }
      inherited = importedFunction(*symbol);
      callee = &inherited;
    }
    if (f->isStatic && !callee->isStatic)
      l.error("Instance function called from static context: " + name);
    auto result = call(*callee, superCall);
    if (globalCall && !result.native) result.objectOp = 0x38;
    return result;
  }
  Expr call(const Function& function, bool superCall = false) {
    if (((function.flags | function.inheritedFlags) & 8) &&
        (!f->stateScript || iteratorDepth))
      l.error("Latent calls require state code outside foreach");
    bool iterator = ((function.flags | function.inheritedFlags) & 4) != 0;
    if (iterator && !iteratorHead)
      l.error("Iterator function requires foreach");
    iteratorHead = false;
    auto visibility = function.flags | function.inheritedFlags;
    checkAccess(visibility & 0x40000   ? 1
                : visibility & 0x80000 ? 2
                                       : 0,
                function.owner);
    const Function* callee = &function;
    auto name = callee->name;
    Expr e;
    e.type = callee->type;
    e.iterator = iterator;
    e.writable = false;
    e.call = callee->native ? 0 : callee->id;
    e.native = callee->native;
    e.callName = callee->name;
    e.finalCall = superCall || callee->isStatic || callee->isFinal;
    if (callee->delegateStorage && !superCall) {
      e.objectOp = 0x43;
      e.ref = callee->delegateStorage;
    }
    std::size_t i = 0;
    std::optional<Type> iteratorClass;
    if (!l.is(")"))
      for (;;) {
        if (i >= callee->params.size())
          l.error("Too many arguments to " + name);
        auto& param = callee->params[i];
        auto argumentType = param.type;
        if (iteratorClass && i == 1 && (param.flags & 0x100) &&
            param.type.kind == Type::Object && !param.type.isArray() &&
            callee->params[0].type.identity() == param.type.identity())
          argumentType = *iteratorClass;
        Expr arg;
        if (l.is(",") || l.is(")")) {
          if (!(param.flags & 0x10))
            l.error("Required argument omitted: " + param.name);
        } else {
          arg = expression(0, argumentType, argumentType.dimension() > 1);
          if (iterator && i == 0 && param.type.kind == Type::Class &&
              arg.type.kind == Type::Class && !arg.type.isArray())
            iteratorClass = Type::named(Type::Object, arg.type.identity());
          // The native Actor.Spawn contract specializes its return type to
          // the supplied class. Ordinary class-taking functions do not.
          if (i == 0 && callee->native == 278 && lower(name) == "spawn" &&
              classType("Engine.Actor") == std::optional<Type>{callee->owner} &&
              callee->type == callee->owner && param.type.kind == Type::Class &&
              param.type.identity() == callee->owner.identity() &&
              arg.type.kind == Type::Class && !arg.type.isArray())
            e.type = Type::named(Type::Object, arg.type.identity());
          if ((param.flags & 0x100) &&
              (!arg.ref || !arg.writable || arg.type != argumentType))
            l.error(
                "Out argument requires a writable variable of the exact "
                "type: " +
                param.name);
          arg = convert(std::move(arg), argumentType, false, param.flags);
        }
        e.args.push_back(std::move(arg));
        ++i;
        if (!l.accept(",")) break;
      }
    l.expect(")");
    for (; i < callee->params.size(); ++i) {
      if (!(callee->params[i].flags & 0x10))
        l.error("Missing argument: " + callee->params[i].name);
    }
    return e;
  }
  Expr member(Expr receiver, const std::string& mode = {}) {
    auto name = l.identifier();
    const Variable* field = nullptr;
    const Function* function = nullptr;
    Function imported;
    std::optional<Symbol> external;
    if (receiver.type.identity().rfind("@local.", 0) == 0) {
      auto local = receiver.type.identity().substr(7);
      const Class* owner =
          local == lower(c.name) ? &c : sourceClasses->at(local);
      for (auto current = owner; current; current = current->parent) {
        for (auto& v : current->vars)
          if (lower(v.name) == lower(name)) field = &v;
        for (auto& fn : current->funcs)
          if (!fn.state && lower(fn.name) == lower(name)) function = &fn;
        if (field || function) break;
        for (const auto& item : current->constants)
          if (lower(item.name) == lower(name)) {
            auto value = constant(item.value);
            value.context = std::make_shared<Expr>(std::move(receiver));
            return value;
          }
      }
      if (!field && !function) external = target.member(owner->base, name);
    } else {
      auto owner = target.findClass(receiver.type.identity());
      if (!owner) l.error("Unresolved context class");
      external = target.member(*owner, name);
    }
    Expr result;
    if (external &&
        target.packages[external->package].data.kind(external->id) == "Const") {
      result = constant(importedConstant(*external));
      result.context = std::make_shared<Expr>(std::move(receiver));
      return result;
    }
    if (!function && external &&
        target.packages[external->package].data.kind(external->id) ==
            "Function") {
      imported = importedFunction(*external);
      function = &imported;
    }
    if (l.accept("(")) {
      if (mode == "default") l.error("default requires a property");
      if (field) l.error("Field is not callable: " + name);
      if (!function) {
        if (!external) l.error("Unknown context function: " + name);
        imported = importedFunction(*external);
        function = &imported;
      }
      result = call(*function);
      if (mode == "static") {
        if (!function->isStatic)
          l.error("Class static context requires a static function");
        result.finalCall = function->isFinal;
      }
    } else {
      if (mode == "static") l.error("static requires a function call");
      if (function) {
        result = functionValue(*function);
        result.context = std::make_shared<Expr>(std::move(receiver));
        return result;
      }
      if (field) {
        checkAccess(field->access, field->owner);
        result.type = field->type;
        result.type =
            contextPropertyType(result.type, name, field->owner, receiver.type);
        result.ref = field->id;
        result.writable = !(field->flags & 2);
      } else {
        if (!external) l.error("Unknown context property: " + name);
        auto& p = target.packages[external->package].data;
        auto prop = propertyInfo(p, external->id);
        checkAccess(importedAccess(*external), memberOwner(*external));
        result.type = importedType(p, prop);
        result.type = contextPropertyType(
            result.type, name, memberOwner(*external), receiver.type);
        if (result.type == Type::Void)
          l.error("Unsupported context property type");
        result.ref = imports.symbol(*external);
        result.writable = !(prop.flags & 2);
      }
      result.instance = true;
      result.defaultRef = mode == "default";
    }
    result.context = std::make_shared<Expr>(std::move(receiver));
    if (lower(name) == "outer" && result.type.kind == Type::Object) {
      auto within = withinOf(result.context->type);
      if (within != Type::Void) result.type = within;
    }
    return result;
  }
  Expr postfix(Expr e) {
    for (;;) {
      if (e.type.kind == Type::Struct && !e.type.isArray() && l.accept(".")) {
        auto name = l.identifier();
        auto field = structField(e.type, name);
        if (!field) l.error("Unknown struct field: " + name);
        checkAccess(field->access, field->owner);
        Expr member;
        member.type = field->type;
        member.ref = field->id;
        member.objectOp = 0x36;
        member.writable = e.writable && !(field->flags & 2);
        member.args.push_back(std::move(e));
        e = std::move(member);
        continue;
      }
      if (e.type.kind == Type::Class && !e.type.isArray() && l.accept(".")) {
        auto mode = lower(l.identifier());
        if (mode != "static" && mode != "default")
          l.error("Class context requires static or default");
        l.expect(".");
        e = member(std::move(e), mode);
        continue;
      }
      if (e.type.isArray() &&
          (l.is("[") || l.is(".") ||
           (!e.type.isDynamicArray() && !staticArrayArgument))) {
        e = element(std::move(e));
        continue;
      }
      if (e.type.kind == Type::Object && !e.type.isArray() && l.accept(".")) {
        if (e.type.identity().empty()) l.error("None has no member type");
        std::string mode;
        if (l.accept("default")) {
          mode = "default";
          l.expect(".");
        }
        e = member(std::move(e), mode);
        continue;
      }
      return e;
    }
  }
  Expr expression(int minimum = 0, Type expected = Type::Void,
                  bool arrayArgument = false) {
    struct RestoreArrayArgument {
      bool& target;
      bool saved;
      ~RestoreArrayArgument() { target = saved; }
    } restoreArray{staticArrayArgument, staticArrayArgument};
    staticArrayArgument = arrayArgument;
    struct RestoreType {
      Type& target;
      Type saved;
      ~RestoreType() { target = saved; }
    } restore{expectedEnum, expectedEnum};
    if (expected != Type::Void)
      expectedEnum = expected.kind == Type::Enum && !expected.isArray()
                         ? expected
                         : Type::Void;
    auto lhs = postfix(atom());
    for (;;) {
      int binding = -1;
      auto symbol = l.token.text;
      if ((symbol == "++" || symbol == "--") && minimum <= 300) {
        l.next();
        lhs = operation(symbol, {std::move(lhs)}, false);
        continue;
      }
      for (auto& op : operators)
        if (op.symbol == symbol && !(op.flags & 0x10) &&
            op.parameters.size() == 2 && usable(op)) {
          int candidate = 256 - op.precedence;
          if (binding != -1 && candidate != binding)
            l.error("Conflicting operator precedences");
          binding = candidate;
        }
      if (binding < minimum) break;
      l.next();
      auto rhs = expression(binding + 1, lhs.type);
      lhs = operation(symbol, {std::move(lhs), std::move(rhs)}, false);
    }
    return lhs;
  }
  void condition(const Expr& e) {
    if (e.type != Type::Bool) l.error("Condition requires scalar bool");
  }
  std::vector<Statement> block() {
    if (!l.accept("{")) return {statement()};
    std::vector<Statement> out;
    while (!l.accept("}")) {
      if (l.end()) l.error("Unterminated block");
      out.push_back(statement());
    }
    return out;
  }
  Statement simple() {
    Statement s{};
    s.lhs = expression();
    if (l.accept("=")) {
      if (!s.lhs.ref || !s.lhs.writable)
        l.error("Assignment requires a writable variable");
      s.kind = Statement::Assign;
      s.rhs = convert(expression(0, s.lhs.type), s.lhs.type);
    } else {
      if (!hasCall(s.lhs)) l.error("Expected assignment or function call");
      s.kind = Statement::Evaluate;
    }
    return s;
  }
  static bool hasCall(const Expr& e) {
    if (e.call || e.native || e.arrayOp == 0x40 || e.arrayOp == 0x41 ||
        e.arrayOp == 0x47)
      return true;
    if (e.context && hasCall(*e.context)) return true;
    for (const auto& arg : e.args)
      if (hasCall(arg)) return true;
    return false;
  }
  Statement statement() {
    Statement s{};
    if (l.is("assert")) {
      s.kind = Statement::Assert;
      s.line = l.token.line;
      if (s.line > 65535) l.error("Assertion line exceeds bytecode range");
      l.next();
      l.expect("(");
      s.rhs = convert(expression(), Type::Bool);
      l.expect(")");
      l.expect(";");
      return s;
    }
    if (f && f->stateScript) {
      if (l.accept("stop")) {
        if (iteratorDepth) l.error("stop is not allowed inside foreach");
        s.kind = Statement::Stop;
        l.expect(";");
        return s;
      }
      if (l.accept("goto")) {
        if (iteratorDepth) l.error("goto inside foreach is unsupported");
        s.kind = Statement::Goto;
        s.rhs = convert(expression(), Type::Name);
        l.expect(";");
        return s;
      }
      if (!l.token.quote && !l.end()) {
        auto saved = l;
        auto label = l.token.text;
        l.next();
        if (l.accept(":")) {
          s.kind = Statement::Label;
          s.label = label;
          return s;
        }
        l = std::move(saved);
      }
    }
    if (l.accept("switch")) {
      s.kind = Statement::Switch;
      l.expect("(");
      s.rhs = expression();
      auto type = s.rhs.type;
      if (type.isArray() || (type != Type::Int && type != Type::Bool &&
                             type != Type::Byte && type.kind != Type::Enum &&
                             type != Type::Name && type != Type::String &&
                             type.kind != Type::Object && !type.isInt64()))
        l.error("Unsupported switch selector type");
      l.expect(")");
      l.expect("{");
      ++switchDepth;
      bool sawDefault = false;
      while (!l.accept("}")) {
        if (sawDefault) l.error("default must be the final switch label");
        Statement arm{};
        arm.kind = Statement::Case;
        if (l.accept("case"))
          arm.rhs = convert(expression(0, type), type);
        else {
          l.expect("default");
          sawDefault = true;
        }
        l.expect(":");
        while (!l.is("case") && !l.is("}")) {
          if (l.is("default")) {
            auto next = l;
            next.next();
            if (next.is(":")) break;
          }
          if (l.end()) l.error("Unterminated switch");
          arm.body.push_back(statement());
        }
        s.body.push_back(std::move(arm));
      }
      --switchDepth;
      return s;
    }
    if (l.accept("return")) {
      s.kind = Statement::Return;
      if (!l.is(";")) s.rhs = expression(0, f->type);
      s.rhs = convert(std::move(s.rhs), f->type);
      l.expect(";");
      return s;
    }
    if (l.accept("if")) {
      s.kind = Statement::If;
      l.expect("(");
      s.rhs = expression();
      condition(s.rhs);
      l.expect(")");
      s.body = block();
      if (l.accept("else")) s.alternative = block();
      return s;
    }
    if (l.accept("foreach")) {
      s.kind = Statement::Foreach;
      iteratorHead = true;
      s.rhs = expression();
      iteratorHead = false;
      if (!s.rhs.iterator) l.error("foreach requires an iterator function");
      ++loopDepth;
      ++iteratorDepth;
      s.body = l.is("{") ? block() : std::vector<Statement>{statement()};
      --iteratorDepth;
      --loopDepth;
      return s;
    }
    if (l.accept("while")) {
      s.kind = Statement::While;
      l.expect("(");
      s.rhs = expression();
      condition(s.rhs);
      l.expect(")");
      ++loopDepth;
      s.body = block();
      --loopDepth;
      return s;
    }
    if (l.accept("for")) {
      s.kind = Statement::For;
      l.expect("(");
      if (!l.is(";")) s.initializer.push_back(simple());
      l.expect(";");
      s.rhs = {Type::Bool, 0, false, 1};
      if (!l.is(";")) s.rhs = expression();
      condition(s.rhs);
      l.expect(";");
      if (!l.is(")")) s.increment.push_back(simple());
      l.expect(")");
      ++loopDepth;
      s.body = block();
      --loopDepth;
      return s;
    }
    if (l.is("break") || l.is("continue")) {
      if (!loopDepth && (l.is("continue") || !switchDepth))
        l.error("Loop control outside a loop or break outside a switch");
      s.kind = l.accept("break") ? Statement::Break : Statement::Continue;
      if (s.kind == Statement::Continue) l.expect("continue");
      l.expect(";");
      return s;
    }
    s = simple();
    l.expect(";");
    return s;
  }
  void readDefaultObject(int outer, const std::string& owner) {
    if (outer != c.id || owner != c.name)
      l.error(
          "Nested Begin Object class-ownership semantics are not supported");
    if (std::count(owner.begin(), owner.end(), '.') >= 64 ||
        c.objects.size() >= 100000)
      l.error("Default subobject nesting/count limit");
    l.expect("begin");
    l.expect("object");
    l.expect("class");
    l.expect("=");
    auto className = l.identifier();
    while (l.accept(".")) className += "." + l.identifier();
    auto type = classType(className);
    if (!type) l.error("Unknown default subobject class: " + className);
    l.expect("name");
    l.expect("=");
    DefaultObject object;
    object.name = l.identifier();
    object.path = owner + "." + object.name;
    object.type = *type;
    object.id = nextId++;
    object.outer = outer;
    object.body = l;
    for (const auto& previous : c.objects)
      if (lower(previous.path) == lower(object.path))
        l.error("Duplicate default subobject: " + object.path);
    c.objects.push_back(object);
    for (;;) {
      if (l.end() || l.is("}")) l.error("Unterminated default subobject");
      if (l.is("begin")) {
        readDefaultObject(object.id, object.path);
        continue;
      }
      if (l.accept("end")) {
        l.expect("object");
        break;
      }
      l.next();
    }
  }
  void readDefaultAssignment() {
    auto n = l.identifier();
    int subscript = -1;
    if (l.accept("(")) {
      auto ix = literal(l);
      if (ix.type != Type::Int || ix.value < 0)
        l.error("Invalid default array index");
      subscript = ix.value;
      l.expect(")");
    }
    l.expect("=");
    auto it = std::find_if(
        c.vars.begin(), c.vars.end(),
        [&](const Variable& x) { return lower(x.name) == lower(n); });
    Variable field;
    if (it != c.vars.end())
      field = *it;
    else if (auto local = parentVariable(n))
      field = *local;
    else {
      auto inherited = target.member(c.base, n);
      if (!inherited) l.error("Unknown default property: " + n);
      auto& p = target.packages[inherited->package].data;
      auto prop = propertyInfo(p, inherited->id);
      auto t = importedType(p, prop);
      if (t == Type::Void) l.error("Unsupported inherited default type");
      field = {p.name(p.exports[inherited->id - 1].name), t, 0};
      if (t.isDynamicArray() && !c.arrayDefaults.count(lower(n)))
        c.arrayDefaults[lower(n)] = inheritedArrayDefaults(n, t.element());
    }
    if (field.type.isDynamicArray()) {
      if (subscript < 0 || subscript >= 1000000)
        l.error("Dynamic array default index outside supported range");
      auto key = lower(n),
           elementKey = key + "(" + std::to_string(subscript) + ")";
      if (!c.arrayDefaultElements.insert(elementKey).second)
        l.error("Duplicate default: " + elementKey);
      auto& values = c.arrayDefaults[key];
      auto t = field.type.element();
      if (std::size_t(subscript) >= values.size())
        values.resize(std::size_t(subscript) + 1,
                      Value{t, 0,
                            t == Type::Name          ? "None"
                            : t.kind == Type::Struct ? loadStruct(t).name
                                                     : ""});
      auto v = compoundDefault(t);
      mergeDefault(values[std::size_t(subscript)], std::move(v));
      c.defaultFields.emplace(key, field);
      l.accept(";");
      return;
    }
    if (field.type.dimension() > 1 && subscript < 0) subscript = 0;
    if ((field.type.dimension() > 1) != (subscript >= 0) ||
        subscript >= field.type.dimension())
      l.error("Default index does not match property dimension");
    auto v = compoundDefault(field.type.element());
    if (field.type.kind == Type::Struct && it == c.vars.end()) {
      auto inherited =
          inheritedStructDefault(n, subscript, field.type.element());
      mergeDefault(inherited, std::move(v));
      v = std::move(inherited);
    }
    auto key = lower(n) +
               (subscript >= 0 ? "(" + std::to_string(subscript) + ")" : "");
    if (!c.defaults.emplace(key, v).second)
      l.error("Duplicate default: " + key);
    c.defaultFields.emplace(key, field);
    c.defaultIndices.emplace(key, subscript);
    l.accept(";");
  }
  Class parse() {
    l.expect("class");
    c.name = l.identifier();
    c.id = nextId++;
    imports.bindLocal(c.name, c.id);
    std::string baseName;
    if (l.accept("extends")) {
      baseName = l.identifier();
      while (l.accept(".")) baseName += "." + l.identifier();
    } else if (lower(packageName) != "core" || lower(c.name) != "object") {
      l.error("Only Core.Object may omit extends");
    }
    c.parent = sourceParent;
    if (c.parent) {
      c.base = c.parent->base;
      c.super = c.parent->id;
      c.arrayDefaults = c.parent->arrayDefaults;
      c.flags |= c.parent->flags & 0x366c;
      c.config = c.parent->config;
      c.within = c.parent->within;
      c.withinType = c.parent->withinType;
    } else if (!baseName.empty()) {
      auto base = target.findClass(baseName);
      if (!base) l.error("Unresolved base class: " + baseName);
      if (base->id < 0)
        l.error("Native superclass metadata is unavailable: " + baseName);
      c.base = *base;
      target.ancestry(c.base);
      c.super = imports.symbol(c.base);
      auto info = classInfo(target.packages[c.base.package].data, c.base.id);
      c.flags |= info.flags & 0x366c;
      c.config = info.config;
      if (info.within) c.within = imports.symbol({c.base.package, info.within});
      if (info.within) {
        auto owner = target.resolve({c.base.package, info.within});
        if (owner)
          c.withinType = Type::named(Type::Object, target.identity(*owner));
      }
    }
    std::set<std::string> classModifiers;
    while (!l.is(";")) {
      auto modifier = lower(l.identifier());
      if (!classModifiers.insert(modifier).second && modifier != "dependson")
        l.error("Duplicate class modifier: " + modifier);
      if (modifier == "abstract")
        c.flags |= 1;
      else if (modifier == "transient")
        c.flags |= 8;
      else if (modifier == "perobjectconfig")
        c.flags |= 0x400;
      else if (modifier == "dynamicrecompile")
        c.flags |= 0x400000;
      else if (modifier == "instanced")
        c.flags |= 0x200000;
      else if (modifier == "native")
        c.nativeClass = true;
      else if (modifier == "noexport")
        c.flags |= 0x100;
      else if (modifier == "safereplace")
        c.flags |= 0x40;
      else if (modifier == "nativereplication")
        c.flags |= 0x800;
      else if (modifier == "editinlinenew")
        c.flags |= 0x1000;
      else if (modifier == "noteditinlinenew")
        c.flags &= ~0x1000u;
      else if (modifier == "collapsecategories")
        c.flags |= 0x2000;
      else if (modifier == "dontcollapsecategories")
        c.flags &= ~0x2000u;
      else if (modifier == "placeable") {
        c.flags |= 0x200;
      } else if (modifier == "notplaceable")
        c.flags &= ~0x200u;
      else if (modifier == "dependson") {
        l.expect("(");
        do {
          auto name = l.identifier();
          while (l.accept(".")) name += "." + l.identifier();
          if (!classType(name)) l.error("Unknown dependsOn class: " + name);
        } while (l.accept(","));
        l.expect(")");
      } else if (modifier == "within") {
        auto name = l.identifier();
        while (l.accept(".")) name += "." + l.identifier();
        auto owner = classType(name);
        if (!owner) l.error("Unknown within class: " + name);
        if (c.withinType != Type::Void &&
            objectDistance(*owner, c.withinType) < 0)
          l.error("Within class is incompatible with superclass within");
        c.withinType = *owner;
      } else if (modifier == "config") {
        c.flags |= 4;
        if (l.accept("(")) {
          c.config = l.identifier();
          l.expect(")");
        }
      } else
        l.error("Unsupported class modifier: " + modifier);
    }
    l.expect(";");
    while (!l.end()) {
      if (l.accept("replication")) {
        if (currentState || !c.replication.empty())
          l.error("Invalid replication block");
        l.expect("{");
        while (!l.accept("}")) {
          Replication rule;
          rule.reliable = l.accept("reliable");
          if (!rule.reliable) l.expect("unreliable");
          l.expect("if");
          l.expect("(");
          rule.start = l;
          int depth = 1;
          while (depth) {
            if (l.end()) l.error("Unterminated replication condition");
            if (l.is("("))
              ++depth;
            else if (l.is(")"))
              --depth;
            l.next();
          }
          do {
            rule.names.push_back(l.identifier());
          } while (l.accept(","));
          l.expect(";");
          c.replication.push_back(std::move(rule));
        }
        continue;
      }
      if (currentState && l.accept("}")) {
        currentState = 0;
        continue;
      }
      if (l.is("state") || l.is("auto")) {
        if (currentState) l.error("Nested state declaration");
        State state;
        if (l.accept("auto")) state.flags |= 2;
        l.expect("state");
        if (l.accept("(")) {
          l.expect(")");
          state.flags |= 1;
        }
        state.name = l.identifier();
        unique(state.name);
        for (const auto& old : c.states) {
          if (lower(old.name) == lower(state.name)) l.error("Duplicate state");
          if ((old.flags & state.flags & 2) != 0)
            l.error("Multiple auto states");
        }
        state.id = nextId++;
        if (l.accept("extends")) state.baseName = l.identifier();
        state.script.state = state.id;
        state.script.stateScript = true;
        state.script.owner = *classType(c.name);
        currentState = state.id;
        c.states.push_back(std::move(state));
        l.expect("{");
        continue;
      }
      if (currentState) {
        auto& state =
            *std::find_if(c.states.begin(), c.states.end(),
                          [&](const State& s) { return s.id == currentState; });
        if (l.accept("ignores")) {
          do {
            state.ignores.push_back(l.identifier());
          } while (l.accept(","));
          l.expect(";");
          continue;
        }
        auto saved = l;
        l.next();
        bool startsCode = l.is(":");
        l = std::move(saved);
        if (startsCode) {
          state.script.bodyStart = l;
          int depth = 1;
          while (depth) {
            if (l.end()) l.error("Unterminated state code");
            if (l.is("{"))
              ++depth;
            else if (l.is("}"))
              --depth;
            l.next();
          }
          currentState = 0;
          continue;
        }
        if (l.is("var") || l.is("struct") || l.is("enum") || l.is("const") ||
            l.is("defaultproperties"))
          l.error("Declaration is not permitted in a state");
      }
      if (l.accept("struct")) {
        parseStruct();
        continue;
      }
      if (l.accept("enum")) {
        parseEnum();
        l.expect(";");
        continue;
      }
      if (l.accept("const")) {
        Constant item;
        item.name = l.identifier();
        unique(item.name);
        l.expect("=");
        auto start = l.token.offset;
        item.value = constantLiteral();
        item.text = l.source.substr(start, l.token.offset - start);
        l.expect(";");
        item.id = nextId++;
        c.constants.push_back(std::move(item));
        continue;
      }
      if (l.accept("var")) {
        std::uint32_t flags = 0;
        int access = 0;
        bool sawAccess = false;
        std::string category;
        if (l.accept("(")) {
          flags |= 1;
          category = l.is(")") ? c.name : l.identifier();
          l.expect(")");
        }
        const std::map<std::string, std::uint32_t> modifiers = {
            {"const", 2},
            {"input", 4},
            {"export", 8},
            {"travel", 0x10000},
            {"editconstarray", 0x40},
            {"editinline", 0x4000000},
            {"editinlineuse", 0x14000000},
            {"editinlinenotify", 0x44000000},
            {"edfindable", 0x8000000},
            {"deprecated", 0x20000000},
            {"native", 0x1000},
            {"transient", 0x2000},
            {"config", 0x4000},
            {"editconst", 0x20000},
            {"localized", 0x8000},
            {"globalconfig", 0x40000}};
        for (;;) {
          if (l.is("private") || l.is("protected") || l.is("public")) {
            if (sawAccess) l.error("Duplicate property access modifier");
            sawAccess = true;
            access = l.is("private") ? 1 : l.is("protected") ? 2 : 0;
            l.next();
            continue;
          }
          auto it = modifiers.find(lower(l.token.text));
          if (l.token.quote || it == modifiers.end()) break;
          if (flags & it->second) l.error("Duplicate property modifier");
          flags |= it->second;
          l.next();
        }
        auto spec = variableType();
        do {
          auto n = l.identifier();
          unique(n);
          c.vars.push_back(declaration(n, spec, flags, category, access));
          if (flags & 0x8000) c.flags |= 0x20;
        } while (l.accept(","));
        l.expect(";");
        continue;
      }
      if (l.accept("defaultproperties")) {
        if (!defaultsEnabled) {
          l.expect("{");
          int depth = 1;
          while (depth) {
            if (l.end()) l.error("Unterminated defaultproperties");
            if (l.is("begin")) {
              readDefaultObject(c.id, c.name);
              continue;
            }
            if (l.is("{"))
              ++depth;
            else if (l.is("}"))
              --depth;
            l.next();
          }
          continue;
        }
        l.expect("{");
        while (!l.accept("}")) {
          if (l.is("begin"))
            readDefaultObject(c.id, c.name);
          else
            readDefaultAssignment();
        }
        continue;
      }
      Function fun;
      fun.owner = *classType(c.name);
      fun.state = currentState;
      const std::map<std::string, std::uint32_t> modifiers = {
          {"iterator", 4},      {"latent", 8},          {"final", 1},
          {"singular", 0x20},
          {"simulated", 0x100}, {"exec", 0x200},        {"static", 0x2000},
          {"private", 0x40000}, {"protected", 0x80000}, {"public", 0x20000}};
      for (;;) {
        if (l.accept("native")) {
          if (fun.flags & 0x400) l.error("Duplicate native modifier");
          fun.flags |= 0x400;
          if (l.accept("(")) {
            auto value = literal(l);
            if (value.type != Type::Int || value.value < 1 ||
                value.value >= 4096)
              l.error("Native index outside 1..4095");
            fun.native = value.value;
            l.expect(")");
          }
          continue;
        }
        auto it = modifiers.find(lower(l.token.text));
        if (l.token.quote || it == modifiers.end()) break;
        if (fun.flags & it->second) l.error("Duplicate function modifier");
        fun.flags |= it->second;
        l.next();
      }
      fun.isStatic = (fun.flags & 0x2000) != 0;
      if ((fun.flags & 4) && !(fun.flags & 0x400))
        l.error("Iterator declarations must be native");
      if ((fun.flags & 8) && !(fun.flags & 0x400))
        l.error("Latent declarations must be native");
      auto accessFlags = fun.flags & 0xe0000;
      if (accessFlags && (accessFlags & (accessFlags - 1)))
        l.error("Conflicting function access modifiers");
      fun.isFinal = (fun.flags & 1) != 0;
      if (l.accept("event"))
        fun.flags |= 0x800;
      else if (l.accept("delegate")) {
        if (currentState || fun.isStatic)
          l.error("Delegate must be a non-static class member");
        fun.flags |= 0x100000;
      } else if (l.is("operator") || l.is("preoperator") ||
                 l.is("postoperator")) {
        bool binary = l.is("operator"), prefix = l.is("preoperator");
        l.next();
        fun.flags |= 0x1000 | (prefix ? 0x10 : 0);
        if (binary) {
          l.expect("(");
          auto value = literal(l);
          if (value.type != Type::Int || value.value < 1 || value.value > 255)
            l.error("Operator precedence outside 1..255");
          fun.precedence = value.value;
          l.expect(")");
        }
      } else
        l.expect("function");
      if (fun.flags & 0x1000) {
        fun.type = variableType();
        fun.friendly = l.token.text;
        l.next();
        if (!(fun.flags & 0x400) || currentState)
          l.error("Only verified native operator declarations are supported");
        auto original = target.findClass(packageName + "." + c.name);
        if (!original)
          l.error("Native operator requires original class metadata");
        const auto& package = target.packages[original->package].data;
        for (std::size_t i = 0; i < package.exports.size(); ++i) {
          if (package.exports[i].outer != original->id ||
              package.kind(int(i) + 1) != "Function")
            continue;
          auto info = functionInfo(package, int(i) + 1);
          if ((info.flags & 0x1410) != (fun.flags & 0x1410) ||
              info.native != fun.native || info.precedence != fun.precedence ||
              lower(info.symbol) != lower(fun.friendly))
            continue;
          if (!fun.name.empty())
            l.error("Ambiguous native operator declaration");
          fun.name = package.name(package.exports[i].name);
        }
        if (fun.name.empty())
          l.error("Native operator does not match target metadata");
      } else {
        auto returnStart = l;
        l.identifier();
        bool hasReturn = !l.is("(");
        l = std::move(returnStart);
        if (hasReturn) fun.type = variableType();
        fun.name = l.identifier();
      }
      unique(fun.name);
      fun.id = nextId++;
      if (fun.flags & 0x100000) {
        fun.delegateStorage = nextId++;
        c.vars.push_back({"__" + fun.name + "__Delegate", delegateType(fun),
                          fun.delegateStorage, 0x400000});
      }
      f = &fun;
      l.expect("(");
      if (!l.is(")")) do {
          std::uint32_t flags = 0;
          while (l.is("optional") || l.is("out") || l.is("coerce") ||
                 l.is("skip")) {
            auto flag = l.accept("optional") ? 0x10u
                        : l.accept("coerce") ? 0x800u
                        : l.accept("skip")   ? 0x200u
                                             : 0x100u;
            if (flag == 0x100) l.expect("out");
            if (flags & flag) l.error("Duplicate parameter modifier");
            flags |= flag;
          }
          auto spec = variableType();
          auto n = l.identifier();
          localUnique(n);
          auto v = declaration(n, spec, flags);
          fun.params.push_back(std::move(v));
        } while (l.accept(","));
      l.expect(")");
      if (fun.flags & 0x1000) {
        auto owner = *target.findClass(packageName + "." + c.name);
        auto original = target.member(owner, fun.name);
        if (!original) l.error("Missing native operator export");
        const auto& package = target.packages[owner.package].data;
        auto info = functionInfo(package, original->id);
        auto identity = [&](Type type) {
          auto name = type.identity();
          for (const auto& prefix :
               {std::string("@local."), std::string("@enum.")})
            if (name.rfind(prefix, 0) == 0)
              name = lower(packageName) + "." + name.substr(prefix.size());
          return std::make_pair(type.kind, name);
        };
        if (!info.result || info.parameters.size() != fun.params.size() ||
            identity(importedType(package, *info.result)) != identity(fun.type))
          l.error("Native operator signature mismatch");
        for (std::size_t i = 0; i < fun.params.size(); ++i)
          if (identity(importedType(package, info.parameters[i])) !=
                  identity(fun.params[i].type) ||
              (info.parameters[i].flags & 0xb10) !=
                  (fun.params[i].flags & 0xb10))
            l.error("Native operator parameter mismatch");
      }
      // Reserve a return-property export before locals; it is part of params.
      if (fun.type != Type::Void) {
        fun.returnId = nextId++;
        if (fun.type.isDynamicArray()) fun.returnInner = nextId++;
      }
      if (l.accept(";")) {
        fun.defined = false;
        c.funcs.push_back(std::move(fun));
        f = nullptr;
        continue;
      }
      l.expect("{");
      if (fun.flags & 0x400)
        l.error("Native function cannot have a script body");
      while (l.accept("local")) {
        auto spec = variableType();
        do {
          auto n = l.identifier();
          localUnique(n);
          fun.locals.push_back(declaration(n, spec));
        } while (l.accept(","));
        l.expect(";");
      }
      fun.bodyStart = l;
      int depth = 1;
      while (depth) {
        if (l.end()) l.error("Unterminated function");
        if (l.is("{"))
          ++depth;
        else if (l.is("}"))
          --depth;
        l.next();
      }
      c.funcs.push_back(std::move(fun));
      f = nullptr;
    }
    if (currentState) l.error("Unterminated state");
    for (auto& state : c.states) {
      auto stateBaseName = state.baseName.empty() ? state.name : state.baseName;
      if (!state.baseName.empty())
        for (const auto& base : c.states)
          if (lower(base.name) == lower(stateBaseName)) state.super = base.id;
      if (!state.super)
        for (auto owner = c.parent; owner && !state.super;
             owner = owner->parent)
          for (const auto& base : owner->states)
            if (lower(base.name) == lower(stateBaseName)) state.super = base.id;
      if (!state.super) {
        auto external = target.member(c.base, stateBaseName);
        if (external && target.packages[external->package].data.kind(
                            external->id) == "State") {
          state.externalSuper = external;
          state.super = imports.symbol(*external);
        } else if (!state.baseName.empty())
          l.error("Unknown base state: " + stateBaseName);
      }
    }
    for (auto& state : c.states) {
      std::set<int> seen;
      for (const State* base = &state; base; base = sourceState(base->super))
        if (!seen.insert(base->id).second) l.error("Cyclic state inheritance");
      for (const auto& name : state.ignores) {
        auto inherited = stateFunction(name, state.id);
        if (inherited && inherited->state == state.id)
          l.error("State both defines and ignores function: " + name);
        if (!inherited)
          for (const auto& fn : c.funcs)
            if (!fn.state && lower(fn.name) == lower(name)) inherited = fn;
        if (!inherited) {
          auto base = parentFunction(name);
          if (base) inherited = *base;
        }
        if (!inherited) {
          auto base = target.member(c.base, name);
          if (base) inherited = importedFunction(*base);
        }
        if (!inherited || inherited->isFinal)
          l.error("Cannot ignore function: " + name);
        auto bit = probeBit(name);
        if (bit >= 0) {
          state.ignore &= ~(std::uint64_t(1) << bit);
          continue;
        }
        auto stub = *inherited;
        stub.state = state.id;
        stub.super = stub.id;
        stub.id = nextId++;
        stub.owner = *classType(c.name);
        stub.defined = false;
        stub.body.clear();
        stub.bodyStart.reset();
        stub.locals.clear();
        for (auto& param : stub.params) {
          param.id = nextId++;
          if (param.type.isDynamicArray()) param.inner = nextId++;
        }
        if (stub.type != Type::Void) {
          stub.returnId = nextId++;
          if (stub.type.isDynamicArray()) stub.returnInner = nextId++;
        }
        c.funcs.push_back(std::move(stub));
      }
    }
    for (auto& fn : c.funcs) {
      if (parentVariable(fn.name))
        l.error("Inherited member collision: " + fn.name);
      Function inherited;
      bool sameClassGlobal = false;
      auto scoped = fn.state ? stateFunction(fn.name, fn.state, true)
                             : std::optional<Function>{};
      if (!scoped && fn.state)
        for (const auto& global : c.funcs)
          if (!global.state && lower(global.name) == lower(fn.name)) {
            scoped = global;
            sameClassGlobal = true;
          }
      if (scoped)
        inherited = *scoped;
      else if (auto local = parentFunction(fn.name))
        inherited = *local;
      else {
        auto parent = target.member(c.base, fn.name);
        if (!parent || target.packages[parent->package].data.kind(parent->id) !=
                           "Function")
          continue;
        inherited = importedFunction(*parent);
      }
      auto inheritedAccess = inherited.flags | inherited.inheritedFlags;
      // A parent's private function is not an override target. UCC permits a
      // distinct child declaration with the same name and no SuperField link.
      if (!sameClassGlobal && (inheritedAccess & 0x40000)) continue;
      if (inherited.isFinal)
        l.error("Cannot override final function: " + fn.name);
      if (fn.type != inherited.type || fn.isStatic != inherited.isStatic ||
          fn.params.size() != inherited.params.size())
        l.error("Override signature mismatch: " + fn.name);
      if (!sameClassGlobal &&
          ((fn.flags & 0x40000) ||
           ((fn.flags & 0x80000) && !(inheritedAccess & 0x80000))))
        l.error("Override narrows access or overrides private function: " +
                fn.name);
      for (std::size_t i = 0; i < fn.params.size(); ++i)
        if (fn.params[i].type != inherited.params[i].type ||
            ((fn.params[i].flags ^ inherited.params[i].flags) & 0x10))
          l.error("Override parameter mismatch: " + fn.name);
      bool outMismatch = false;
      for (std::size_t i = 0; i < fn.params.size(); ++i)
        outMismatch |=
            ((fn.params[i].flags ^ inherited.params[i].flags) & 0x100) != 0;
      if (outMismatch) {
        // Some shipped packages retain a SuperField while changing out
        // parameters. Accept only the exact pair attested by a reference.
        bool verified = false;
        auto canonical = [&](Type type) {
          auto name = type.identity();
          for (const auto& prefix :
               {std::string("@local."), std::string("@enum."),
                std::string("@struct.")})
            if (name.rfind(prefix, 0) == 0)
              name = lower(packageName) + "." + name.substr(prefix.size());
          return std::make_pair(type.kind, name);
        };
        for (const auto& reference : target.packages) {
          if (!reference.referenceOnly ||
              lower(reference.name) != lower(packageName))
            continue;
          std::string path = c.name;
          for (const auto& state : c.states)
            if (state.id == fn.state) path += "." + state.name;
          auto found = reference.exports.find(lower(path + "." + fn.name));
          if (found == reference.exports.end() || found->second.size() != 1)
            continue;
          int id = found->second.front();
          const auto& p = reference.data;
          if (p.kind(id) != "Function") continue;
          int super = p.exports[id - 1].super;
          auto expected =
              canonical(inherited.owner).second + "." + lower(inherited.name);
          auto actual = lower(p.ref(super));
          if (super > 0) actual = lower(reference.name) + "." + actual;
          if (actual != expected) continue;
          auto resolvedSuper = target.resolve(
              {std::size_t(&reference - target.packages.data()), super});
          if (!resolvedSuper) continue;
          const auto& superPackage =
              target.packages[resolvedSuper->package].data;
          if (superPackage.kind(resolvedSuper->id) != "Function") continue;
          auto matches = [&](const Package& package, int member,
                             const Function& declaration) {
            auto info = functionInfo(package, member);
            if (info.parameters.size() != declaration.params.size() ||
                bool(info.result) != (declaration.type != Type::Void))
              return false;
            if (info.result && canonical(importedType(package, *info.result)) !=
                                   canonical(declaration.type))
              return false;
            for (std::size_t i = 0; i < info.parameters.size(); ++i)
              if (info.parameters[i].dimension != 1 ||
                  declaration.params[i].type.isArray() ||
                  canonical(importedType(package, info.parameters[i])) !=
                      canonical(declaration.params[i].type) ||
                  ((info.parameters[i].flags ^ declaration.params[i].flags) &
                   0xb10))
                return false;
            return true;
          };
          verified = matches(p, id, fn) &&
                     matches(superPackage, resolvedSuper->id, inherited);
        }
        if (!verified) l.error("Override parameter mismatch: " + fn.name);
      }
      fn.super = inherited.id;
      fn.inheritedFlags = (inherited.inheritedFlags | inherited.flags) & 0xc0;
    }
    // Resolve bodies only after all source classes have stable declarations.
    for (auto& fn : c.funcs) fn.inheritedFlags |= fn.flags & 0x800;
    std::set<int> replicated;
    for (auto& rule : c.replication)
      for (const auto& name : rule.names) {
        int id = 0;
        for (auto& field : c.vars)
          if (lower(field.name) == lower(name)) {
            id = field.id;
            field.flags |= 0x20;
          }
        for (auto& fn : c.funcs)
          if (!fn.state && lower(fn.name) == lower(name)) {
            if (id || fn.super || fn.isStatic || fn.delegateStorage)
              l.error("Invalid replicated function: " + name);
            id = fn.id;
            fn.flags |= 0x40 | (rule.reliable ? 0x80 : 0);
          }
        if (!id) l.error("Replication requires a declared member: " + name);
        if (!replicated.insert(id).second)
          l.error("Duplicate replication member: " + name);
        rule.members.push_back(id);
      }
    // Replication declarations are applied after override linking. Propagate
    // their flags to state overrides regardless of declaration order.
    bool changedNetworkFlags;
    do {
      changedNetworkFlags = false;
      for (auto& fn : c.funcs)
        for (const auto& base : c.funcs)
          if (fn.super == base.id) {
            auto flags =
                fn.inheritedFlags | ((base.flags | base.inheritedFlags) & 0xc0);
            changedNetworkFlags |= flags != fn.inheritedFlags;
            fn.inheritedFlags = flags;
          }
    } while (changedNetworkFlags);
    c.probe =
        c.parent ? c.parent->probe
        : c.base.id
            ? stateInfo(target.packages[c.base.package].data, c.base.id).probe
            : 0;
    for (const auto& fn : c.funcs) {
      auto bit = probeBit(fn.name);
      if (!fn.state && fn.defined && bit >= 0)
        c.probe |= std::uint64_t(1) << bit;
    }
    std::set<int> linkedStates;
    std::function<void(State&)> linkState = [&](State& state) {
      if (!linkedStates.insert(state.id).second) return;
      state.probe = 0;
      if (state.externalSuper) {
        auto base =
            stateInfo(target.packages[state.externalSuper->package].data,
                      state.externalSuper->id);
        state.probe |= base.probe;
      } else if (state.super) {
        for (auto& local : c.states)
          if (local.id == state.super) linkState(local);
        auto base = sourceState(state.super);
        state.probe |= base->probe;
      }
      for (const auto& fn : c.funcs)
        if (fn.state == state.id) {
          auto bit = probeBit(fn.name);
          if (bit < 0) continue;
          if (fn.defined) {
            state.probe |= std::uint64_t(1) << bit;
            state.ignore |= std::uint64_t(1) << bit;
          } else {
            state.probe &= ~(std::uint64_t(1) << bit);
            state.ignore &= ~(std::uint64_t(1) << bit);
          }
        }
      state.probe &= state.ignore;
    };
    for (auto& state : c.states) linkState(state);
    return std::move(c);
  }
  void resolveBodies() {
    cacheConstants = true;
    Function replicationContext;
    replicationContext.owner = *classType(c.name);
    f = &replicationContext;
    for (auto& rule : c.replication) {
      l = rule.start;
      rule.condition = expression();
      condition(rule.condition);
      l.expect(")");
    }
    for (auto& fn : c.funcs) {
      if (!fn.defined) continue;
      if (progress) progress("function " + c.name + "." + fn.name);
      f = &fn;
      l = *fn.bodyStart;
      try {
        while (!l.accept("}")) fn.body.push_back(statement());
      } catch (const std::exception& error) {
        l.error("In " + c.name + "." + fn.name + ": " + error.what());
      }
      fn.bodyStart.reset();
    }
    for (auto& state : c.states) {
      f = &state.script;
      if (f->bodyStart) {
        l = *f->bodyStart;
        while (!l.accept("}")) f->body.push_back(statement());
      }
    }
    f = nullptr;
  }
};
struct Emitter {
  std::vector<std::pair<std::string, std::uint32_t>> labels;
  Bytes bytes;
  std::uint32_t pc = 0;
  Package* package = nullptr;
  std::function<int(Type)> typeReference;
  std::function<int(Type)> typeSize;
  struct Loop {
    bool isSwitch = false;
    std::vector<std::size_t> breaks, continues;
    bool iterator = false;
  };
  std::vector<Loop> loops;
  void byte(std::uint8_t b) {
    bytes.push_back(b);
    ++pc;
  }
  void reference(int ref) {
    index(bytes, ref);
    pc += 4;
  }
  void expr(const Expr& e) {
    if (e.context) {
      byte(e.context->type.kind == Type::Class ? 0x12 : 0x19);
      expr(*e.context);
      auto skip = bytes.size();
      u16(bytes, 0);
      pc += 2;
      int size = e.type.isDynamicArray() || e.type.element() == Type::String ||
                         e.type == Type::Void
                     ? 0
                     : typeSize(e.type);
      if (size > 255)
        throw std::runtime_error("Context value exceeds 255-byte target limit");
      byte(std::uint8_t(size));
      auto start = pc;
      auto value = e;
      value.context.reset();
      expr(value);
      target(skip, pc - start - (e.implicitOuterCall ? 1 : 0));
    } else if (e.objectOp) {
      if (e.objectOp == 0x36 && e.type == Type::Bool) byte(0x2d);
      byte(std::uint8_t(e.objectOp));
      if (e.objectOp == 0x2e || e.objectOp == 0x13) {
        reference(typeReference(e.type));
        expr(e.args.at(0));
      } else if (e.objectOp == 0x20)
        reference(e.ref ? e.ref
                        : typeReference(Type::named(Type::Object, e.text)));
      else if (e.objectOp == 0x11)
        for (auto& arg : e.args) expr(arg);
      else if (e.objectOp == 0x36) {
        reference(e.ref);
        expr(e.args.at(0));
      } else if (e.objectOp == 0x32 || e.objectOp == 0x33) {
        reference(typeReference(e.args.at(0).type));
        for (auto& arg : e.args) expr(arg);
      } else if (e.objectOp == 0x22 || e.objectOp == 0x23) {
        for (auto& arg : e.args) {
          u32(bytes, std::uint32_t(arg.value));
          pc += 4;
        }
      } else if (e.objectOp == 0x44)
        reference(package->intern(e.text));
      else if (e.objectOp == 0x43) {
        reference(e.ref);
        reference(package->intern(e.callName));
        for (auto& arg : e.args) expr(arg);
        byte(0x16);
      } else if (e.objectOp == 0x38) {
        reference(package->intern(e.callName));
        for (auto& arg : e.args) expr(arg);
        byte(0x16);
      }
    } else if (e.arrayOp == 0x37 || e.arrayOp == 0x40 || e.arrayOp == 0x41 ||
               e.arrayOp == 0x47) {
      byte(std::uint8_t(e.arrayOp));
      for (auto& arg : e.args) expr(arg);
    } else if (e.indexed) {
      byte(e.arrayOp == 0x10 ? 0x10 : 0x1a);
      expr(e.args.at(0));
      expr(e.args.at(1));
    } else if (e.cast >= 0) {
      byte(0x39);
      byte(std::uint8_t(e.cast));
      expr(e.args.at(0));
    } else if (e.call) {
      if (e.finalCall) {
        byte(0x1c);
        reference(e.call);
      } else {
        byte(0x1b);
        reference(package->intern(e.callName));
      }
      for (auto& arg : e.args) expr(arg);
      byte(0x16);
    } else if (e.native) {
      if (e.native < 256 && e.native >= 112)
        byte(std::uint8_t(e.native));
      else {
        byte(std::uint8_t(0x60 + (e.native >> 8)));
        byte(std::uint8_t(e.native));
      }
      for (std::size_t i = 0; i < e.args.size(); ++i) {
        if (i < e.skipArgs.size() && e.skipArgs[i]) {
          auto skip = jump(0x18);
          auto start = pc;
          expr(e.args[i]);
          target(skip, pc - start + 1);
        } else
          expr(e.args[i]);
      }
      byte(0x16);
    } else if (e.ref) {
      if (e.type == Type::Bool) byte(0x2d);
      byte(e.defaultRef ? 2 : e.instance ? 1 : 0);
      reference(e.ref);
    } else if (e.type == Type::Void)
      byte(0x0b);
    else if (e.type == Type::Bool)
      byte(e.value ? 0x27 : 0x28);
    else if (e.type == Type::Byte || e.type.kind == Type::Enum) {
      byte(0x24);
      byte(std::uint8_t(e.value));
    } else if (e.type == Type::Float) {
      byte(0x1e);
      u32(bytes, std::uint32_t(e.value));
      pc += 4;
    } else if (e.type == Type::Name) {
      byte(0x21);
      reference(package->intern(e.text));
    } else if (e.type == Type::String) {
      auto units = utf16(e.text);
      if (std::all_of(units.begin(), units.end(),
                      [](char16_t c) { return c <= 255; })) {
        byte(0x1f);
        for (auto c : units) byte(std::uint8_t(c));
        byte(0);
      } else {
        byte(0x34);
        for (auto c : utf16(e.text)) {
          u16(bytes, std::uint16_t(c));
          pc += 2;
        }
        u16(bytes, 0);
        pc += 2;
      }
    } else if (e.value == 0 || e.value == 1)
      byte(e.value ? 0x26 : 0x25);
    else if (e.type == Type::Int && e.value > 1 && e.value <= 255) {
      byte(0x2c);
      byte(std::uint8_t(e.value));
    } else {
      byte(0x1d);
      u32(bytes, std::uint32_t(e.value));
      pc += 4;
    }
  }
  std::size_t jump(std::uint8_t opcode) {
    byte(opcode);
    auto at = bytes.size();
    u16(bytes, 0);
    pc += 2;
    return at;
  }
  void target(std::size_t at, std::uint32_t dest) {
    if (dest > 65534)
      throw std::runtime_error("Function exceeds 16-bit jump target range");
    bytes[at] = std::uint8_t(dest);
    bytes[at + 1] = std::uint8_t(dest >> 8);
  }
  void statements(const std::vector<Statement>& body) {
    for (auto& s : body) {
      if (s.kind == Statement::Label) {
        for (const auto& old : labels)
          if (lower(old.first) == lower(s.label))
            throw std::runtime_error("Duplicate state label: " + s.label);
        labels.emplace_back(s.label, pc);
      } else if (s.kind == Statement::Stop)
        byte(8);
      else if (s.kind == Statement::Goto) {
        byte(0x0d);
        expr(s.rhs);
      } else if (s.kind == Statement::Return) {
        for (const auto& loop : loops)
          if (loop.iterator) byte(0x30);
        byte(4);
        expr(s.rhs);
      } else if (s.kind == Statement::Assert) {
        byte(9);
        byte(std::uint8_t(s.line));
        byte(std::uint8_t(s.line >> 8));
        expr(s.rhs);
      } else if (s.kind == Statement::Evaluate) {
        if (s.lhs.type == Type::String) byte(0x0e);
        expr(s.lhs);
      } else if (s.kind == Statement::Break || s.kind == Statement::Continue) {
        auto at = jump(6);
        if (s.kind == Statement::Break)
          loops.back().breaks.push_back(at);
        else {
          auto loop =
              std::find_if(loops.rbegin(), loops.rend(),
                           [](const Loop& item) { return !item.isSwitch; });
          if (loop == loops.rend())
            throw std::runtime_error("Missing continue loop");
          loop->continues.push_back(at);
        }
      } else if (s.kind == Statement::Assign) {
        byte(s.lhs.type.kind == Type::Delegate ? 0x45
             : s.lhs.type == Type::Bool        ? 0x14
                                               : 0x0f);
        expr(s.lhs);
        expr(s.rhs);
      } else if (s.kind == Statement::If) {
        auto branch = jump(7);
        expr(s.rhs);
        statements(s.body);
        if (s.alternative.empty())
          target(branch, pc);
        else {
          auto done = jump(6);
          target(branch, pc);
          statements(s.alternative);
          target(done, pc);
        }
      } else if (s.kind == Statement::Foreach) {
        byte(0x2f);
        expr(s.rhs);
        auto end = bytes.size();
        byte(0);
        byte(0);
        loops.push_back({});
        loops.back().iterator = true;
        statements(s.body);
        auto next = pc;
        byte(0x31);
        target(end, pc);
        for (auto at : loops.back().breaks) target(at, pc);
        for (auto at : loops.back().continues) target(at, next);
        byte(0x30);
        loops.pop_back();
      } else if (s.kind == Statement::Switch) {
        byte(5);
        byte(s.rhs.type.isInt64()                                        ? 8
             : s.rhs.type == Type::String                                ? 0
             : s.rhs.type == Type::Byte || s.rhs.type.kind == Type::Enum ? 1
                                                                         : 4);
        expr(s.rhs);
        loops.push_back({true});
        std::optional<std::size_t> previous;
        bool hasDefault = false;
        for (auto& arm : s.body) {
          if (previous) target(*previous, pc);
          auto next = jump(0x0a);
          if (arm.rhs.type == Type::Void) {
            bytes[next] = bytes[next + 1] = 0xff;
            previous.reset();
            hasDefault = true;
          } else {
            previous = next;
            expr(arm.rhs);
          }
          statements(arm.body);
        }
        if (!hasDefault) {
          if (previous) target(*previous, pc);
          auto end = jump(0x0a);
          bytes[end] = bytes[end + 1] = 0xff;
        }
        for (auto at : loops.back().breaks) target(at, pc);
        loops.pop_back();
      } else {
        statements(s.initializer);
        auto start = pc;
        auto end = jump(7);
        expr(s.rhs);
        loops.push_back({});
        statements(s.body);
        auto step = pc;
        statements(s.increment);
        auto back = jump(6);
        target(back, start);
        target(end, pc);
        for (auto at : loops.back().breaks) target(at, pc);
        for (auto at : loops.back().continues)
          target(at, s.kind == Statement::For ? step : start);
        loops.pop_back();
      }
    }
  }
};
Bytes structure(int super, int next, int children, int name,
                const Emitter& code, bool cls) {
  Bytes b;
  if (!cls) index(b, 0);
  index(b, super);
  index(b, next);
  index(b, 0);
  index(b, children);
  index(b, name);
  index(b, 0);
  u32(b, 0xffffffff);
  u32(b, 0xffffffff);
  u32(b, code.pc);
  b.insert(b.end(), code.bytes.begin(), code.bytes.end());
  return b;
}
void tagData(Bytes&, int, int, const Bytes&, int, int structName = -1);
Bytes scalarData(const Value& v, Package& package, int& kind,
                 const std::function<int(Type)>& typeReference) {
  Bytes data;
  if (v.type.isDynamicArray()) {
    kind = 9;
    index(data, int(v.elements.size()));
    for (auto& element : v.elements) {
      int elementKind = 0;
      auto bytes = scalarData(element, package, elementKind, typeReference);
      data.insert(data.end(), bytes.begin(), bytes.end());
    }
    return data;
  }
  switch (v.type.kind) {
    case Type::Struct: {
      kind = 10;
      auto id = v.type.identity();
      if (id == "core.object.vector" || id == "core.object.rotator" ||
          id == "core.object.color") {
        auto fields = id == "core.object.vector"
                          ? std::vector<std::string>{"X", "Y", "Z"}
                      : id == "core.object.rotator"
                          ? std::vector<std::string>{"Pitch", "Yaw", "Roll"}
                          : std::vector<std::string>{"B", "G", "R", "A"};
        for (auto& name : fields) {
          auto found = std::find_if(v.members.begin(), v.members.end(),
                                    [&](const auto& member) {
                                      return lower(member.first) == lower(name);
                                    });
          auto value = found == v.members.end() ? 0 : found->second.value;
          if (id == "core.object.color")
            data.push_back(std::uint8_t(value));
          else
            u32(data, std::uint32_t(value));
        }
      } else {
        for (auto& member : v.members) {
          auto name = member.first;
          auto pos = name.find('(');
          int subscript =
              pos == name.npos ? -1 : std::stoi(name.substr(pos + 1));
          if (pos != name.npos) name.resize(pos);
          if (member.second.type == Type::Bool) {
            index(data, package.intern(name));
            data.push_back(member.second.value ? 0x83 : 3);
          } else {
            int fieldKind = 0;
            auto bytes =
                scalarData(member.second, package, fieldKind, typeReference);
            tagData(data, package.intern(name), fieldKind, bytes, subscript,
                    fieldKind == 10 ? package.intern(member.second.text) : -1);
          }
        }
        index(data, package.intern("None"));
      }
      break;
    }
    case Type::Byte:
    case Type::Enum:
    case Type::Bool:
      kind = 1;
      data.push_back(std::uint8_t(v.value));
      break;
    case Type::Int:
      kind = 2;
      u32(data, std::uint32_t(v.value));
      break;
    case Type::Float:
      kind = 4;
      u32(data, std::uint32_t(v.value));
      break;
    case Type::Name:
      kind = 6;
      index(data, package.intern(v.text));
      break;
    case Type::Object:
      kind = 5;
      index(data, v.value);
      break;
    case Type::Class:
      kind = 5;
      index(data, v.text.empty()
                      ? v.value
                      : typeReference(Type::named(Type::Object, v.text)));
      break;
    case Type::String:
      kind = 13;
      string(data, v.text);
      break;
    default:
      throw std::runtime_error("Unsupported default tag type");
  }
  return data;
}
void tagData(Bytes& out, int name, int kind, const Bytes& data, int subscript,
             int structName) {
  index(out, name);
  unsigned code = data.size() == 1      ? 0
                  : data.size() == 2    ? 1
                  : data.size() == 4    ? 2
                  : data.size() == 12   ? 3
                  : data.size() == 16   ? 4
                  : data.size() < 256   ? 5
                  : data.size() < 65536 ? 6
                                        : 7;
  out.push_back(std::uint8_t(kind | (code << 4) | (subscript >= 0 ? 128 : 0)));
  if (kind == 10) {
    if (structName < 0) throw std::runtime_error("Missing struct default name");
    index(out, structName);
  }
  if (code == 5) out.push_back(std::uint8_t(data.size()));
  if (code == 6) u16(out, std::uint16_t(data.size()));
  if (code == 7) u32(out, std::uint32_t(data.size()));
  if (subscript >= 0) {
    if (subscript < 128)
      out.push_back(std::uint8_t(subscript));
    else if (subscript < 16384) {
      out.push_back(std::uint8_t(128 | (subscript >> 8)));
      out.push_back(std::uint8_t(subscript));
    } else {
      out.push_back(std::uint8_t(192 | (subscript >> 24)));
      out.push_back(std::uint8_t(subscript >> 16));
      out.push_back(std::uint8_t(subscript >> 8));
      out.push_back(std::uint8_t(subscript));
    }
  }
  out.insert(out.end(), data.begin(), data.end());
}
void defaultTag(Bytes& out, int name, const Value& v, Package& package,
                int subscript, const std::function<int(Type)>& typeReference) {
  if (v.type == Type::Bool) {
    index(out, name);
    out.push_back(v.value ? 0x83 : 3);
    return;
  }
  int kind = 0;
  auto data = scalarData(v, package, kind, typeReference);
  tagData(out, name, kind, data, subscript,
          kind == 10 ? package.intern(v.text) : -1);
}
}  // namespace
Package compileObjects(
    const std::vector<std::pair<std::string, Bytes>>& sources,
    const TargetModel& target, const std::string& packageName,
    const std::function<void(const std::string&)>& progress) {
  const TargetPackage* reference = nullptr;
  for (const auto& package : target.packages)
    if (package.referenceOnly && lower(package.name) == lower(packageName)) {
      if (reference)
        throw std::runtime_error("Ambiguous object package reference");
      reference = &package;
    }
  if (!reference || sources.empty())
    throw std::runtime_error(
        "Object compilation requires sources and a self reference");
  const auto& original = reference->data;
  if (original.version != 133 || original.licensee != 40)
    throw std::runtime_error("Object reconstruction requires profile 133/40");
  auto coreObject = target.findClass("Core.Object");
  if (!coreObject || coreObject->id <= 0)
    throw std::runtime_error("Missing serialized Core.Object");
  const auto& core = target.packages[coreObject->package].data;
  if (core.version != original.version || core.licensee != original.licensee)
    throw std::runtime_error("Object reference and Core profiles differ");
  auto operators = nativeOperators(core, coreObject->id);
  Package p;
  p.indexNames(true);
  p.version = original.version;
  p.licensee = original.licensee;
  p.flags = 1;
  p.header.resize(64, 0);
  p.header[52] = 1;
  p.intern("None");
  TargetImports imports(target, p, packageName);
  struct ObjectSource {
    std::string name, path, outer;
    Symbol type;
    int referenceId;
    Lexer body;
  };
  std::vector<ObjectSource> objects;
  std::map<std::string, int> ids;
  std::set<int> declared;
  for (const auto& source : sources) {
    Lexer l{source.first, sourceText(source.second)};
    l.next();
    auto quoted = [&]() {
      if (l.token.quote != '"')
        l.error("Expected a double-quoted object name/path");
      auto text = l.token.text;
      l.next();
      return text;
    };
    while (!l.end()) {
      l.expect("object");
      l.expect("class");
      l.expect("=");
      auto className = l.identifier();
      while (l.accept(".")) className += "." + l.identifier();
      auto type = target.findClass(className);
      if (!type || type->id <= 0 ||
          target.packages[type->package].referenceOnly)
        l.error("Object class requires a loaded defining dependency");
      l.expect("name");
      l.expect("=");
      auto name = quoted();
      if (name.empty() || name.size() > 63 || name.find('.') != name.npos ||
          lower(name) == "none")
        l.error("Invalid object declaration name");
      l.expect("outer");
      l.expect("=");
      auto outer = quoted();
      auto path = lower(outer) == "none" ? name : outer + "." + name;
      auto found = reference->exports.find(lower(path));
      int referenceId = 0;
      if (found != reference->exports.end())
        for (int id : found->second) {
          const auto& e = original.exports[id - 1];
          if (lower(original.ref(e.type)) != lower(target.identity(*type)))
            continue;
          if (referenceId) l.error("Ambiguous typed object declaration");
          referenceId = id;
        }
      if (!referenceId)
        l.error("Object declaration is absent from reference: " + path);
      const auto& e = original.exports[referenceId - 1];
      if (e.super || e.type >= 0 || (e.flags & ~0x000f0004u) ||
          lower(original.ref(e.outer)) != lower(outer))
        l.error("Unsupported/mismatched object reference header");
      Reader raw{e.payload};
      defaultTags(original, raw);
      if (raw.pos != raw.data.size())
        l.error("Object reference contains native trailing bytes");
      if (!declared.insert(referenceId).second ||
          !ids.emplace(lower(path), int(objects.size()) + 1).second)
        l.error("Duplicate object declaration");
      l.expect("{");
      objects.push_back({name, path, outer, *type, referenceId, l});
      int depth = 1;
      while (depth) {
        if (l.end()) l.error("Unterminated object properties");
        if (l.is("{")) ++depth;
        if (l.is("}")) --depth;
        l.next();
      }
    }
  }
  if (declared.size() != original.exports.size())
    throw std::runtime_error(
        "Every reference export requires an object declaration");
  p.exports.resize(objects.size());
  for (const auto& item : ids) imports.bindLocal(item.first, item.second);
  std::map<std::string, std::string> sourceBases;
  std::map<Type, Structure> structures;
  std::map<Type, Enumeration> enumerations;
  auto typeReference = [&](Type type) {
    auto symbol = target.findClass(type.identity());
    if (!symbol)
      throw std::runtime_error("Unresolved object property type: " +
                               type.identity());
    return imports.symbol(*symbol);
  };
  for (std::size_t i = 0; i < objects.size(); ++i) {
    const auto& source = objects[i];
    if (progress) progress("object " + source.path);
    int outer = 0;
    if (lower(source.outer) != "none") {
      auto found = ids.find(lower(source.outer));
      if (found == ids.end())
        throw std::runtime_error("Missing declared object outer: " +
                                 source.outer);
      outer = found->second;
    }
    Parser parser(source.body.file, {}, int(objects.size()) + 1, operators,
                  target, imports, sourceBases, structures, enumerations);
    parser.l = source.body;
    parser.packageName = packageName;
    parser.c.name = source.path;
    parser.c.id = int(i) + 1;
    parser.c.base = source.type;
    while (!parser.l.accept("}")) parser.readDefaultAssignment();
    Bytes bytes;
    for (const auto& value : parser.c.defaults) {
      const auto& field = parser.c.defaultFields.at(value.first);
      defaultTag(bytes, p.intern(field.name), value.second, p,
                 parser.c.defaultIndices.at(value.first), typeReference);
    }
    for (const auto& field : parser.c.defaultFields)
      if (field.second.type.isDynamicArray()) {
        const auto& values = parser.c.arrayDefaults.at(field.first);
        Bytes data;
        index(data, int(values.size()));
        for (const auto& value : values) {
          int kind = 0;
          auto encoded = scalarData(value, p, kind, typeReference);
          data.insert(data.end(), encoded.begin(), encoded.end());
        }
        tagData(bytes, p.intern(field.second.name), 9, data, -1);
      }
    index(bytes, p.intern("None"));
    p.exports[i] = {imports.symbol(source.type),
                    0,
                    outer,
                    p.intern(source.name),
                    original.exports[source.referenceId - 1].flags,
                    std::move(bytes)};
  }
  std::uint32_t hash = 2166136261u;
  for (const auto& source : sources)
    for (auto byte : source.second) {
      hash ^= byte;
      hash *= 16777619u;
    }
  for (int j = 0; j < 4; ++j) {
    hash ^= std::uint32_t(j);
    hash *= 16777619u;
    for (int i = 0; i < 4; ++i)
      p.header[36 + j * 4 + i] = std::uint8_t(hash >> (8 * i));
  }
  p.validate();
  return p;
}
Package compile(const std::vector<std::pair<std::string, Bytes>>& sources,
                const Package& core) {
  TargetModel target;
  target.add("Core", core);
  return compile(sources, target);
}
Package compile(const std::vector<std::pair<std::string, Bytes>>& sources,
                const TargetModel& target, const std::string& packageName,
                const std::function<void(const std::string&)>& progress) {
  auto object = target.findClass("Core.Object");
  if (!object)
    throw std::runtime_error("Dependency does not define Core.Object");
  auto& core = target.packages[object->package].data;
  if (sources.empty()) throw std::runtime_error("No source files");
  auto operators = nativeOperators(core, object->id);
  Package p;
  p.indexNames(true);
  p.version = core.version;
  p.licensee = core.licensee;
  // A reconstructed package may use a different supported container profile
  // than its Core dependency. Emit its own format, not a header-only
  // conversion.
  for (const auto& package : target.packages)
    if (package.referenceOnly && lower(package.name) == lower(packageName)) {
      p.version = package.data.version;
      p.licensee = package.data.licensee;
    }
  p.flags = 1;
  p.header.resize(64, 0);
  p.header[52] = 1;
  p.intern("None");
  TargetImports imports(target, p, packageName);
  int objectRef = lower(packageName) == "core" ? 0 : imports.symbol(*object),
      functionRef = imports.nativeClass("Core", "Function");
  std::map<Type, int> propertyRefs;
  for (auto t : {Type::Int, Type::Bool, Type::Byte, Type::Float, Type::String,
                 Type::Name, Type::Pointer})
    propertyRefs[t] = imports.nativeClass("Core", propertyKind(t));
  int arrayRef = imports.nativeClass("Core", "ArrayProperty");
  int objectPropertyRef = imports.nativeClass("Core", "ObjectProperty");
  int classPropertyRef = imports.nativeClass("Core", "ClassProperty");
  std::vector<Class> classes;
  classes.reserve(sources.size());
  int count = 1;
  std::map<std::string, std::size_t> declarations;
  std::map<std::string, std::string> sourceBases;
  std::map<Type, Structure> structures;
  std::map<Type, Enumeration> enumerations;
  std::vector<std::string> bases;
  for (std::size_t i = 0; i < sources.size(); ++i) {
    auto& source = sources[i];
    if (progress) progress("prescan " + source.first);
    Lexer header{source.first, sourceText(source.second)};
    header.next();
    header.expect("class");
    auto name = header.identifier();
    if (!declarations.emplace(lower(name), i).second)
      header.error("Duplicate class: " + name);
    std::string base;
    if (header.accept("extends")) {
      base = header.identifier();
      while (header.accept(".")) base += "." + header.identifier();
    } else if (lower(packageName) != "core" || lower(name) != "object") {
      header.error("Only Core.Object may omit extends");
    }
    auto prefix = lower(packageName) + ".";
    if (lower(base).rfind(prefix, 0) == 0) base.erase(0, prefix.size());
    bases.push_back(lower(base));
    sourceBases.emplace(lower(name), lower(base));
    std::vector<std::string> scopes;
    std::string scope;
    while (!header.end()) {
      if (header.accept("struct")) {
        Structure item;
        item.name = header.identifier();
        item.path = scope.empty() ? item.name : scope + "." + item.name;
        item.type = Type::named(Type::Struct,
                                "@struct." + lower(name + "." + item.path));
        if (lower(packageName) == "core" && lower(name) == "object")
          item.type =
              Type::named(Type::Struct, "core.object." + lower(item.path));
        if (!structures.emplace(item.type, item).second)
          header.error("Duplicate struct declaration");
        while (!header.is("{")) {
          if (header.end()) header.error("Unterminated struct declaration");
          header.next();
        }
        header.next();
        scopes.push_back(scope);
        scope = item.path;
        continue;
      }
      if (header.accept("enum")) {
        if (header.token.quote == '\'') {
          header.next();
          continue;
        }
        Enumeration item;
        item.name = header.identifier();
        item.path = scope.empty() ? item.name : scope + "." + item.name;
        item.type =
            Type::named(Type::Enum, "@enum." + lower(name + "." + item.path));
        header.expect("{");
        do {
          item.names.push_back(header.identifier());
        } while (header.accept(",") && !header.is("}"));
        header.expect("}");
        if (!enumerations.emplace(item.type, item).second)
          header.error("Duplicate enum declaration");
        continue;
      }
      if (header.accept("{")) {
        scopes.push_back(scope);
        continue;
      }
      if (header.accept("}")) {
        if (scopes.empty()) header.error("Unmatched declaration brace");
        scope = scopes.back();
        scopes.pop_back();
        continue;
      }
      header.next();
    }
  }
  std::vector<int> linkStatus(sources.size());
  std::vector<const Class*> compiled(sources.size(), nullptr);
  std::function<void(std::size_t)> link = [&](std::size_t i) {
    if (linkStatus[i] == 2) return;
    if (linkStatus[i] == 1)
      throw std::runtime_error("Source inheritance cycle: " + sources[i].first);
    linkStatus[i] = 1;
    const Class* parent = nullptr;
    auto found = declarations.find(bases[i]);
    if (found != declarations.end()) {
      link(found->second);
      parent = compiled[found->second];
    }
    auto& source = sources[i];
    Parser parser(source.first, source.second, count, operators, target,
                  imports, sourceBases, structures, enumerations);
    if (progress) progress("declarations " + source.first);
    parser.sourceParent = parent;
    parser.packageName = packageName;
    classes.push_back(parser.parse());
    count = parser.nextId;
    compiled[i] = &classes.back();
    linkStatus[i] = 2;
  };
  for (std::size_t i = 0; i < sources.size(); ++i) link(i);
  if (lower(packageName) == "core") {
    auto root = declarations.find("object");
    if (root == declarations.end())
      throw std::runtime_error("Core reconstruction requires source Object");
    objectRef = compiled[root->second]->id;
  }
  for (const auto& cls : classes) {
    for (const auto& field : cls.vars)
      imports.bindLocal(cls.name + "." + field.name, field.id);
    for (const auto& item : cls.enums)
      imports.bindLocal(cls.name + "." + item.path, item.id);
    for (const auto& item : cls.constants)
      imports.bindLocal(cls.name + "." + item.name, item.id);
    for (const auto& state : cls.states)
      imports.bindLocal(cls.name + "." + state.name, state.id);
    for (const auto& fn : cls.funcs) {
      std::string owner = cls.name;
      if (fn.state)
        for (const auto& state : cls.states)
          if (state.id == fn.state) owner += "." + state.name;
      imports.bindLocal(owner + "." + fn.name, fn.id);
    }
    for (auto type : cls.structs) {
      const auto& item = structures.at(type);
      auto path = cls.name + "." + item.path;
      imports.bindLocal(path, item.id);
      for (const auto& field : item.fields)
        imports.bindLocal(path + "." + field.name, field.id);
    }
  }
  std::map<std::string, const Class*> sourceClasses;
  std::map<Symbol, std::pair<int, ResourceBody>> resources;
  for (std::size_t i = 0; i < target.packages.size(); ++i) {
    const auto& reference = target.packages[i];
    if (!reference.referenceOnly || lower(reference.name) != lower(packageName))
      continue;
    for (std::size_t j = 0; j < reference.data.exports.size(); ++j) {
      int id = int(j) + 1;
      auto kind = reference.data.kind(id);
      if (kind != "Texture" && kind != "Palette" && kind != "Font" &&
          kind != "Package" && kind != "StaticMesh" && kind != "Cubemap" && kind != "MeshAnimation" && kind != "SkeletalMesh" && kind != "VertMesh" && kind != "Sound" && !materialResource(kind) && !animationNotifyResource(kind))
        continue;
      auto body = decodeResource(reference.data, id);
      imports.bindReference({i, id}, count);
      resources.emplace(Symbol{i, id}, std::make_pair(count++, std::move(body)));
    }
  }
  for (auto& cls : classes) sourceClasses.emplace(lower(cls.name), &cls);
  for (const auto& cls : classes)
    for (const auto& object : cls.objects)
      imports.bindLocal(object.path, object.id);
  for (auto& cls : classes) {
    auto& source = sources[declarations.at(lower(cls.name))];
    if (progress) progress("defaults " + cls.name);
    Parser parser(source.first, source.second, cls.id, operators, target,
                  imports, sourceBases, structures, enumerations);
    parser.sourceParent = cls.parent;
    parser.packageName = packageName;
    parser.sourceClasses = &sourceClasses;
    parser.defaultsEnabled = true;
    cls = parser.parse();
  }
  for (auto& cls : classes)
    for (auto& object : cls.objects) {
      auto& source = sources[declarations.at(lower(cls.name))];
      Parser parser(source.first, source.second, count, operators, target,
                    imports, sourceBases, structures, enumerations);
      parser.sourceClasses = &sourceClasses;
      parser.packageName = packageName;
      parser.c.name = cls.name;
      parser.c.id = cls.id;
      if (object.type.identity().rfind("@local.", 0) == 0) {
        parser.c.parent = sourceClasses.at(object.type.identity().substr(7));
        parser.c.base = parser.c.parent->base;
        parser.c.arrayDefaults = parser.c.parent->arrayDefaults;
      } else {
        auto base = target.findClass(object.type.identity());
        if (!base || base->id <= 0)
          throw std::runtime_error("Unresolved default subobject class");
        parser.c.base = *base;
      }
      parser.l = *object.body;
      while (!parser.l.is("end")) {
        if (parser.l.is("begin"))
          parser.readDefaultObject(object.id, object.path);
        else
          parser.readDefaultAssignment();
      }
      parser.l.expect("end");
      parser.l.expect("object");
      static_cast<DefaultValues&>(object) =
          std::move(static_cast<DefaultValues&>(parser.c));
    }
  for (auto& cls : classes) {
    auto& source = sources[declarations.at(lower(cls.name))];
    Parser parser(source.first, source.second, count, operators, target,
                  imports, sourceBases, structures, enumerations);
    parser.sourceClasses = &sourceClasses;
    parser.c = cls;
    parser.progress = progress;
    parser.packageName = packageName;
    std::set<Type> withinChain;
    auto within = *parser.classType(cls.name);
    while (within != Type::Void && within.identity() != "core.object") {
      if (!withinChain.insert(within).second)
        throw std::runtime_error("Cyclic within classes");
      within = parser.withinOf(within);
    }
    parser.resolveBodies();
    cls = std::move(parser.c);
  }
  auto typeReference = [&](Type t) {
    if (t.kind == Type::Delegate) {
      for (const auto& cls : classes)
        for (const auto& fn : cls.funcs)
          if (!fn.state &&
              t.identity() == "@delegate." + lower(cls.name + "." + fn.name))
            return fn.id;
      auto dot = t.identity().rfind('.');
      auto cls = target.findClass(t.identity().substr(0, dot));
      if (cls) {
        auto fn = target.member(*cls, t.identity().substr(dot + 1));
        if (fn) return imports.symbol(*fn);
      }
      throw std::runtime_error("Unresolved delegate signature: " +
                               t.identity());
    }
    if (t.kind == Type::Struct) return structures.at(t).id;
    if (t.kind == Type::Enum) {
      for (auto& cls : classes)
        for (auto& item : cls.enums)
          if (item.type == t) return item.id;
      auto dot = t.identity().rfind('.');
      auto owner = target.findClass(t.identity().substr(0, dot));
      if (owner) {
        auto symbol = target.member(*owner, t.identity().substr(dot + 1));
        if (symbol) return imports.symbol(*symbol);
      }
      throw std::runtime_error("Unresolved enum: " + t.identity());
    }
    if (t.identity().rfind("@local.", 0) == 0) {
      auto found = declarations.find(t.identity().substr(7));
      if (found == declarations.end())
        throw std::runtime_error("Unresolved source type");
      return compiled[found->second]->id;
    }
    auto symbol = target.findClass(t.identity());
    if (!symbol)
      throw std::runtime_error("Unresolved object type: " + t.identity());
    return imports.symbol(*symbol);
  };
  // Reproducible bootstrap GUID, derived from sources (not a security hash).
  std::uint32_t hash = 2166136261u;
  for (auto& s : sources)
    for (auto b : s.second) {
      hash ^= b;
      hash *= 16777619u;
    }
  for (int j = 0; j < 4; ++j) {
    hash ^= std::uint32_t(j);
    hash *= 16777619u;
    for (int i = 0; i < 4; ++i)
      p.header[36 + j * 4 + i] = std::uint8_t(hash >> (8 * i));
  }
  p.exports.resize(std::size_t(count - 1));
  std::map<Type, std::pair<int, int>> layouts;
  std::set<Type> checkedStructs, activeStructs;
  std::function<void(Type)> validateStruct = [&](Type type) {
    if (checkedStructs.count(type)) return;
    if (!activeStructs.insert(type).second)
      throw std::runtime_error("Recursive struct type graph");
    const auto& item = structures.at(type);
    if (!item.complete || !item.id)
      throw std::runtime_error("Unresolved source struct");
    if (item.base != Type::Void) validateStruct(item.base);
    for (auto& field : item.fields) {
      if (field.type.kind == Type::Struct) validateStruct(field.type.element());
      for (auto base = item.base; base != Type::Void;
           base = structures.at(base).base)
        for (auto& inherited : structures.at(base).fields)
          if (lower(inherited.name) == lower(field.name))
            throw std::runtime_error("Inherited struct field redeclaration");
    }
    activeStructs.erase(type);
    checkedStructs.insert(type);
  };
  for (auto& item : structures) validateStruct(item.first);
  std::set<Type> layoutActive;
  std::function<std::pair<int, int>(Type)> layout =
      [&](Type t) -> std::pair<int, int> {
    if (t.isDynamicArray()) return {12, 4};
    if (t.isArray()) {
      auto value = layout(t.element());
      value.first *= t.dimension();
      return value;
    }
    if (t.kind == Type::Byte || t.kind == Type::Enum) return {1, 1};
    if (t.kind == Type::String) return {12, 4};
    if (t.kind == Type::Delegate) return {8, 4};
    if (t.kind != Type::Struct) return {4, 4};
    if (layouts.count(t)) return layouts.at(t);
    if (!layoutActive.insert(t).second)
      throw std::runtime_error("Recursive struct layout");
    auto& item = structures.at(t);
    int size = 0, alignment = 4, boolBits = 0;
    if (item.base != Type::Void) {
      auto base = layout(item.base);
      size = base.first;
      alignment = base.second;
    }
    for (auto& field : item.fields) {
      auto part = layout(field.type);
      alignment = std::max(alignment, part.second);
      if (field.type == Type::Bool && boolBits && boolBits < 32) {
        ++boolBits;
        continue;
      }
      size = (size + part.second - 1) / part.second * part.second;
      size += part.first;
      boolBits = field.type == Type::Bool ? 1 : 0;
      if (size > 65535)
        throw std::runtime_error("Struct layout exceeds supported size");
    }
    size = (size + alignment - 1) / alignment * alignment;
    layoutActive.erase(t);
    return layouts[t] = {size, alignment};
  };
  std::function<bool(Type)> needsConstructor = [&](Type t) {
    if (t.kind == Type::Delegate) return true;
    if (t.isDynamicArray() || t.element() == Type::String) return true;
    if (t.kind == Type::Object) {
      if (t.identity().rfind("@local.", 0) == 0)
        return (sourceClasses.at(t.identity().substr(7))->flags & 0x200000) !=
               0;
      auto cls = target.findClass(t.identity());
      return cls && cls->id > 0 &&
             (classInfo(target.packages[cls->package].data, cls->id).flags &
              0x200000) != 0;
    }
    if (t.kind != Type::Struct) return false;
    auto& item = structures.at(t.element());
    if (item.base != Type::Void && needsConstructor(item.base)) return true;
    for (auto& field : item.fields)
      if (!(field.flags & 0x1000) && (needsConstructor(field.type) ||
                                      (field.flags & 0x4000008) == 0x4000008))
        return true;
    return false;
  };
  for (auto& item : structures) layout(item.first);
  auto save = [&](int id, int kind, int super, int outer,
                  const std::string& name, Bytes bytes) {
    p.exports[id - 1] = {kind,           super,      outer,
                         p.intern(name), 0x000f0004, std::move(bytes)};
  };
  std::function<void(const Variable&, int, int, std::uint32_t)> property;
  std::map<int, std::uint16_t> replicationOffsets;
  property = [&](const Variable& v, int owner, int next, std::uint32_t flags) {
    Bytes b;
    index(b, 0);
    index(b, 0);
    index(b, next);
    u32(b, v.type.dimension());
    if (!(flags & 0x1000) &&
        (needsConstructor(v.type) || (flags & 0x4000008) == 0x4000008))
      flags |= 0x400000;
    u32(b, flags);
    index(b, v.category.empty() ? 0 : p.intern(v.category));
    if (flags & 0x20) u16(b, replicationOffsets.at(v.id));
    if (v.type.isDynamicArray()) {
      index(b, v.inner);
      Variable inner{v.name, v.type.element(), v.inner};
      property(inner, v.id, 0, 0);
    } else if (v.type.element() == Type::Byte)
      index(b, 0);
    else if (v.type.kind == Type::Enum)
      index(b, typeReference(v.type.element()));
    else if (v.type.kind == Type::Struct || v.type.kind == Type::Delegate)
      index(b, typeReference(v.type.element()));
    else if (v.type.kind == Type::Object)
      index(b, typeReference(v.type.element()));
    else if (v.type.kind == Type::Class) {
      index(b, imports.nativeClass("Core", "Class"));
      index(b, typeReference(v.type.element()));
    }
    save(v.id,
         v.type.isDynamicArray()       ? arrayRef
         : v.type.kind == Type::Object ? objectPropertyRef
         : v.type.kind == Type::Class  ? classPropertyRef
         : v.type.kind == Type::Enum   ? propertyRefs.at(Type::Byte)
         : v.type.kind == Type::Delegate
             ? imports.nativeClass("Core", "DelegateProperty")
         : v.type.kind == Type::Struct
             ? imports.nativeClass("Core", "StructProperty")
             : propertyRefs.at(v.type.element()),
         0, owner, v.name, std::move(b));
    if (v.access) p.exports[v.id - 1].flags &= ~4u;
    if (v.access == 1) p.exports[v.id - 1].flags |= 0x80;
  };
  auto emitDefaults = [&](const DefaultValues& values) {
    Bytes bytes;
    for (const auto& item : values.defaults) {
      const auto& field = values.defaultFields.at(item.first);
      defaultTag(bytes, p.intern(field.name), item.second, p,
                 values.defaultIndices.at(item.first), typeReference);
    }
    for (const auto& field : values.defaultFields)
      if (field.second.type.isDynamicArray()) {
        const auto& elements = values.arrayDefaults.at(field.first);
        Bytes data;
        index(data, int(elements.size()));
        for (const auto& value : elements) {
          int kind = 0;
          auto element = scalarData(value, p, kind, typeReference);
          data.insert(data.end(), element.begin(), element.end());
        }
        tagData(bytes, p.intern(field.second.name), 9, data, -1);
      }
    index(bytes, p.intern("None"));
    return bytes;
  };
  for (auto& c : classes) {
    std::vector<int> fields;
    if (progress) progress("emit " + c.name);
    for (auto type : c.structs)
      if (structures.at(type).outer == c.id)
        fields.push_back(structures.at(type).id);
    for (auto& v : c.enums)
      if (v.outer == c.id) fields.push_back(v.id);
    for (auto& v : c.constants) fields.push_back(v.id);
    for (auto& v : c.vars) fields.push_back(v.id);
    for (auto& f : c.funcs)
      if (!f.state) fields.push_back(f.id);
    for (auto& state : c.states) fields.push_back(state.id);
    std::map<int, int> next;
    for (std::size_t i = 0; i < fields.size(); ++i)
      next[fields[i]] = i + 1 < fields.size() ? fields[i + 1] : 0;
    Emitter replicationCode;
    replicationCode.package = &p;
    replicationCode.typeReference = typeReference;
    replicationCode.typeSize = [&](Type t) { return layout(t).first; };
    for (const auto& rule : c.replication) {
      if (replicationCode.pc >= 65535)
        throw std::runtime_error("Replication offset exceeds 16-bit range");
      for (auto id : rule.members)
        replicationOffsets[id] = std::uint16_t(replicationCode.pc);
      replicationCode.expr(rule.condition);
    }
    auto b = structure(c.super, 0, fields.empty() ? 0 : fields[0],
                       p.intern(c.name), replicationCode, true);
    u32(b, std::uint32_t(c.probe));
    u32(b, std::uint32_t(c.probe >> 32));
    b.insert(b.end(), 8, 255);
    u16(b, 0xffff);
    u32(b, 0);
    u32(b, c.flags);
    b.insert(b.end(), 16, 0);
    index(b, 0);
    index(b, 0);
    index(b, c.withinType != Type::Void ? typeReference(c.withinType)
             : c.within                 ? c.within
                                        : objectRef);
    index(b, p.intern(c.config));
    index(b, 0);
    auto defaults = emitDefaults(c);
    b.insert(b.end(), defaults.begin(), defaults.end());
    save(c.id, 0, c.super, 0, c.name, std::move(b));
    for (const auto& object : c.objects) {
      save(object.id, typeReference(object.type), 0, object.outer, object.name,
           emitDefaults(object));
      p.exports[object.id - 1].flags = 0x00070004;
    }
    if (c.nativeClass) p.exports[c.id - 1].flags |= 0x04000000;
    for (auto type : c.structs) {
      auto& item = structures.at(type);
      std::vector<int> members;
      for (auto nested : item.nested)
        members.push_back(structures.at(nested).id);
      for (const auto& enumeration : c.enums)
        if (enumeration.outer == item.id) members.push_back(enumeration.id);
      for (auto& field : item.fields) members.push_back(field.id);
      for (std::size_t i = 0; i < members.size(); ++i)
        next[members[i]] = i + 1 < members.size() ? members[i + 1] : 0;
      int super = item.base == Type::Void ? 0 : typeReference(item.base);
      save(item.id, imports.nativeClass("Core", "Struct"), super, item.outer,
           item.name,
           structure(super, next[item.id], members.empty() ? 0 : members[0],
                     p.intern(item.name), {}, false));
      for (auto& field : item.fields)
        property(field, item.id, next[field.id], field.flags);
    }
    for (auto& item : c.enums) {
      Bytes data;
      index(data, 0);
      index(data, 0);
      index(data, next[item.id]);
      index(data, int(item.names.size()));
      for (auto& name : item.names) index(data, p.intern(name));
      save(item.id, imports.nativeClass("Core", "Enum"), 0, item.outer,
           item.name, std::move(data));
    }
    for (auto& item : c.constants) {
      Bytes data;
      index(data, 0);
      index(data, 0);
      index(data, next[item.id]);
      string(data, item.text);
      save(item.id, imports.nativeClass("Core", "Const"), 0, c.id, item.name,
           std::move(data));
    }
    for (auto& v : c.vars) property(v, c.id, next[v.id], v.flags);
    for (auto& state : c.states) {
      std::vector<int> members;
      for (auto& fn : c.funcs)
        if (fn.state == state.id) members.push_back(fn.id);
      for (std::size_t i = 0; i < members.size(); ++i)
        next[members[i]] = i + 1 < members.size() ? members[i + 1] : 0;
      Emitter e;
      e.package = &p;
      e.typeReference = typeReference;
      e.typeSize = [&](Type t) { return layout(t).first; };
      e.statements(state.script.body);
      e.byte(8);
      std::uint16_t labelOffset = 0xffff;
      if (!e.labels.empty()) {
        while ((e.pc & 3) != 3) e.byte(0x0b);
        e.byte(0x0c);
        if (e.pc > 65534)
          throw std::runtime_error("State label offset exceeds 16-bit range");
        labelOffset = std::uint16_t(e.pc);
        for (auto label = e.labels.rbegin(); label != e.labels.rend();
             ++label) {
          e.reference(p.intern(label->first));
          u32(e.bytes, label->second);
          e.pc += 4;
        }
        e.reference(p.intern("None"));
        u32(e.bytes, 0xffff);
        e.pc += 4;
      }
      auto data = structure(state.super, next[state.id],
                            members.empty() ? 0 : members[0],
                            p.intern(state.name), e, false);
      u32(data, std::uint32_t(state.probe));
      u32(data, std::uint32_t(state.probe >> 32));
      u32(data, std::uint32_t(state.ignore));
      u32(data, std::uint32_t(state.ignore >> 32));
      u16(data, labelOffset);
      u32(data, state.flags);
      save(state.id, imports.nativeClass("Core", "State"), state.super, c.id,
           state.name, std::move(data));
      p.exports[state.id - 1].flags &= ~0x80000u;
    }
    for (auto& f : c.funcs) {
      auto vars = f.params;
      int returnId = f.returnId;
      if (f.type != Type::Void) {
        Variable result{"ReturnValue", f.type, returnId};
        result.inner = f.returnInner;
        vars.push_back(std::move(result));
      }
      vars.insert(vars.end(), f.locals.begin(), f.locals.end());
      for (std::size_t i = 0; i < vars.size(); ++i) {
        auto flags =
            i < f.params.size()
                ? (0x80u | vars[i].flags)
                : (f.type != Type::Void && vars[i].id == returnId ? 0x580u
                                                                  : 0u);
        property(vars[i], f.id, i + 1 < vars.size() ? vars[i + 1].id : 0,
                 flags);
      }
      Emitter e;
      e.package = &p;
      e.typeReference = typeReference;
      e.typeSize = [&](Type t) { return layout(t).first; };
      if (f.flags & 0x400) {
        for (const auto& param : f.params) {
          e.byte(0x29);
          e.reference(param.id);
        }
        e.byte(0x0b);
      } else {
        try {
          e.statements(f.body);
        } catch (const std::exception& error) {
          throw std::runtime_error(c.name + "." + f.name + ": " + error.what());
        }
        e.byte(4);
        e.byte(0x0b);
      }
      auto data = structure(f.super, next[f.id], vars.empty() ? 0 : vars[0].id,
                            p.intern(f.friendly.empty() ? f.name : f.friendly),
                            e, false);
      u16(data, std::uint16_t(f.native));
      data.push_back(std::uint8_t(f.precedence));
      u32(data, (f.flags & 0xe0000 ? 0 : 0x00020000) | f.inheritedFlags |
                    f.flags | (f.defined ? 2 : 0));
      if ((f.flags | f.inheritedFlags) & 0x40)
        u16(data, replicationOffsets.count(f.id) ? replicationOffsets.at(f.id)
                                                 : 0xffff);
      save(f.id, functionRef, f.super, f.state ? f.state : c.id, f.name,
           std::move(data));
    }
  }
  for (const auto& resource : resources) {
    auto symbol = resource.first;
    const auto& original = target.packages[symbol.package].data;
    const auto& source = original.exports[symbol.id - 1];
    auto& output = p.exports[resource.second.first - 1];
    auto reference = [&](int id) {
      return id ? imports.symbol({symbol.package, id}) : 0;
    };
    output.name = p.intern(original.name(source.name));
    output.type = reference(source.type);
    output.outer = reference(source.outer);
    output.super = reference(source.super);
    output.flags = source.flags;
    encodeResource(resource.second.second, p, output, reference);
  }
  // Load-context and standalone export flags have no source declaration
  // spelling. Preserve only these bits from an explicit package reference;
  // access and native flags continue to be derived from the new declarations.
  auto sameFunctionContract = [&](const Package& old, int oldId, int newId) {
    auto a = functionInfo(old, oldId), b = functionInfo(p, newId);
    if (a.native != b.native || (a.flags & 0x3c0d) != (b.flags & 0x3c0d) ||
        a.parameters.size() != b.parameters.size() ||
        bool(a.result) != bool(b.result))
      return false;
    auto typeMatches = [&](const PropertyInfo& x, const PropertyInfo& y) {
      return x.kind == y.kind && x.dimension == y.dimension &&
             lower(old.ref(x.reference)) == lower(p.ref(y.reference)) &&
             lower(old.ref(x.metaclass)) == lower(p.ref(y.metaclass)) &&
             ((x.flags ^ y.flags) & 0xb10) == 0;
    };
    if (a.result && !typeMatches(*a.result, *b.result)) return false;
    for (std::size_t i = 0; i < a.parameters.size(); ++i)
      if (!typeMatches(a.parameters[i], b.parameters[i])) return false;
    return true;
  };
  std::set<int> defaultObjectIds;
  for (const auto& cls : classes)
    for (const auto& object : cls.objects) defaultObjectIds.insert(object.id);
  for (const auto& reference : target.packages) {
    if (!reference.referenceOnly || lower(reference.name) != lower(packageName))
      continue;
    for (std::size_t i = 0; i < p.exports.size(); ++i) {
      auto found = reference.exports.find(lower(p.ref(int(i) + 1)));
      if (found == reference.exports.end()) continue;
      std::optional<int> match;
      for (int id : found->second)
        if (reference.data.kind(id) == p.kind(int(i) + 1)) {
          if (match)
            throw std::runtime_error("Ambiguous reference export flags");
          match = id;
        }
      if (match) {
        auto& output = p.exports[i];
        const auto& original = reference.data.exports[*match - 1];
        if (defaultObjectIds.count(int(i) + 1)) {
          if (original.super ||
              lower(reference.data.ref(original.type)) !=
                  lower(p.ref(output.type)) ||
              lower(reference.data.ref(original.outer)) !=
                  lower(p.ref(output.outer)))
            throw std::runtime_error(
                "Default subobject reference identity mismatch");
          // Native editor profiles differ in public visibility of default
          // instances. Retain only that attested bit, not arbitrary flags.
          output.flags = (output.flags & ~4u) | (original.flags & 4u);
        }
        if (p.kind(int(i) + 1) == "ObjectProperty" && output.outer > 0 &&
            original.outer > 0 && p.kind(output.outer) == "Class" &&
            reference.data.kind(original.outer) == "Class") {
          auto oldProperty = propertyInfo(reference.data, *match);
          auto newProperty = propertyInfo(p, int(i) + 1);
          if ((oldProperty.flags ^ newProperty.flags) == 0x400000 &&
              (oldProperty.flags & 0x4000008) == 0x4000008) {
            // Exported editinline fields occur with either constructor-link
            // form in clients. Preserve only the attested bit, after checking
            // the property identity and serialized contract.
            Reader oldReader{original.payload}, newReader{output.payload};
            for (int field = 0; field < 3; ++field) {
              oldReader.index();
              newReader.index();
            }
            oldReader.u32();
            newReader.u32();
            auto flagOffset = newReader.pos;
            oldReader.u32();
            newReader.u32();
            if (oldProperty.dimension != newProperty.dimension ||
                oldProperty.replicationOffset != newProperty.replicationOffset ||
                lower(reference.data.ref(oldProperty.reference)) !=
                    lower(p.ref(newProperty.reference)) ||
                lower(reference.data.ref(original.outer)) !=
                    lower(p.ref(output.outer)) ||
                lower(reference.data.name(oldReader.index())) !=
                    lower(p.name(newReader.index())))
              throw std::runtime_error(
                  "Inline constructor-link reference contract mismatch");
            for (int byte = 0; byte < 4; ++byte)
              output.payload[flagOffset + byte] =
                  std::uint8_t(oldProperty.flags >> (byte * 8));
          }
        }
        if (p.kind(int(i) + 1) == "ObjectProperty" && output.outer > 0 &&
            original.outer > 0 && p.kind(output.outer) == "Function" &&
            reference.data.kind(original.outer) == "Function") {
          auto oldProperty = propertyInfo(reference.data, *match);
          auto newProperty = propertyInfo(p, int(i) + 1);
          if ((oldProperty.flags ^ newProperty.flags) == 0x400000 &&
              (oldProperty.flags & 0x180) == 0x180 &&
              oldProperty.reference > 0 && newProperty.reference > 0 &&
              reference.data.kind(oldProperty.reference) == "Class" &&
              p.kind(newProperty.reference) == "Class" &&
              (classInfo(reference.data, oldProperty.reference).flags &
               0x200000) &&
              (classInfo(p, newProperty.reference).flags & 0x200000) &&
              (functionInfo(reference.data, original.outer).flags & 0x400)) {
            // Client packages contain both constructor-link forms for native
            // out parameters of instanced classes. Retain only an attested
            // bit after verifying the entire native function contract.
            if (!sameFunctionContract(reference.data, original.outer,
                                      output.outer))
              throw std::runtime_error(
                  "Native constructor-link reference contract mismatch");
            Reader propertyReader{output.payload};
            propertyReader.index();
            propertyReader.index();
            propertyReader.index();
            propertyReader.u32();
            for (int byte = 0; byte < 4; ++byte)
              output.payload[propertyReader.pos + byte] =
                  std::uint8_t(oldProperty.flags >> (byte * 8));
          }
        }
        if (p.kind(int(i) + 1) == "Function" && output.outer > 0 &&
            p.kind(output.outer) == "State" &&
            lower(reference.data.ref(original.super)) !=
                lower(p.ref(output.super))) {
          // Client compilers can retain a different SuperField anchor from
          // the SDK. Only an explicit reference with unchanged contracts may
          // select that anchor; ordinary compilation keeps SDK lookup rules.
          if (original.outer <= 0 ||
              reference.data.kind(original.outer) != "State" ||
              !sameFunctionContract(reference.data, *match, int(i) + 1))
            throw std::runtime_error(
                "State reference function contract mismatch");
          int super = 0;
          if (original.super) {
            if (original.super < 0 ||
                reference.data.kind(original.super) != "Function")
              throw std::runtime_error(
                  "Unsupported reference state super link");
            auto packageIndex =
                std::size_t(&reference - target.packages.data());
            super = imports.symbol({packageIndex, original.super});
            if (super <= 0 ||
                !sameFunctionContract(reference.data, original.super, super))
              throw std::runtime_error(
                  "State reference super contract mismatch");
          }
          Reader reader{output.payload};
          reader.index();
          auto start = reader.pos;
          if (reader.index() != output.super)
            throw std::runtime_error(
                "Inconsistent emitted function super link");
          Bytes payload(output.payload.begin(), output.payload.begin() + start);
          index(payload, super);
          payload.insert(payload.end(), output.payload.begin() + reader.pos,
                         output.payload.end());
          output.payload = std::move(payload);
          output.super = super;
        }
        p.exports[i].flags =
            (p.exports[i].flags & ~0xf0000u) |
            (reference.data.exports[*match - 1].flags & 0xf0000u);
      }
    }
  }
  // Initial generation records include all emitted identities.
  for (int i = 0; i < 4; ++i) {
    p.header[56 + i] = std::uint8_t(p.exports.size() >> (8 * i));
    p.header[60 + i] = std::uint8_t(p.names.size() >> (8 * i));
  }
  p.validate();
  p.indexNames(false);
  return p;
}
}  // namespace openucc
