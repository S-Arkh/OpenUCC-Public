class StateReference extends Object;
var int Number;
function int ReadProbe() { return 1; }
function MarkProbe() { Number=3; }
auto state Idle
{
    function int ReadProbe() { return 7; }
    function BeginState() { Number=11; }
Begin:
    Number=13;
    goto 'Finished';
Finished:
    stop;
}
state Active extends Idle
{
    ignores MarkProbe;
    function int ReadProbe() { return super.ReadProbe(); }
    function int GlobalProbe() { return global.ReadProbe(); }
    function EndState() { Number=17; }
}
