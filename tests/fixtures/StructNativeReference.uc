class StructNativeReference extends Object;
var Vector Position;
var Rotator Rotation;
function Vector AddProbe(Vector A, Vector B) { return A+B; }
function float ReadProbe(Vector V) { return V.X; }
function bool EqualProbe(Vector A, Vector B) { return A==B; }
