class ClientSortProbe extends Object config(OpenUCCSortProbe);
var array<int> Values;
var config int SortResult;
var config int Stage, Calls, Count, First, Second, Third, Fourth, Fifth;
delegate int Compare(int A, int B) { Calls++; return A-B; }
delegate int ReverseCompare(int A, int B) { Calls++; return B-A; }
static function Run()
{
    local ClientSortProbe Probe;
    Probe = new class'ClientSortProbe';
    Probe.CheckSort();
}
function CheckSort()
{
    SortResult = 0;
    Stage = 1;
    Calls = 0;
    SaveConfig();
    Values.Length = 5;
    Values[0] = 7;
    Values[1] = -2;
    Values[2] = 7;
    Values[3] = 0;
    Values[4] = 3;
    Values.Sort(Compare);
    Stage = 2;
    Count = Values.Length;
    First = Values[0];
    Second = Values[1];
    Third = Values[2];
    Fourth = Values[3];
    Fifth = Values[4];
    SaveConfig();
    // Client Sort places positive comparator results first.
    if (Calls > 0 && Values.Length == 5 && Values[0] == 7 && Values[1] == 7 && Values[2] == 3 && Values[3] == 0 && Values[4] == -2)
    {
        Values.Sort(ReverseCompare);
        Stage = 3;
        if (Values[0] != -2 || Values[1] != 0 || Values[2] != 3 || Values[3] != 7 || Values[4] != 7)
        {
            SaveConfig();
            return;
        }
        Values.Length = 0;
        Values.Sort(Compare);
        Stage = 4;
        if (Values.Length == 0) SortResult = 1;
    }
    SaveConfig();
    Log("OPENUCC CLIENT SORT " $ string(SortResult));
}
