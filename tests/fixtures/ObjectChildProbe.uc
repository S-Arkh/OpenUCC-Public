class ObjectChildProbe extends ObjectReference;
var ObjectPeerProbe Peer;
var array<Object> References;
function int VerifyProbe(Object Other)
{
    local Object Copy;
    Copy=self;
    StoreProbe(Copy);
    if (ReadProbe()!=self || EchoProbe(self)!=self || CastProbe(self)!=self) { return 1; }
    if (CastProbe(Other)!=None || !ExistsProbe(self) || ExistsProbe(None)) { return 2; }
    Copy=None;
    SetOutProbe(Copy);
    if (Copy!=self) { return 3; }
    References.Length=2;
    References[0]=self;
    References[1]=Other;
    if (References[0]!=self || References[1]!=Other) { return 4; }
    References.Length=0;
    References.Length=1;
    if (References[0]!=None || Peer!=None) { return 5; }
    return 0;
}
