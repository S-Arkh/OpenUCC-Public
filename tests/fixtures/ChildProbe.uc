class ChildProbe extends ParentPkg.ParentProbe;

function int TransformProbe(int Value)
{
    return Value * 2 + InheritedValue;
}

function int CheckInheritedProbe()
{
    InheritedValue = 13;
    return DispatchProbe();
}

function int ExternalCallsProbe()
{
    return ExternalStaticProbe(4) + Max(3, 9);
}

function int SuperProbe()
{
    return super.TransformProbe(5);
}

function int SelfProbe(int InheritedValue)
{
    return self.InheritedValue;
}

function bool SideEffectProbe()
{
    InheritedValue++;
    return true;
}

function int LogicProbe()
{
    local bool Flag;
    InheritedValue = 0;
    Flag = false && SideEffectProbe();
    if (InheritedValue != 0) return -1;
    Flag = true || SideEffectProbe();
    if (InheritedValue != 0) return -2;
    Flag = true && SideEffectProbe();
    if (InheritedValue != 1) return -3;
    Flag = false || SideEffectProbe();
    if (InheritedValue != 2) return -4;
    Flag = false && (SideEffectProbe() || SideEffectProbe());
    Flag = true || (SideEffectProbe() && SideEffectProbe());
    return InheritedValue;
}

function int IncrementProbe()
{
    local int Value;
    local int Total;
    Value = 1;
    Total = Value++;
    Total += ++Value;
    Total -= Value--;
    Total += --Value;
    return Total;
}

defaultproperties
{
    InheritedValue = 9
}
