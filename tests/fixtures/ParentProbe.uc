class ParentProbe extends Object;

var int InheritedValue;

function int TransformProbe(int Value)
{
    return Value + InheritedValue;
}

function int DispatchProbe()
{
    return TransformProbe(5);
}

static function int ExternalStaticProbe(int Value)
{
    return Value + 11;
}

defaultproperties
{
    InheritedValue = 4
}
