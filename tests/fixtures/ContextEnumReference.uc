class ContextEnumReference extends Object;
var int Numbers[2];
var string ModeB;
enum EFirst { Zero, Max };
enum ESecond { OtherZero, Max };
function int MaxProbe(int A, int B) { return Max(A,B); }
function bool CompareProbe(ContextEnumOwner V) { return V.Mode==ModeB; }
function ContextEnumOwner.EMode ReturnProbe() { return ModeB; }
function SetProbe(ContextEnumOwner V) { V.Mode=ModeB; }
function bool CastProbe(ContextEnumOwner V) { return V.Mode==EMode(1); }
defaultproperties
{
    Numbers=7
    Numbers(1)=9
}
