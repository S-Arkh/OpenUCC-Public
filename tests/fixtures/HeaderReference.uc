class HeaderReference extends Object dependsOn(WithinOuter) dependsOn(AccessReference);
var int Min;
function int ReadProbe() { return Min; }
function int FunctionProbe() { return super.Min(7,3); }
defaultproperties { Min=11 }
