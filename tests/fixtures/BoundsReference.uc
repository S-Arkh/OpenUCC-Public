class BoundsReference extends Object;
function int Probe()
{
    local int Values[2];
    Values[2]=7;
    return Values[2];
}
function int NegativeProbe()
{
    local int Values[2];
    return Values[-1];
}
