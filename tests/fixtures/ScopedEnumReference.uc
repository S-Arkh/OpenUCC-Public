class ScopedEnumReference extends Object;
enum EFirst { First, Max };
enum ESecond { Second, Max };
function byte FirstProbe() { return EFirst.Max; }
function byte SecondProbe() { return ESecond.Max; }
