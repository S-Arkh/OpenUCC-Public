class Int64Reference extends Object;
const Big=1152921504606846976;
var INT64 Value;
var INT64 Fixed[2];
var array<INT64> Items;
function INT64 LiteralProbe() { return Big; }
function INT64 MinimumProbe() { return -9223372036854775808; }
function INT64 AddProbe(INT64 A, int B) { return A+B; }
function bool CompareProbe(INT64 A, INT64 B) { return A==B; }
function INT64 NegateProbe(INT64 A) { return -A; }
function INT64 MultiplyProbe(INT64 A, INT64 B) { return A*B; }
function INT64 ScaleProbe(INT64 A, float B) { return A*B; }
function INT64 DivideProbe(INT64 A, INT64 B) { return A/B; }
function INT64 SubtractProbe(INT64 A, INT64 B) { return A-B; }
function AddOutProbe(out INT64 A, INT64 B) { A+=B; }
function SubtractOutProbe(out INT64 A, INT64 B) { A-=B; }
function bool LessProbe(INT64 A, INT64 B) { return A<B; }
function bool GreaterProbe(INT64 A, INT64 B) { return A>B; }
function bool LessEqualProbe(INT64 A, INT64 B) { return A<=B; }
function bool GreaterEqualProbe(INT64 A, INT64 B) { return A>=B; }
function bool DifferentProbe(INT64 A, INT64 B) { return A!=B; }
function Object.INT64 IntProbe(int V) { return INT64(V); }
function INT64 ByteProbe(byte V) { return INT64(V); }
function INT64 BoolProbe(bool V) { return INT64(V); }
function INT64 FloatProbe(float V) { return INT64(V); }
function INT64 StringProbe(string V) { return INT64(V); }
function byte ToByteProbe(INT64 V) { return byte(V); }
function int ToIntProbe(INT64 V) { return int(V); }
function bool ToBoolProbe(INT64 V) { return bool(V); }
function float ToFloatProbe(INT64 V) { return float(V); }
function string ToStringProbe(INT64 V) { return string(V); }
function SetProbe(out INT64 V) { V=5000000000; }
defaultproperties
{
    Value=5000000000
    Fixed(1)=-9223372036854775808
    Items(0)=9223372036854775807
}
