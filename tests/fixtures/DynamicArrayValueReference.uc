class DynamicArrayValueReference extends Object;
var array<int> Values;
var array<string> Texts;
function array<int> CopyProbe(array<int> Input) { return Input; }
function int ReadProbe(array<int> Input) { return Input[0]; }
function SetProbe(out array<int> Input) { Input.Length=2; Input[1]=9; }
function AssignProbe() { Values=CopyProbe(Values); }
function DiscardProbe() { CopyProbe(Values); }
defaultproperties
{
    Values(0)=5
    Values(2)=7
    Texts(0)="first"
    Texts(2)="last"
}
