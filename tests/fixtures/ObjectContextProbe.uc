class ObjectContextProbe extends ObjectContextReference;
var int Counter;
function int GetValue() { return Value+10; }
function ObjectContextReference IdentityProbe(ObjectContextReference Target) { return Target; }
function int SideEffectProbe() { Counter++; return 1; }
function ConsumeProbe(int V) { Value=V; }
function TouchProbe(out int V) { V+=2; }
function int ForwardProbe(ObjectContextLatePeer Peer) { return Peer.Owner.GetValue(); }
function int VerifyContext(ObjectContextReference Target)
{
    Value=7;
    Target.Value=21;
    Target.Flag=true;
    Target.Text="context";
    Target.Next=self;
    if (ReadFieldProbe(Target)!=21 || !ReadBoolProbe(Target) || ReadStringProbe(Target)!="context") { return 1; }
    if (CallProbe(self)!=17 || ChainedProbe(Target)!=7 || IdentityProbe(Target).Value!=21) { return 2; }
    Target.Values.Length=2;
    Target.Values[0]=3;
    Target.Values[1]=5;
    Target.Values.Insert(1,1);
    if (LengthProbe(Target)!=3 || self.ArrayProbe(Target,2)!=5 || Target.Values[1]!=0) { return 3; }
    Target.Values.Remove(0,1);
    TouchProbe(Target.Value);
    if (Target.Value!=23 || Target.Values.Length!=2 || Target.Values[1]!=5) { return 4; }
    return 0;
}
function bool NullContextProbe(ObjectContextProbe Target)
{
    Counter=0;
    Target.ConsumeProbe(SideEffectProbe());
    return Counter==0 && Target.Value==0;
}
