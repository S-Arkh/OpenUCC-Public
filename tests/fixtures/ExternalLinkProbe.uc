class ExternalLinkProbe extends ParentPkg.AccessReference;
function bool VerifyProbe()
{
    local ParentPkg.WithinOuter Owner;
    local ParentPkg.WithinReference Child;
    Owner=new class'ParentPkg.WithinOuter';
    Child=new(Owner) class'ParentPkg.WithinReference';
    Child.SetProbe(89);
    return Child.CallProbe()==89 && FamilyProbe()==0;
}
