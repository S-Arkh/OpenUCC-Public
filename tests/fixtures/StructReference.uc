class StructReference extends Object;
struct BaseData
{
    var int Value;
    var string Text;
};
struct RichData extends BaseData
{
    var float Ratio;
};
struct Wrapper
{
    struct InnerData
    {
        var int Code;
    };
    var InnerData Detail;
    var BaseData Item;
    var RichData Rich;
};
var BaseData Data;
var RichData Rich;
var Wrapper Nested;
struct PackedData
{
    var byte First;
    var bool Flag, OtherFlag;
    var byte Last;
};
struct TinyData { var byte Value; };
struct BooleanBase { var bool First; };
struct BooleanChild extends BooleanBase { var bool Second; };
var PackedData Packed;
var TinyData Tiny;
var BooleanChild BooleanValue;
function int ReadProbe() { return Data.Value; }
function WriteProbe(int V) { Data.Value=V; }
function BaseData CopyProbe(BaseData V) { Data=V; return Data; }
function string TextProbe(BaseData V) { return V.Text; }
function OutProbe(out BaseData V) { V.Value=29; V.Text="out"; }
function int NestedProbe() { return Nested.Detail.Code; }
function int InheritedProbe(RichData V) { return V.Value; }
function bool EqualProbe(BaseData A, BaseData B) { return A==B; }
function bool DifferentProbe(BaseData A, BaseData B) { return A!=B; }
function BaseData ContextProbe(StructReference Target) { return Target.Data; }
function BaseData UpcastProbe(RichData V) { return V; }
function bool BoolProbe() { return Packed.Flag; }
function PackedData PackedContextProbe(StructReference Target) { return Target.Packed; }
function TinyData TinyContextProbe(StructReference Target) { return Target.Tiny; }
function BooleanChild BooleanContextProbe(StructReference Target) { return Target.BooleanValue; }
