class ArrayProbe extends ArrayReference;
var string Texts[3];
var int Wide[130];
function int WideValue(int I) { return Wide[I]; }
function Increment(out int V) { V++; }
function int LocalProbe()
{
    local int Work[3], I;
    for(I=0; I<3; I++) { Work[I]=I*2; }
    Increment(Work[2]);
    Values[1]=Work[2];
    Texts[1]="changed";
    return Work[0]+Work[1]+Values[1];
}
defaultproperties
{
    Values(0)=10
    Values(2)=30
    Texts(0)="first"
    Texts(2)="last"
    Wide(127)=127
    Wide(128)=128
}
