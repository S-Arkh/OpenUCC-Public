class DynamicArrayBoolReference extends Object;
var array<bool> Bits;
function bool ReadProbe(int I) { return Bits[I]; }
function SetProbe(int I, bool V) { Bits[I]=V; }
