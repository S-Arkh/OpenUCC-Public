class EnumReference extends Object;
enum EMode { ModeIdle, ModeActive, ModeDone };
var EMode Mode;
var EMode Fixed[2];
var array<EMode> Modes;
function EMode LiteralProbe() { return ModeActive; }
function EMode ReadProbe() { return Mode; }
function SetProbe(EMode V) { Mode=V; }
function int IntProbe(EMode V) { return int(V); }
function EMode CastProbe(byte V) { return EMode(V); }
function bool EqualProbe(EMode A, EMode B) { return A==B; }
function OutProbe(out EMode V) { V=ModeDone; }
function int CountProbe() { return EMode.EnumCount; }
function EMode QualifiedProbe() { return EMode.ModeDone; }
defaultproperties
{
    Mode=ModeActive
    Fixed(1)=ModeDone
}
