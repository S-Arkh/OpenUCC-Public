class Int64SwitchReference extends Object;
function int Probe(INT64 V)
{
    switch (V)
    {
    case INT64("4194304"): return 7;
    case INT64("33554432"): return 3;
    default: return 1;
    }
}
