class ExternalStateProbe extends ParentPkg.StateReference;
state Idle
{
    function int ReadProbe() { return super.ReadProbe(); }
}
