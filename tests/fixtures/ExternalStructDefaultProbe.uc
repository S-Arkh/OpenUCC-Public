class ExternalStructDefaultProbe extends ParentPkg.StructDefaultReference;
function bool VerifyExternal()
{
    return Data.Item.Number==43 && Data.Item.Text=="nested" && Items.Length==3 && Items[0].Number==31 && Items[0].Text=="external" && Items[2].Number==37 && Tint.R==71 && Tint.G==73;
}
defaultproperties
{
    Data=(Item=(Number=43))
    Items(0)=(Text="external")
    Tint=(G=73)
}
