class DynamicArrayProbe extends DynamicArrayReference;
function Change(out int V) { V+=10; }
function int MutateProbe()
{
    Values.Length=3;
    Values[0]=1;
    Values[1]=2;
    Values[2]=3;
    Values.Insert(1,2);
    if (Values.Length!=5 || Values[3]!=2 || Values[1]!=0) { return 1; }
    Values.Remove(2,2);
    if (Values.Length!=3 || Values[2]!=3) { return 2; }
    Change(Values[2]);
    if (Values[2]!=13) { return 3; }
    Values.Length=1;
    Values.Length=3;
    if (Values[0]!=1 || Values[1]!=0 || Values[2]!=0) { return 4; }
    Texts.Length=2;
    Texts[0]="first";
    Texts[1]="last";
    Texts.Insert(1,1);
    if (Texts[1]!="" || Texts[2]!="last") { return 5; }
    Texts.Remove(0,1);
    if (Texts.Length!=2 || Texts[1]!="last") { return 6; }
    Texts.Length=0;
    Texts.Length=2;
    if (Texts[0]!="" || Texts[1]!="") { return 7; }
    return 0;
}
