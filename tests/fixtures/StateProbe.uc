class StateProbe extends StateReference;
function bool VerifyProbe()
{
    GotoState('Idle');
    if (ReadProbe()!=7 || Number!=11 || !IsInState('Idle')) return false;
    GotoState('Active');
    Number=23;
    MarkProbe();
    if (ReadProbe()!=7 || Number!=23) return false;
    GotoState('');
    if (Number!=17 || ReadProbe()!=1) return false;
    MarkProbe();
    return Number==3;
}
