class ExternalDelegateProbe extends ParentPkg.DelegateReference;
function int Handler(int V) { return V+5; }
function bool VerifyProbe()
{
    BindProbe();
    if (CallProbe(7)!=12) return false;
    ClearProbe();
    return CallProbe(7)==7;
}
