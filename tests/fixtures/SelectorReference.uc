class SelectorReference extends Object;
enum EColor { Yellow03, Blue03 };
var int Yellow03;
const Limit=7;
function int ConstantProbe(SelectorReference V) { return V.Limit; }
function int ObjectProbe(Object V, Object A)
{
    switch (V)
    {
    case A: return 7;
    case None: return 3;
    default: return 1;
    }
}
function int FieldProbe() { return Yellow03; }
function int BoolProbe(bool V)
{
    switch (V) { case true: return 7; case false: return 3; }
}
