class StructDefaultChild extends StructDefaultReference;
function bool VerifyChild()
{
    return Data.Item.Number==41 && Data.Item.Text=="nested" && Data.Item.Enabled && Data.Fixed[1]==23 && Items.Length==3 && Items[0].Number==31 && Items[0].Text=="changed" && Items[2].Number==37 && Position.X==1.5 && Position.Y==7.5;
}
defaultproperties
{
    Data=(Item=(Number=41))
    Items(0)=(Text="changed")
    Position=(Y=7.5)
}
