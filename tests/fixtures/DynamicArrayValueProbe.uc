class DynamicArrayValueProbe extends DynamicArrayValueReference;
function array<string> CopyStrings(array<string> Input) { return Input; }
function int VerifyProbe()
{
    local array<int> Copy;
    local array<string> Words;
    if (Values.Length!=3 || Values[0]!=5 || Values[1]!=6 || Values[2]!=7) { return 1; }
    if (Texts.Length!=3 || Texts[0]!="first" || Texts[1]!="middle" || Texts[2]!="last") { return 2; }
    Copy=CopyProbe(Values);
    Copy[0]=99;
    if (Values[0]!=5 || ReadProbe(Copy)!=99) { return 3; }
    SetProbe(Copy);
    if (Copy.Length!=2 || Copy[1]!=9 || Values.Length!=3) { return 4; }
    Words=CopyStrings(Texts);
    Words[0]="changed";
    if (Texts[0]!="first" || Words[0]!="changed") { return 5; }
    CopyStrings(Words);
    DiscardProbe();
    AssignProbe();
    if (Values.Length!=3 || Values[0]!=5 || Values[2]!=7) { return 6; }
    return 0;
}
defaultproperties { Values(1)=6 Texts(1)="middle" }
