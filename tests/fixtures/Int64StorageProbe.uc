class Int64StorageProbe extends Object;
var INT64 Value;
var INT64 Minimum;
var array<INT64> Items;
function INT64 CopyProbe(INT64 V) { return V; }
function SetProbe(out INT64 V) { V=Value; }
function bool VerifyProbe()
{
    local INT64 Copy;
    Copy=CopyProbe(Value);
    Copy.Left=3;
    Items.Length=2;
    SetProbe(Items[1]);
    return Value.Left==1 && Value.Right==705032704 && Copy.Left==3 && Copy.Right==705032704
        && Minimum.Left==-2147483648 && Minimum.Right==0
        && Items[0].Left==2147483647 && Items[0].Right==-1
        && Items[1].Left==1 && Items[1].Right==705032704;
}
defaultproperties
{
    Value=5000000000
    Minimum=-9223372036854775808
    Items(0)=9223372036854775807
}
