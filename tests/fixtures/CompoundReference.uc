class CompoundReference extends Object;
const Count = 3;
const Caption = "compound";
enum EMode
{
    ModeIdle,
    ModeActive,
    ModeDone
};
struct Sample
{
    var int Value;
    var string Text;
    var EMode Mode;
};
var Sample Item;
var EMode Mode;
var int Fixed[Count];
function int ConstantProbe() { return Count; }
function string StringConstantProbe() { return Caption; }
function EMode EnumProbe() { return ModeActive; }
function int ReadProbe() { return Item.Value; }
function WriteProbe(int V) { Item.Value=V; }
function Sample CopyProbe(Sample V) { Item=V; return Item; }
function string MemberProbe(Sample V) { return V.Text; }
defaultproperties
{
    Item=(Value=17,Text="sample",Mode=ModeDone)
    Mode=ModeActive
    Fixed(2)=9
}
