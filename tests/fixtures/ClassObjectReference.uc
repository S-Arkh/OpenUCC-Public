class ClassObjectReference extends Object;
var class<Object> Factory;
var int Value;
static function int StaticProbe(int V) { return V; }
function class<Object> LiteralProbe() { return class'ClassObjectReference'; }
function Object MakeProbe() { return new class'ClassObjectReference'; }
function Object FactoryProbe(class<Object> T) { return new T; }
function Object NamedProbe(Object Outer, string N) { return new(Outer,N,0) class'ClassObjectReference'; }
function class<ClassObjectReference> CastProbe(Object V) { return class<ClassObjectReference>(V); }
function int DefaultProbe(class<ClassObjectReference> T) { return T.default.Value; }
function int CallProbe(class<ClassObjectReference> T) { return T.static.StaticProbe(7); }
function int OwnDefaultProbe() { return default.Value; }
defaultproperties { Factory=class'ClassObjectReference' Value=41 }
