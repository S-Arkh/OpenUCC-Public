class EnumArrayDefaultProbe extends EnumReference;
function bool VerifyDefaults()
{
    return Modes.Length==2 && Modes[0]==ModeActive && Modes[1]==ModeDone;
}
defaultproperties
{
    Modes(0)=ModeActive
    Modes(1)=ModeDone
}
