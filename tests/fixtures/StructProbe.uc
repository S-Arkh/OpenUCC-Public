class StructProbe extends StructReference;
function ModifyValue(BaseData V) { V.Value=99; V.Text="local"; }
function int VerifyProbe()
{
    local BaseData A, B;
    local RichData R;
    A.Value=17;
    A.Text="first";
    B=CopyProbe(A);
    A.Text="changed";
    if (B.Value!=17 || B.Text!="first" || Data.Text!="first") { return 1; }
    if (!EqualProbe(B,Data) || DifferentProbe(B,Data) || !DifferentProbe(A,B)) { return 2; }
    OutProbe(B);
    if (B.Value!=29 || B.Text!="out" || Data.Value!=17) { return 3; }
    ModifyValue(B);
    if (B.Value!=29 || B.Text!="out") { return 9; }
    Nested.Detail.Code=43;
    Nested.Item=B;
    R.Value=51;
    R.Text="inherited";
    R.Ratio=1.5;
    Rich=R;
    Nested.Rich=R;
    if (NestedProbe()!=43 || InheritedProbe(R)!=51 || Nested.Item.Text!="out" || Rich.Ratio!=1.5) { return 4; }
    Nested.Item.Value=0;
    OutProbe(Nested.Item);
    if (Nested.Item.Value!=29 || Nested.Item.Text!="out") { return 10; }
    if (ContextProbe(self).Value!=17 || CopyProbe(B).Text!="out") { return 5; }
    Packed.First=7;
    Packed.Flag=true;
    Packed.OtherFlag=false;
    Packed.Last=13;
    Tiny.Value=19;
    BooleanValue.First=true;
    BooleanValue.Second=false;
    if (!BooleanContextProbe(self).First || BooleanContextProbe(self).Second) { return 8; }
    if (!BoolProbe() || PackedContextProbe(self).Last!=13 || TinyContextProbe(self).Value!=19) { return 7; }
    return 0;
}
