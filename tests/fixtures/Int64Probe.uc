class Int64Probe extends Int64Reference;
function int VerifyProbe()
{
    local INT64 A;
    local array<INT64> Copy;
    if (string(Value)!="5000000000" || string(Fixed[1])!="-9223372036854775808" || string(Items[0])!="9223372036854775807") return 1;
    if (string(LiteralProbe())!="1152921504606846976" || string(MinimumProbe())!="-9223372036854775808") return 2;
    if (AddProbe(Value,7)!=5000000007 || SubtractProbe(Value,1)!=4999999999 || NegateProbe(Value)!=-5000000000) return 3;
    if (MultiplyProbe(Value,2)!=10000000000 || DivideProbe(Value,2)!=2500000000 || ScaleProbe(Value,0.5)!=2500000000) return 4;
    A=Value;
    AddOutProbe(A,7);
    SubtractOutProbe(A,3);
    if (A!=5000000004 || !GreaterProbe(A,Value) || !LessProbe(Value,A) || !DifferentProbe(A,Value)) return 5;
    if (!CompareProbe(Value,Value) || !LessEqualProbe(Value,Value) || !GreaterEqualProbe(Value,Value)) return 6;
    if (ByteProbe(7)!=7 || IntProbe(-7)!=-7 || BoolProbe(true)!=1 || FloatProbe(7.5)!=7 || StringProbe("5000000000")!=Value) return 7;
    if (ToByteProbe(7)!=7 || ToIntProbe(-7)!=-7 || !ToBoolProbe(Value) || ToFloatProbe(7)!=7.0 || ToStringProbe(Value)!="5000000000") return 8;
    Items.Length=2;
    SetProbe(Items[1]);
    Copy=Items;
    Items[1]=7;
    if (Copy[1]!=Value || Items[1]!=7) return 9;
    return 0;
}
