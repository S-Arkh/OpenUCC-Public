class LocalGrandchild extends LocalChild;
function int ReadValue() { return super.ReadValue() + 100; }
defaultproperties { Label="grandchild" }
