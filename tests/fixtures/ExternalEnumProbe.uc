class ExternalEnumProbe extends ParentPkg.EnumReference;
var ParentPkg.EnumReference.EMode Other;
function int VerifyProbe()
{
    Other=ModeDone;
    return IntProbe(Other);
}
defaultproperties { Other=ModeActive }
