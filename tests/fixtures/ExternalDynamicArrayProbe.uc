class ExternalDynamicArrayProbe extends ParentPkg.DynamicArrayValueReference;
function int VerifyProbe()
{
    local array<int> Copy;
    if (Values.Length!=3 || Values[0]!=5 || Values[1]!=11 || Values[2]!=7) { return 1; }
    if (Texts.Length!=3 || Texts[0]!="first" || Texts[1]!="external" || Texts[2]!="last") { return 2; }
    Copy=CopyProbe(Values);
    SetProbe(Copy);
    if (Copy.Length!=2 || Copy[1]!=9 || Values.Length!=3) { return 3; }
    return ReadProbe(Values)-5;
}
defaultproperties { Values(1)=11 Texts(1)="external" }
