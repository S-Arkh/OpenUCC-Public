class DelegateOptionalReference extends Object;
delegate int Changed(optional int V) { return V; }
function int Handler(int V) { return V+7; }
function BindProbe() { Changed=Handler; }
function int CallProbe() { return Changed(); }
