class DynamicArrayReference extends Object;
var array<int> Values;
var array<string> Texts;
function int ReadProbe(int I) { return Values[I]; }
function SetProbe(int I, int V) { Values[I]=V; }
function int LengthProbe() { return Values.Length; }
function ResizeProbe(int N) { Values.Length=N; }
function InsertProbe(int I, int N) { Values.Insert(I,N); }
function RemoveProbe(int I, int N) { Values.Remove(I,N); }
function int LocalProbe(int N)
{
    local array<int> Work;
    Work.Length=N;
    Work[0]=7;
    return Work[0];
}
