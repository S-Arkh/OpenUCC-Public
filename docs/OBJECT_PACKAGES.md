# Explicit object packages

`compile-objects` reconstructs a classless package from explicit `.uobj`
declarations. This is an openUCC data-source format, not SDK UnrealScript syntax.
It separates object ownership from the SDK-specific semantics of nested
`Begin Object` blocks.

Use this command for packages containing instances of existing object classes,
such as effect definitions. Each `.uobj` declaration supplies an object's class,
name, owner and properties. The matching original package attests identities,
and dependencies supply the classes and property types. It is not a general
conversion from arbitrary files and is limited to the supported forms below.

For ordinary UnrealScript classes, use `compile` with `.uc` files instead;
`compile-objects` and `.uobj` are not required.

```text
object Class=Engine.L2SkillEffect Name="Skill" Outer="None"
{
    SkillID=1
    CastingAction(0)=L2EffectEmitter'Skill.Emitter'
}
object Class=Engine.L2EffectEmitter Name="Emitter" Outer="Skill"
{
    ScaleSize=1.0
}
```

`Name` is a quoted leaf name; `Outer` is a quoted package-relative object path,
or `"None"` for package scope. A file may contain multiple declarations.
Declarations may precede their owners or referenced objects. Values use the
existing default-property parser, including struct and array values and the
`__FloatBits` extension.

```sh
openucc compile-objects Core.u rebuilt/Objects.u \
  --reference original/Objects.u --dependency Engine.u \
  --source-dir objects
```

## Initial supported scope

- Profile 133/40, with a matching Core profile.
- An explicit self-package reference is required.
- Every reference export must have exactly one typed declaration.
- Object classes must resolve to exported classes in loaded dependencies.
- Reference name, class and outer identities must agree with declarations.
- Only public and load-context object flag bits are supported and retained.
- Reference bodies must be complete tagged-property streams. Super links,
  stacked objects, other flag bits and trailing native bytes are rejected.

All output property values are parsed and encoded from source. Reference bodies
are checked for supported boundaries, not copied into output. Local object
references are rebound to new export indices. External imports follow the same
reference-attestation rules as ordinary compilation.
