class OptionalCallReference extends Object;
function int F(optional int A, optional int B) { return A+B; }
function int EmptyProbe() { return F(); }
function int FirstProbe() { return F(7); }
function int SecondProbe() { return F(,7); }
function NativeProbe() { Log("probe"); }
