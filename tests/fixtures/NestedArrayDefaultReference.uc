class NestedArrayDefaultReference extends Object;
struct Item { var int Number; var string Text; };
struct Container { var array<Item> Items; };
var Container Data;
function bool VerifyProbe() { return Data.Items.Length==2 && Data.Items[0].Number==3 && Data.Items[1].Text=="nested"; }
defaultproperties
{
    Data=(Items=((Number=3),(Text="nested")))
}
