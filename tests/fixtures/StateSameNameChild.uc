// Negative oracle: UCC rejects explicit extends on a same-name state override.
class StateSameNameChild extends StateSameNameParent;
state Active extends Active
{
    function int Value() { return super.Value() + 1; }
}
