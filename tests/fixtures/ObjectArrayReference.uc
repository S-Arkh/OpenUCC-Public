class ObjectArrayReference extends Object;
function int CountProbe(array<Object> V) { return V.Length; }
function int CallProbe(array<ObjectArrayReference> V) { return CountProbe(V); }
function array<Object> ReturnProbe(array<ObjectArrayReference> V) { return V; }
