class StateOverrideChild extends StateSameNameParent;
state Active
{
    function int Value() { return super.Value() + 1; }
}
