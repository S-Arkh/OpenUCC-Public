class ObjectContextReference extends Object;
var int Value;
var bool Flag;
var string Text;
var array<int> Values;
var ObjectContextReference Next;
function int GetValue() { return Value; }
function int ReadFieldProbe(ObjectContextReference Target) { return Target.Value; }
function SetFieldProbe(ObjectContextReference Target, int V) { Target.Value=V; }
function bool ReadBoolProbe(ObjectContextReference Target) { return Target.Flag; }
function string ReadStringProbe(ObjectContextReference Target) { return Target.Text; }
function int CallProbe(ObjectContextReference Target) { return Target.GetValue(); }
function int ChainedProbe(ObjectContextReference Target) { return Target.Next.Value; }
function int ArrayProbe(ObjectContextReference Target, int I) { return Target.Values[I]; }
function int LengthProbe(ObjectContextReference Target) { return Target.Values.Length; }
