class PropertyShadowReference extends PropertyShadowParent;
var string Name;
var string Counter;
function string ReadProbe() { return Name; }
function bool VerifyProbe() { return GetParentCounter()==7 && Counter=="child" && Name=="shadow"; }
defaultproperties
{
    Counter="child"
    Name="shadow"
}
