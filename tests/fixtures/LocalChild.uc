class LocalChild extends LocalParent;
function int ReadValue() { return super.ReadValue() + 10; }
function int SetValue(int V) { Value=V; return Dispatch() + StaticValue(); }
defaultproperties { Value=9 }
