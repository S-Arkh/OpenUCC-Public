class FunctionClassCollisionReference extends Object dependsOn(CollisionName);
function int CollisionName(int V) { return V; }
function int CallProbe() { return CollisionName(7); }
