class ExternalStructProbe extends ParentPkg.StructReference;
var ParentPkg.StructReference.BaseData Other;
struct Extended extends ParentPkg.StructReference.BaseData
{
    var int Extra;
};
function bool VerifyProbe()
{
    local BaseData V;
    local Extended E;
    local Wrapper.InnerData Detail;
    V.Value=61;
    V.Text="external";
    Other=CopyProbe(V);
    Nested.Item=Other;
    Nested.Detail.Code=71;
    E.Value=83;
    E.Text="derived";
    E.Extra=97;
    Detail.Code=101;
    Nested.Detail=Detail;
    return Other.Value==61 && TextProbe(Nested.Item)=="external" && NestedProbe()==101 && E.Text=="derived" && E.Extra==97;
}
