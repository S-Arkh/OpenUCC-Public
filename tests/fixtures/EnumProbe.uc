class EnumProbe extends EnumReference;
function int VerifyProbe()
{
    local EMode V;
    if (Mode!=ModeActive || Fixed[1]!=ModeDone || Modes.Length!=0) { return 1; }
    V=LiteralProbe();
    if (IntProbe(V)!=1 || !EqualProbe(V,ModeActive) || CastProbe(2)!=ModeDone) { return 2; }
    OutProbe(V);
    SetProbe(V);
    if (ReadProbe()!=ModeDone || CountProbe()!=3 || QualifiedProbe()!=ModeDone) { return 3; }
    Modes.Length=2;
    Modes[1]=ModeDone;
    if (Modes[1]!=V) { return 4; }
    return 0;
}
