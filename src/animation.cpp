#include "openucc/animation.hpp"
#include <stdexcept>

namespace openucc {
namespace {
void check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
std::size_t count(Reader& r, std::size_t stride = 1) {
  auto n = r.index();
  check(n >= 0 && n <= 1000000 && std::size_t(n) <= (r.data.size() - r.pos) / stride,
        "Invalid animation array count");
  return std::size_t(n);
}
void count(Bytes& out, std::size_t n) {
  check(n <= 1000000, "Excessive animation array");
  index(out, int(n));
}
template <std::size_t N> std::array<std::uint32_t, N> words(Reader& r) {
  std::array<std::uint32_t, N> result{};
  for (auto& word : result) word = r.u32();
  return result;
}
template <std::size_t N> void words(Bytes& out, const std::array<std::uint32_t, N>& a) {
  for (auto word : a) u32(out, word);
}
template <std::size_t N> std::vector<std::array<std::uint32_t, N>> vectors(Reader& r) {
  std::vector<std::array<std::uint32_t, N>> result(count(r, N * 4));
  for (auto& value : result) value = words<N>(r);
  return result;
}
template <std::size_t N> void vectors(Bytes& out, const std::vector<std::array<std::uint32_t, N>>& a) {
  count(out, a.size());
  for (const auto& value : a) words(out, value);
}
AnimationTrack track(Reader& r) {
  AnimationTrack t; t.flags = r.u32();
  t.rotations = vectors<4>(r); t.positions = vectors<3>(r);
  t.times.resize(count(r, 4));
  for (auto& time : t.times) time = r.u32();
  return t;
}
void track(Bytes& out, const AnimationTrack& t) {
  u32(out, t.flags); vectors(out, t.rotations); vectors(out, t.positions);
  count(out, t.times.size()); for (auto time : t.times) u32(out, time);
}
int reference(Reader& r, const Package& p) {
  int id = r.index(); p.ref(id); return id;
}
void profile(const Package& p) {
  check(((p.version == 131 || p.version == 133) && p.licensee == 40) ||
        (p.version == 123 && p.licensee == 36), "Unsupported MeshAnimation profile");
}
}
AnimationBody decodeAnimation(Reader& r, const Package& p, const Export& e) {
  profile(p); AnimationBody body; body.version = r.u32();
  check(body.version == 1, "Unsupported MeshAnimation version");
  body.bones.resize(count(r, 9));
  for (auto& bone : body.bones) {
    bone.name = p.name(r.index()); bone.flags = r.u32(); bone.parent = r.u32();
  }
  auto end = r.u32(); body.moves.resize(count(r, 35));
  for (auto& move : body.moves) {
    auto localEnd = r.u32(); move.header = words<6>(r);
    move.boneIndices.resize(count(r, 4));
    for (auto& bone : move.boneIndices) bone = r.u32();
    move.tracks.resize(count(r, 7));
    for (auto& t : move.tracks) t = track(r);
    move.root = track(r);
    check(localEnd == e.sourceOffset + r.pos, "Invalid animation move boundary");
  }
  check(end == e.sourceOffset + r.pos, "Invalid animation moves boundary");
  body.sequences = decodeAnimationSequences(r, p);
  check(r.pos == r.data.size(), "Trailing MeshAnimation native bytes");
  return body;
}
std::vector<AnimationSequence> decodeAnimationSequences(Reader& r, const Package& p) {
  std::vector<AnimationSequence> sequences(count(r));
  for (auto& s : sequences) {
    s.scalar = r.u32(); s.name = p.name(r.index()); s.groups.resize(count(r));
    for (auto& group : s.groups) group = p.name(r.index());
    s.start = r.u32(); s.frames = r.u32(); s.events.resize(count(r, p.version >= 131 ? 10 : 6));
    for (auto& event : s.events) {
      event.time = r.u32(); event.function = p.name(r.index()); event.object = reference(r, p);
      auto n = p.version >= 131 ? r.u32() : 0;
      check(n <= 1000000 && n <= (r.data.size() - r.pos) / 2, "Invalid animation event text length");
      event.text.resize(n); for (auto& unit : event.text) unit = r.u16();
    }
    s.rate = r.u32(); s.prefix = words<3>(r); s.object = reference(r, p);
    s.suffix = words<2>(r); s.flag = r.byte(); s.pairs = vectors<2>(r);
    s.nested.resize(count(r, 5));
    for (auto& nested : s.nested) { nested.word = r.u32(); nested.pairs = vectors<2>(r); }
    s.final = words<2>(r); s.last = vectors<2>(r);
  }
  return sequences;
}
void encodeAnimation(const AnimationBody& body, Package& p, Bytes& out, Export& e,
                     const std::function<int(int)>& ref) {
  profile(p); check(body.version == 1, "Unsupported MeshAnimation version");
  u32(out, body.version); count(out, body.bones.size());
  for (const auto& bone : body.bones) {
    index(out, p.intern(bone.name)); u32(out, bone.flags); u32(out, bone.parent);
  }
  auto end = out.size(); u32(out, 0); count(out, body.moves.size());
  for (const auto& move : body.moves) {
    auto localEnd = out.size(); u32(out, 0); words(out, move.header);
    count(out, move.boneIndices.size()); for (auto bone : move.boneIndices) u32(out, bone);
    count(out, move.tracks.size()); for (const auto& t : move.tracks) track(out, t);
    track(out, move.root); e.lazyOffsets.push_back({localEnd, out.size()});
  }
  e.lazyOffsets.push_back({end, out.size()});
  encodeAnimationSequences(body.sequences, p, out, ref);
}
void encodeAnimationSequences(const std::vector<AnimationSequence>& sequences, Package& p, Bytes& out,
                              const std::function<int(int)>& ref) {
  auto object = [&](int id) { int mapped = ref(id); p.ref(mapped); index(out, mapped); };
  count(out, sequences.size());
  for (const auto& s : sequences) {
    u32(out, s.scalar); index(out, p.intern(s.name)); count(out, s.groups.size());
    for (const auto& group : s.groups) index(out, p.intern(group));
    u32(out, s.start); u32(out, s.frames); count(out, s.events.size());
    for (const auto& event : s.events) {
      u32(out, event.time); index(out, p.intern(event.function)); object(event.object);
      check(event.text.size() <= 1000000, "Excessive animation event text");
      if (p.version >= 131) {
        u32(out, std::uint32_t(event.text.size())); for (auto unit : event.text) u16(out, unit);
      } else check(event.text.empty(), "Legacy animation events cannot store text");
    }
    u32(out, s.rate); words(out, s.prefix); object(s.object); words(out, s.suffix);
    out.push_back(s.flag); vectors(out, s.pairs); count(out, s.nested.size());
    for (const auto& nested : s.nested) { u32(out, nested.word); vectors(out, nested.pairs); }
    words(out, s.final); vectors(out, s.last);
  }
}
}
