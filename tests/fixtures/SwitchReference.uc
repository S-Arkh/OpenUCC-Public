class SwitchReference extends Object;
enum EChoice { FirstChoice, SecondChoice };
function int EnumSwitchProbe(EChoice V)
{
    switch (V) { case SecondChoice: return 19; default: return 29; }
    return 0;
}
function int IntProbe(int V)
{
    local int R;
    switch (V)
    {
        case 1: R=11; break;
        case 2:
        case 3: R=23; break;
        default: R=37;
    }
    return R;
}
function int StringProbe(string V)
{
    switch (V) { case "match": return 7; default: return 9; }
    return 0;
}
function int NameProbe(name V)
{
    switch (V) { case 'Match': return 3; default: return 5; }
    return 0;
}
function int ByteProbe(byte V)
{
    switch (V) { case 1: return 13; }
    return 17;
}
