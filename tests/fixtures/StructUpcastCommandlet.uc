class StructUpcastCommandlet extends Commandlet dependsOn(StructReference);
event int Main(string Parms)
{
    local StructReference Target;
    local StructReference.RichData Value;
    Target=new class'StructReference';
    Value.Value=51;
    Value.Text="inherited";
    Value.Ratio=1.5;
    Log("STRUCT_ORACLE_BEFORE_UPCAST");
    if (Target.UpcastProbe(Value).Text!="inherited") { return 1; }
    Log("STRUCT_ORACLE_UPCAST_PASS");
    return 0;
}
