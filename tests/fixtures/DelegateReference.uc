class DelegateReference extends Object;
delegate int Changed(int V) { return V; }
function int Handler(int V) { return V; }
function BindProbe() { Changed=Handler; }
function ClearProbe() { Changed=None; }
function int CallProbe(int V) { return Changed(V); }
