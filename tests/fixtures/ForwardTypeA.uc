class ForwardTypeA extends Object;
struct Record { var int Value; };
var enum ELocal { LocalZero, LocalOne } Own;
var ForwardTypeB.Data Foreign;
var ForwardTypeB.EMode Mode;
function ForwardTypeB.Data EchoProbe(ForwardTypeB.Data V) { return V; }
function bool VerifyProbe()
{
    local ForwardTypeB Other;
    Other=new class'ForwardTypeB';
    Other.Reference.Value=73;
    return Foreign.Number==67 && Foreign.Text=="forward" && Mode==ForwardTypeB.EMode.ModeSecond && Own==LocalOne && EchoProbe(Foreign).Number==67 && Other.Reference.Value==73;
}
defaultproperties
{
    Foreign=(Number=67,Text="forward")
    Mode=ForwardTypeB.EMode.ModeSecond
    Own=LocalOne
}
