class WithinNestedReference extends Object within WithinReference dependsOn(WithinReference);
function int NestedProbe() { return Outer.Outer.Number; }
function int CallNestedProbe() { return Outer.Outer.ReadNumber(); }
