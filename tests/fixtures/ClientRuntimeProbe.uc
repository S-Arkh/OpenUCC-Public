class ClientRuntimeProbe extends Object config(OpenUCCClientProbe);

var int StateValue;
var config int Int64Result, DelegateResult, ObjectStateResult, UIStateResult;
delegate int Changed(int Value) { return Value; }
function int Handler(int Value) { return Value + 7; }

state Active
{
    function BeginState() { StateValue = 31; }
}

static function ClientRuntimeProbe Run()
{
    local ClientRuntimeProbe Probe;
    Probe = new class'ClientRuntimeProbe';
    Probe.RunProbe();
    return Probe;
}

function RunProbe()
{
    local ClientRuntimeProbe Probe;
    local INT64 A, B;
    Probe = self;
    Int64Result = 0;
    DelegateResult = 0;
    ObjectStateResult = 0;
    UIStateResult = 0;
    Log("OPENUCC CLIENT BEGIN");
    A = 4294967297;
    B = A + 9;
    if (B == 4294967306 && B > A && B - A == 9)
    {
        Int64Result = 1;
        Log("OPENUCC CLIENT INT64 PASS");
    }
    else
        Log("OPENUCC CLIENT INT64 FAIL");
    Probe.Changed = Probe.Handler;
    if (Probe.Changed(5) == 12)
    {
        DelegateResult = 1;
        Log("OPENUCC CLIENT DELEGATE PASS");
    }
    else
        Log("OPENUCC CLIENT DELEGATE FAIL");
    Probe.GotoState('Active');
    if (Probe.IsInState('Active') && Probe.StateValue == 31)
    {
        ObjectStateResult = 1;
        Log("OPENUCC CLIENT STATE PASS");
    }
    else
        Log("OPENUCC CLIENT STATE FAIL");
    Log("OPENUCC CLIENT END");
    SaveConfig();
}

function RecordUIState(bool Success)
{
    UIStateResult = int(Success);
    SaveConfig();
}
