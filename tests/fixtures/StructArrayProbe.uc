class StructArrayProbe extends StructArrayReference;
function int VerifyArrays()
{
    local Item V;
    local array<Item> Copy;
    V.Number=41;
    V.Text="value";
    Items.Length=2;
    WriteProbe(0,V);
    V.Text="changed";
    if (ReadProbe(0).Text!="value" || Items[1].Number!=0 || Items[1].Text!="") { return 1; }
    InsertProbe(0);
    if (Items.Length!=3 || Items[0].Text!="" || Items[1].Number!=41) { return 2; }
    OutProbe(Items[0]);
    Copy=CopyProbe(Items);
    RemoveProbe(0);
    if (Items[0].Text!="value" || Copy[0].Text!="out" || Copy[1].Number!=41) { return 3; }
    Items.Length=0;
    Items.Length=2;
    if (Items[0].Text!="" || Items[1].Number!=0) { return 4; }
    ArrayOutProbe(Copy);
    if (Copy.Length!=1 || Copy[0].Number!=59 || Copy[0].Text!="array-out") { return 5; }
    return 0;
}
