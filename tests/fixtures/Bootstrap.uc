class Bootstrap extends Object;

var int Count;
var bool Active;

function int Choose(bool UseArgument, int Value)
{
    local int Result;
    Result = Value;
    if (UseArgument)
    {
        return Result;
    }
    else
    {
        return 17;
    }
}

function Reset()
{
    Count = 0;
    Active = false;
}

function Drain()
{
    while (Active)
    {
        Count = 1;
        Active = false;
    }
}

function int Sum(int Limit)
{
    local int Index;
    local int Result;
    Index = 0;
    Result = 0;
    while (Index <= Limit)
    {
        Result = Result + Index * 2 - Index;
        Index = Index + 1;
    }
    return Result;
}

function bool InvertFlag(bool Value)
{
    return !Value;
}

function int Negate(int Value)
{
    return -Value;
}

function int CallsProbe()
{
    local int Value;
    Value = 3;
    BumpProbe(Value);
    return LaterProbe(Value) + OptionalProbe(, 5) + StaticProbe(2);
}

function int LaterProbe(int Value)
{
    return Value * 2;
}

function BumpProbe(out int Value)
{
    Value = Value + 4;
}

function int OptionalProbe(optional int A, optional int B)
{
    return A + B;
}

static function int StaticProbe(int Value)
{
    return Value + 1;
}

function int RecurseProbe(int N)
{
    if (N <= 1)
        return 1;
    return N * RecurseProbe(N - 1);
}

function int ForProbe()
{
    local int I;
    local int Total;
    Total = 0;
    for (I = 0; I < 10; I = I + 1)
    {
        if (I == 2)
            continue;
        if (I == 5)
            break;
        Total = Total + I;
    }
    return Total;
}

defaultproperties
{
    Count = 42
    Active = true
}
