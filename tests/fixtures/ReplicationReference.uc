class ReplicationReference extends Object;
var bool Enabled;
var int Counter;
var int Other;
replication
{
    reliable if (Enabled) Counter, Update;
    unreliable if (!Enabled) Other, Notify;
}
function Update(int V) { Counter=V; }
function Notify() { Other=3; }
