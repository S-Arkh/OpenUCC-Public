class ObjectReference extends Object;
var Object Saved;
function Object EchoProbe(Object Value) { return Value; }
function Object SelfProbe() { return self; }
function StoreProbe(Object Value) { Saved=Value; }
function Object ReadProbe() { return Saved; }
function bool EqualProbe(Object A, Object B) { return A==B; }
function bool ExistsProbe(Object Value) { return bool(Value); }
function string TextProbe(Object Value) { return string(Value); }
function ObjectReference CastProbe(Object Value) { return ObjectReference(Value); }
function SetOutProbe(out Object Value) { Value=self; }
defaultproperties { Saved=None }
