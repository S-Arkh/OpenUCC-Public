class WithinReference extends Object within WithinOuter dependsOn(WithinOuter);
function int ReadProbe() { return Number; }
function SetProbe(int V) { Number=V; }
function int CallProbe() { return ReadNumber(); }
function int ExplicitProbe() { return Outer.Number; }
function int ExplicitCallProbe() { return Outer.ReadNumber(); }
