class ExternalObjectProbe extends ParentPkg.ObjectReference;
function Object EchoProbe(Object Value) { return super.EchoProbe(Value); }
function bool VerifyProbe(Object Value)
{
    Saved=Value;
    return ReadProbe()==Value && CastProbe(self)==self && EchoProbe(self)==self;
}
