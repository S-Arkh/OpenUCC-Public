class WithinProbe extends Object;
function bool VerifyProbe()
{
    local WithinOuter Owner;
    local WithinReference Child;
    local WithinNestedReference Nested;
    Owner=new class'WithinOuter';
    Child=new(Owner) class'WithinReference';
    Child.SetProbe(79);
    Nested=new(Child) class'WithinNestedReference';
    return Child.ReadProbe()==79 && Child.CallProbe()==79 && Child.ExplicitProbe()==79 && Child.ExplicitCallProbe()==79 && Nested.NestedProbe()==79 && Nested.CallNestedProbe()==79;
}
