class SortReference extends Object;
var array<int> Values;
delegate int Compare(int A, int B) { return A-B; }
function SortProbe() { Values.Sort(Compare); }
