class SwitchProbe extends SwitchReference;
var int Evaluations;
function int Selector() { Evaluations++; return 2; }
function int VerifyProbe()
{
    local int I, J, R;
    if (IntProbe(1)!=11 || IntProbe(2)!=23 || IntProbe(3)!=23 || IntProbe(9)!=37) return 1;
    if (StringProbe("match")!=7 || StringProbe("MATCH")!=7 || StringProbe("other")!=9 || NameProbe('match')!=3 || NameProbe('other')!=5) return 2;
    if (ByteProbe(1)!=13 || ByteProbe(2)!=17) return 3;
    if (EnumSwitchProbe(FirstChoice)!=29 || EnumSwitchProbe(SecondChoice)!=19) return 4;
    switch (Selector()) { case 1: return 5; case 2: R=2; case 3: R=R+3; }
    if (R!=5 || Evaluations!=1) return 6;
    R=0;
    for (I=0; I<4; I++)
    {
        switch (I)
        {
            case 0: continue;
            case 1:
                switch (I) { case 1: R=R+10; break; default: return 7; }
                break;
            default:
                for (J=0; J<3; J++) { if (J==1) break; R=R+1; }
        }
        R=R+100;
    }
    if (I!=4 || R!=312) return 8;
    return 0;
}
