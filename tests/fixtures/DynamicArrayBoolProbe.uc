class DynamicArrayBoolProbe extends DynamicArrayBoolReference;
function int VerifyProbe()
{
    local array<bool> Copy;
    Bits.Length=3;
    SetProbe(0,true);
    SetProbe(2,true);
    if (!ReadProbe(0) || ReadProbe(1) || !ReadProbe(2)) { return 1; }
    Copy=Bits;
    Copy[0]=false;
    if (!Bits[0] || Copy[0]) { return 2; }
    Bits.Insert(1,1);
    if (Bits.Length!=4 || Bits[1] || !Bits[3]) { return 3; }
    Bits.Remove(0,2);
    if (Bits.Length!=2 || Bits[0] || !Bits[1]) { return 4; }
    return 0;
}
