class DelegateProbe extends DelegateReference;
function int Handler(int V) { return V+3; }
function bool VerifyProbe()
{
    if (CallProbe(7)!=7) return false;
    BindProbe();
    if (CallProbe(7)!=10) return false;
    ClearProbe();
    return CallProbe(7)==7;
}
