class PrimitiveProbe extends Object;

var byte ScalarByte;
var float ScalarFloat;
var string ScalarText;
var name ScalarName;

function string BuildText(coerce string Value)
{
    return ">" $ Value @ "end";
}

function string UnicodeProbe()
{
    return "Привет" $ " " $ "🚀";
}

function string MixedUnicodeProbe()
{
    return "Привет 🚀 end";
}

function string EmptyProbe()
{
    local string Value;
    Value = "";
    return Value;
}

function float WidenProbe(int Value)
{
    return Value + 0.5;
}

function int NarrowProbe(float Value)
{
    return Value;
}

function byte WrapByteProbe(int Value)
{
    return byte(Value);
}

function bool HasNameProbe(name Value)
{
    return bool(Value);
}

function string NameTextProbe(name Value)
{
    return string(Value);
}

function int StringNumberProbe(string Value)
{
    return int(Value);
}

function int UnicodeLengthProbe()
{
    UnicodeProbe();
    return Len(UnicodeProbe());
}

defaultproperties
{
    ScalarByte=255
    ScalarFloat=1.25
    ScalarText="Привет"
    ScalarName='ProbeName'
}
