#include "openucc/resources.hpp"
#include <iostream>
#include <stdexcept>
using namespace openucc;
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error("Animation check failed"); }
template<class F> void rejects(F f) {
  try { f(); } catch (const std::exception&) { return; }
  throw std::runtime_error("Malformed animation accepted");
}
}
int main() {
  try {
    Package p; p.version = 133; p.licensee = 40; p.header.resize(56);
    Import cls; cls.package = p.intern("Engine"); cls.type = p.intern("Class");
    cls.name = p.intern("MeshAnimation"); p.imports.push_back(cls);
    Export e; e.type = -1; e.name = p.intern("Anim"); p.exports.push_back(e);
    ResourceBody body; body.kind = "MeshAnimation";
    auto& a = body.animation; a.bones.push_back({"Root", 7, 0});
    AnimationMove move; move.header[3] = 0x80000000; move.boneIndices = {0};
    AnimationTrack track; track.rotations = {{{0x7fc01234, 0, 0, 0x3f800000}}};
    track.positions = {{{0x80000000, 1, 2}}}; track.times = {0}; move.tracks.push_back(track);
    a.moves.push_back(move);
    AnimationSequence seq; seq.name = "Walk"; seq.groups = {"Movement"}; seq.frames = 1;
    seq.events.push_back({0x3f000000, "Notify", 1, {0xd55c, 0xae00, 0, 0xd800}});
    seq.object = 1; seq.pairs = {{{7, 8}}}; seq.nested.push_back({9, {{{10, 11}}}});
    seq.last = {{{12, 13}}}; a.sequences.push_back(seq);
    encodeResource(body, p, p.exports[0], [](int id) { return id; });
    auto original = Package::parse(p.serialize()); auto decoded = decodeResource(original, 1);
    check(decoded.animation.sequences[0].events[0].text == seq.events[0].text);
    check(decoded.animation.moves[0].tracks[0].rotations[0][0] == 0x7fc01234);
    Package moved = original; moved.names.clear();
    for (int i = 0; i < 130; ++i) moved.intern("Padding" + std::to_string(i));
    moved.imports[0].package = moved.intern("Engine"); moved.imports[0].type = moved.intern("Class");
    moved.imports[0].name = moved.intern("MeshAnimation"); moved.exports[0].name = moved.intern("Anim");
    moved.exports.push_back(moved.exports[0]); moved.exports[1].name = moved.intern("Other");
    for (auto& output : moved.exports)
      encodeResource(decoded, moved, output, [](int id) { return id == 1 ? 2 : id; });
    auto parsed = Package::parse(moved.serialize()); auto result = decodeResource(parsed, 1);
    check(parsed.exports[0].sourceOffset != original.exports[0].sourceOffset);
    check(result.animation.sequences[0].events[0].object == 2);
    check(result.animation.sequences[0].name == "Walk");
    check(result.animation.sequences[0].nested[0].pairs[0][1] == 11);
    auto corrupt = original; corrupt.exports[0].payload.push_back(0);
    rejects([&] { decodeResource(corrupt, 1); });
    // With one one-byte bone name, the global move boundary starts at byte 15.
    corrupt = original; corrupt.exports[0].payload[15] ^= 1;
    rejects([&] { decodeResource(corrupt, 1); });
    corrupt = original; corrupt.exports[0].payload.resize(10);
    rejects([&] { decodeResource(corrupt, 1); });
    body.animation.version = 2;
    rejects([&] { encodeResource(body, p, p.exports[0], [](int id) { return id; }); });
    {
      auto legacy = decoded; auto target = original; target.version = 123; target.licensee = 36;
      rejects([&] { encodeResource(legacy, target, target.exports[0], [](int id) { return id; }); });
      legacy.animation.sequences[0].events[0].text.clear();
      encodeResource(legacy, target, target.exports[0], [](int id) { return id; });
      auto loaded = decodeResource(Package::parse(target.serialize()), 1);
      check(loaded.animation.sequences[0].events[0].text.empty());
      check(loaded.animation.sequences[0].events[0].object == 1 && loaded.animation.sequences[0].rate == seq.rate);
      target.version = 131; target.licensee = 40;
      encodeResource(decoded, target, target.exports[0], [](int id) { return id; });
      check(decodeResource(Package::parse(target.serialize()), 1).animation.sequences[0].events[0].text == seq.events[0].text);
    }
    for (const std::string kind : {"AnimNotify_CameraLocation", "AnimNotify_Sound",
         "AnimNotify_RandomSound", "AnimNotify_AttackShot", "AnimNotify_Illusion",
         "AnimNotify_BoneScaleScript", "AnimNotify_Effect", "AnimNotify_SwimSound",
         "AnimNotify_Channeling", "AnimNotify_PlayCustomAnimation", "AnimNotify_AttackPreShot",
         "AnimNotify_DestroyEffect", "AnimNotify_AttackItem", "AnimNotify_ViewShake", "AnimNotify_Light",
         "AnimNotify_BoneScale", "AnimNotify_AttackVoice", "AnimNotify_JumpSound",
         "AnimNotify_LaunchAccumulativeBeam", "AnimNotify_ModifyVisBound", "AnimNotify_JumpDown", "AnimNotify_JumpUp",
         "AnimNotify_PawnStatusVoice", "AnimNotify_EventAnimEnd", "AnimNotify_BoneDirect", "AnimNotify_Ribbon",
         "AnimNotify_HideWeapon", "AnimNotify_PendingPawnEffectState", "AnimNotify_SkillEffect", "AnimNotify_HidePawn",
         "AnimNotify_ScreenFade", "AnimNotify_IdleSound"}) {
      auto package = original;
      package.imports[0].name = package.intern(kind);
      package.exports.push_back(package.exports[0]);
      package.exports[1].name = package.intern("Target");
      ResourceBody notify; notify.kind = kind;
      ResourceTag vector; vector.name = "Offset"; vector.kind = 10; vector.structure = "Vector";
      u32(vector.value, 0x80000000); u32(vector.value, 0x7fc01234); u32(vector.value, 0);
      notify.tags.push_back(vector);
      ResourceTag attach; attach.name = "Attach"; attach.kind = 3; attach.boolean = true;
      attach.booleanSize = 0;
      if (kind == "AnimNotify_RandomSound") {
        ResourceTag sounds; sounds.name = "Sounds"; sounds.kind = 9; sounds.references = {1, 0, -1};
        notify.tags.push_back(sounds);
      }
      if (kind == "AnimNotify_PlayCustomAnimation") {
        for (int i = 0; i < 130; ++i) package.intern("NotifyPadding" + std::to_string(i));
        ResourceTag name; name.name = "PlayAnimationName"; name.kind = 6; name.nameValue = "Attack";
        ResourceTag percent; percent.name = "PlayRandomPercent"; percent.kind = 2; u32(percent.value, 35);
        notify.tags.push_back(name); notify.tags.push_back(percent);
      }
      notify.tags.push_back(attach);
      if (kind == "AnimNotify_PawnStatusVoice") {
        ResourceTag sounds; sounds.name = "SoundName"; sounds.kind = 9;
        sounds.strings = {"", "ChrSound.Voice", "\xed\x95\x9c"}; notify.tags.insert(notify.tags.end() - 1, sounds);
      }
      encodeResource(notify, package, package.exports[0], [](int id) { return id == 1 ? 2 : id; });
      auto parsedNotify = Package::parse(package.serialize());
      auto decodedNotify = decodeResource(parsedNotify, 1);
      check(decodedNotify.tags[0].value == vector.value);
      check(decodedNotify.tags.back().boolean && decodedNotify.tags.back().booleanSize == 0);
      if (kind == "AnimNotify_PawnStatusVoice") check(decodedNotify.tags[1].strings == notify.tags[1].strings);
      auto nonzeroSize = notify; nonzeroSize.tags.back().booleanSize = 4;
      encodeResource(nonzeroSize, package, package.exports[0], [](int id) { return id; });
      check(decodeResource(Package::parse(package.serialize()), 1).tags.back().booleanSize == 4);
      if (kind == "AnimNotify_RandomSound") check(decodedNotify.tags[1].references == std::vector<int>({2, 0, -1}));
      if (kind == "AnimNotify_PlayCustomAnimation")
        check(decodedNotify.tags[1].nameValue == "Attack" && decodedNotify.tags[2].value == notify.tags[2].value);
      parsedNotify.exports[0].payload.push_back(0);
      rejects([&] { decodeResource(parsedNotify, 1); });
    }
    std::cout << "Animation relocation and malformed checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
