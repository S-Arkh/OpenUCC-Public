#pragma once
#include "openucc/package.hpp"
#include <array>
#include <functional>

namespace openucc {
// Native profile-133/40 fields. Floating-point values retain their exact bits.
struct AnimationTrack {
  std::uint32_t flags = 0;
  std::vector<std::array<std::uint32_t, 4>> rotations;
  std::vector<std::array<std::uint32_t, 3>> positions;
  std::vector<std::uint32_t> times;
};
struct AnimationBone {
  std::string name;
  std::uint32_t flags = 0, parent = 0;
};
struct AnimationMove {
  std::array<std::uint32_t, 6> header{};
  std::vector<std::uint32_t> boneIndices;
  std::vector<AnimationTrack> tracks;
  AnimationTrack root;
};
struct AnimationEvent {
  std::uint32_t time = 0;
  std::string function;
  int object = 0;
  std::vector<std::uint16_t> text;
};
struct AnimationNested {
  std::uint32_t word = 0;
  std::vector<std::array<std::uint32_t, 2>> pairs;
};
struct AnimationSequence {
  std::uint32_t scalar = 0, start = 0, frames = 0, rate = 0;
  std::string name;
  std::vector<std::string> groups;
  std::vector<AnimationEvent> events;
  std::array<std::uint32_t, 3> prefix{};
  int object = 0;
  std::array<std::uint32_t, 2> suffix{}, final{};
  std::uint8_t flag = 0;
  std::vector<std::array<std::uint32_t, 2>> pairs, last;
  std::vector<AnimationNested> nested;
};
struct AnimationBody {
  std::uint32_t version = 1;
  std::vector<AnimationBone> bones;
  std::vector<AnimationMove> moves;
  std::vector<AnimationSequence> sequences;
};
AnimationBody decodeAnimation(Reader&, const Package&, const Export&);
std::vector<AnimationSequence> decodeAnimationSequences(Reader&, const Package&);
void encodeAnimationSequences(const std::vector<AnimationSequence>&, Package&, Bytes&,
                              const std::function<int(int)>&);
void encodeAnimation(const AnimationBody&, Package&, Bytes&, Export&,
                     const std::function<int(int)>&);
}
