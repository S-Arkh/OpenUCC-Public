class LocalParent extends Object;
var int Value;
var string Label;
function int ReadValue() { return Value; }
function int Dispatch() { return ReadValue(); }
static function int StaticValue() { return 7; }
defaultproperties { Value=5 Label="parent" }
