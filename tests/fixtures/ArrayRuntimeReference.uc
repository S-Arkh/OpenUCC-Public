class ArrayRuntimeReference extends Commandlet;
var array<bool> Bits;
var array<int> Values;
function bool ReadProbe(int I) { return Bits[I]; }
function SetProbe(int I, bool V) { Bits[I]=V; }
function int Main(string Params)
{
    Bits.Length=3;
    SetProbe(0,true);
    SetProbe(2,true);
    Log("ARRAY_BOOL_VALUES=" $ ReadProbe(0) $ "," $ ReadProbe(1) $ "," $ ReadProbe(2));
    Values.Length++;
    Log("ARRAY_LENGTH_INCREMENT=" $ Values.Length);
    return 0;
}
