class StructDefaultReference extends Object;
struct Leaf { var int Number; var string Text; var bool Enabled; };
struct Bundle { var Leaf Item; var int Fixed[2]; };
var Bundle Data;
var Leaf Fixed[2];
var array<Leaf> Items;
var Vector Position;
var Color Tint;
function int VerifyProbe()
{
    if (Data.Item.Number!=17 || Data.Item.Text!="nested" || !Data.Item.Enabled || Data.Fixed[1]!=23) { return 1; }
    if (Fixed[1].Number!=29 || Fixed[1].Text!="fixed") { return 2; }
    if (Items.Length!=3 || Items[0].Text!="first" || Items[1].Number!=0 || Items[2].Number!=37) { return 3; }
    if (Position.X!=1.5 || Position.Z!=2.5 || Tint.R!=71 || Tint.A!=255) { return 4; }
    return 0;
}
defaultproperties
{
    Data=(Item=(Number=17,Text="nested",Enabled=True),Fixed[1]=23)
    Fixed(1)=(Number=29,Text="fixed")
    Items(0)=(Number=31,Text="first")
    Items(2)=(Number=37,Text="third")
    Position=(X=1.5,Z=2.5)
    Tint=(R=71,A=255)
}
