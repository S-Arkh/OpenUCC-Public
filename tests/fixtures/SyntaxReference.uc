class SyntaxReference extends Object;
const Amount=20.0f;
native(4003) static final function float ScaleFloat(float A, float B);
function float LiteralProbe() { return Amount; }
function float NativeProbe(float V) { return ScaleFloat(V,2.5f); }
function int PartialProbe(bool V) { if (V) return 7; }
