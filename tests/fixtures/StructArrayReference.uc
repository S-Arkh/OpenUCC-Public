class StructArrayReference extends Object;
struct Item { var int Number; var string Text; };
var array<Item> Items;
function array<Item> CopyProbe(array<Item> V) { return V; }
function Item ReadProbe(int I) { return Items[I]; }
function WriteProbe(int I, Item V) { Items[I]=V; }
function InsertProbe(int I) { Items.Insert(I,1); }
function RemoveProbe(int I) { Items.Remove(I,1); }
function OutProbe(out Item V) { V.Number=53; V.Text="out"; }
function ArrayOutProbe(out array<Item> V) { V.Length=1; V[0].Number=59; V[0].Text="array-out"; }
