class DeclarationReference extends Object;
var() const int Fixed;
var transient int First, Second;
final simulated function int FinalProbe() { return Fixed; }
simulated event int EventProbe() { return 4; }
exec function ExecProbe() { First=1; Second=2; }
singular function SingularProbe() { First=3; }
function EmptyProbe();
defaultproperties { Fixed=11 }
