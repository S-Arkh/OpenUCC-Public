class CheckCommandlet extends Commandlet;

event int Main(string Parms)
{
    local Bootstrap B;
    local Wide W;
    local ChildProbe C;
    local PrimitiveProbe P;
    local LocalGrandchild G;
    local DeclarationReference D;
    local ArrayProbe A;
    local DynamicArrayProbe DA;
    local DynamicArrayValueProbe DV;
    local ExternalDynamicArrayProbe ED;
    local DynamicArrayBoolProbe DB;
    local DynamicArrayLengthOutReference DL;
    local ClassConfigReference CC;
    local ObjectChildProbe OC;
    local ObjectPeerProbe OP;
    local ExternalObjectProbe EO;
    local ObjectContextProbe CP;
    local ObjectContextReference CT;
    local ObjectContextLatePeer CL;
    local ClassObjectProbe CO;
    local ConstantChild KC;
    local EnumProbe EN;
    local StructProbe StructTest;
    local SwitchProbe SwitchTest;
    local int SwitchResult;
    local Int64StorageProbe Int64StorageTest;
    local HeaderReference HeaderTest;
    local DelegateProbe DelegateTest;
    local ExternalDelegateProbe ExternalDelegateTest;
    local PropertyShadowReference ShadowTest;
    local SelectorReference SelectorTest;
    local SyntaxReference SyntaxTest;
    local ContextEnumOwner EnumOwner;
    local ContextEnumReference EnumContext;
    local ReplicationChild ReplicationTest;
    local ExternalReplicationProbe ExternalReplicationTest;
    local ObjectArrayReference ObjectArrayTest;
    local array<ObjectArrayReference> DerivedObjects;
    local array<Object> BaseObjects;
    local FunctionClassCollisionReference CollisionTest;
    local DelegateOptionalReference OptionalDelegateTest;
    local ForwardTypeA ForwardTypes;
    local WithinProbe WithinTest;
    local AccessReference AccessTest;
    local ConstantFormsReference LiteralTest;
    local ExternalLinkProbe ExternalLinkTest;
    local ExternalLiteralProbe ExternalLiteralTest;
    local StructDefaultReference StructDefaults;
    local StructArrayProbe StructArrays;
    local NestedArrayDefaultReference NestedArrays;
    local StructDefaultChild StructDefaultsChild;
    local ExternalStructDefaultProbe StructDefaultsExternal;
    local StructNativeProbe NativeStructTest;
    local ExternalStructProbe ExternalStructTest;
    local EnumArrayDefaultProbe EnumDefaults;
    local ExternalEnumProbe EX;
    local ExternalConstantProbe KE;
    local ExternalClassObjectProbe EC;
    B = new class'Bootstrap';
    if (B.Count != 42 || !B.Active)
    {
        Log("OPEN_UCC_FAIL_DEFAULTS");
        return 1;
    }
    if (B.Choose(true, 91) != 91 || B.Choose(false, 91) != 17)
    {
        Log("OPEN_UCC_FAIL_BRANCH");
        return 2;
    }
    if (B.Sum(5) != 15 || B.Sum(-1) != 0 || B.InvertFlag(true) || !B.InvertFlag(false) || B.Negate(7) != -7)
    {
        Log("OPEN_UCC_FAIL_OPERATORS");
        return 5;
    }
    if (B.CallsProbe() != 22 || B.RecurseProbe(5) != 120 || B.ForProbe() != 8)
    {
        Log("OPEN_UCC_FAIL_CALLS_LOOPS");
        return 7;
    }
    B.Drain();
    if (B.Count != 1 || B.Active)
    {
        Log("OPEN_UCC_FAIL_WHILE");
        return 3;
    }
    B.Reset();
    if (B.Count != 0 || B.Active)
    {
        Log("OPEN_UCC_FAIL_ASSIGN");
        return 4;
    }
    W = new class'Wide';
    if (W.Test(true) != 456 || W.Test(false) != -987)
    {
        Log("OPEN_UCC_FAIL_WIDE_REFS");
        return 6;
    }
    C = new class'ChildProbe';
    if (C.InheritedValue != 9 || C.DispatchProbe() != 19 || C.CheckInheritedProbe() != 23)
    {
        Log("OPEN_UCC_FAIL_INHERITANCE");
        return 8;
    }
    if (C.ExternalCallsProbe() != 24 || C.SuperProbe() != 18 || C.SelfProbe(99) != 13)
    {
        Log("OPEN_UCC_FAIL_EXTERNAL_DISPATCH");
        return 9;
    }
    if (C.LogicProbe() != 2 || C.IncrementProbe() != 2)
    {
        Log("OPEN_UCC_FAIL_SHORT_CIRCUIT");
        return 10;
    }
    P = new class'PrimitiveProbe';
    if (P.ScalarByte != 255 || P.ScalarFloat != 1.25 || P.ScalarText != "Привет" || P.ScalarName != 'ProbeName')
    {
        Log("OPEN_UCC_FAIL_PRIMITIVE_DEFAULTS");
        return 11;
    }
    if (P.BuildText(12) != ">12 end" || P.UnicodeProbe() != ("Привет" $ " " $ "🚀") || P.MixedUnicodeProbe() != ("Привет" $ " " $ "🚀" $ " end") || P.EmptyProbe() != "" || P.UnicodeLengthProbe() != 9)
    {
        Log("OPEN_UCC_FAIL_STRINGS");
        return 12;
    }
    if (P.WidenProbe(3) != 3.5 || P.NarrowProbe(-3.75) != -3 || P.WrapByteProbe(259) != 3 || P.StringNumberProbe("42") != 42)
    {
        Log("OPEN_UCC_FAIL_PRIMITIVE_CASTS");
        return 13;
    }
    if (P.HasNameProbe('None') || !P.HasNameProbe('ProbeName') || P.NameTextProbe('ProbeName') != "ProbeName")
    {
        Log("OPEN_UCC_FAIL_NAME_CASTS");
        return 14;
    }
    G = new class'LocalGrandchild';
    if (G.Value != 9 || G.Label != "grandchild" || G.Dispatch() != 119 || G.SetValue(3) != 120 || G.Value != 3)
    {
        Log("OPEN_UCC_FAIL_LOCAL_INHERITANCE");
        return 15;
    }
    D = new class'DeclarationReference';
    D.EmptyProbe();
    D.ExecProbe();
    if (D.FinalProbe() != 11 || D.EventProbe() != 4 || D.First != 1 || D.Second != 2)
    {
        Log("OPEN_UCC_FAIL_DECLARATIONS");
        return 16;
    }
    D.SingularProbe();
    if (D.First != 3)
    {
        Log("OPEN_UCC_FAIL_SINGULAR");
        return 17;
    }
    A = new class'ArrayProbe';
    if (A.ReadProbe(0) != 10 || A.ReadProbe(2) != 30 || A.Texts[0] != "first" || A.Texts[2] != "last" || A.WideValue(127) != 127 || A.WideValue(128) != 128)
    {
        Log("OPEN_UCC_FAIL_ARRAY_DEFAULTS");
        return 18;
    }
    A.SetProbe(1, 17);
    if (A.ReadProbe(1) != 17 || A.LocalProbe() != 7 || A.ReadProbe(1) != 5 || A.Texts[1] != "changed")
    {
        Log("OPEN_UCC_FAIL_ARRAY_ELEMENTS");
        return 19;
    }
    DA = new class'DynamicArrayProbe';
    if (DA.LengthProbe()!=0 || DA.MutateProbe()!=0 || DA.MutateProbe()!=0 || DA.LocalProbe(2)!=7 || DA.LocalProbe(3)!=7)
    {
        Log("OPEN_UCC_FAIL_DYNAMIC_ARRAYS");
        return 20;
    }
    DA.ResizeProbe(3);
    DA.SetProbe(1,42);
    DA.InsertProbe(0,1);
    if (DA.LengthProbe()!=4 || DA.ReadProbe(2)!=42)
    {
        Log("OPEN_UCC_FAIL_DYNAMIC_ARRAY_CALLS");
        return 21;
    }
    DA.RemoveProbe(0,2);
    if (DA.LengthProbe()!=2 || DA.ReadProbe(0)!=42)
    {
        Log("OPEN_UCC_FAIL_DYNAMIC_ARRAY_REMOVE");
        return 22;
    }
    DV = new class'DynamicArrayValueProbe';
    if (DV.VerifyProbe()!=0 || DV.VerifyProbe()!=0)
    {
        Log("OPEN_UCC_FAIL_DYNAMIC_ARRAY_VALUES");
        return 23;
    }
    ED = new class'ExternalDynamicArrayProbe';
    if (ED.VerifyProbe()!=0)
    {
        Log("OPEN_UCC_FAIL_EXTERNAL_DYNAMIC_ARRAYS");
        return 24;
    }
    DB = new class'DynamicArrayBoolProbe';
    // Original SDK VM also leaves dynamic bool writes ineffective (owned oracle).
    if (DB.VerifyProbe()!=1 || DB.ReadProbe(0) || DB.ReadProbe(2))
    {
        Log("OPEN_UCC_FAIL_SDK_DYNAMIC_BOOL_PARITY");
        return 25;
    }
    DL = new class'DynamicArrayLengthOutReference';
    DL.IncrementProbe();
    // Original SDK compiles Length++, but its VM leaves Length unchanged.
    if (DL.Values.Length!=0)
    {
        Log("OPEN_UCC_FAIL_SDK_DYNAMIC_LENGTH_OUT_PARITY");
        return 26;
    }
    CC = new class'ClassConfigReference';
    if (CC.ReadProbe()!=73)
    {
        Log("OPEN_UCC_FAIL_CLASS_CONFIG");
        return 28;
    }
    OC = new class'ObjectChildProbe';
    OP = new class'ObjectPeerProbe';
    EO = new class'ExternalObjectProbe';
    if (OC.VerifyProbe(OP)!=0 || OP.IdentityProbe(OC)!=OC || OC.TextProbe(OC)!=string(OC) || !EO.VerifyProbe(OP))
    {
        Log("OPEN_UCC_FAIL_OBJECT_TYPES");
        return 29;
    }
    CP = new class'ObjectContextProbe';
    CT = new class'ObjectContextReference';
    CL = new class'ObjectContextLatePeer';
    CL.Owner=CP;
    if (CP.VerifyContext(CT)!=0 || CP.ForwardProbe(CL)!=17 || CL.ReadProbe()!=7 || !CP.NullContextProbe(None))
    {
        Log("OPEN_UCC_FAIL_OBJECT_CONTEXTS");
        return 30;
    }
    CO = new class'ClassObjectProbe';
    if (CO.VerifyProbe()!=0)
    {
        Log("OPEN_UCC_FAIL_CLASS_OBJECTS");
        return 31;
    }
    EC = new class'ExternalClassObjectProbe';
    if (!EC.VerifyProbe())
    {
        Log("OPEN_UCC_FAIL_EXTERNAL_CLASS_OBJECTS");
        return 32;
    }
    KC=new class'ConstantChild';
    KE=new class'ExternalConstantProbe';
    if (KC.CountProbe()!=3 || KC.CaptionProbe()!="constant" || KC.NegativeProbe()!=-17 || KC.RatioProbe()!=1.25 || KC.LabelProbe()!='Marker' || KC.Number!=3 || KC.Values[2]!=-17 || KC.InheritedProbe()!=-14 || KE.InheritedProbe()!=-14 || KE.Number!=-17)
    {
        Log("OPEN_UCC_FAIL_CONSTANTS");
        return 33;
    }
    EN=new class'EnumProbe';
    EnumDefaults=new class'EnumArrayDefaultProbe';
    EX=new class'ExternalEnumProbe';
    if (EN.VerifyProbe()!=0 || EX.VerifyProbe()!=2 || !EnumDefaults.VerifyDefaults())
    {
        Log("OPEN_UCC_FAIL_ENUMS");
        return 34;
    }
    StructTest=new class'StructProbe';
    NativeStructTest=new class'StructNativeProbe';
    ExternalStructTest=new class'ExternalStructProbe';
    if (StructTest.VerifyProbe()!=0 || !ExternalStructTest.VerifyProbe() || NativeStructTest.VerifyProbe()!=0)
    {
        Log("OPEN_UCC_FAIL_STRUCTS");
        return 35;
    }
    StructDefaults=new class'StructDefaultReference';
    StructDefaultsChild=new class'StructDefaultChild';
    StructDefaultsExternal=new class'ExternalStructDefaultProbe';
    if (StructDefaults.VerifyProbe()!=0 || !StructDefaultsChild.VerifyChild() || !StructDefaultsExternal.VerifyExternal())
    {
        Log("OPEN_UCC_FAIL_STRUCT_DEFAULTS");
        return 36;
    }
    StructArrays=new class'StructArrayProbe';
    NestedArrays=new class'NestedArrayDefaultReference';
    if (StructArrays.VerifyArrays()!=0 || !NestedArrays.VerifyProbe())
    {
        Log("OPEN_UCC_FAIL_STRUCT_ARRAYS");
        return 37;
    }
    ForwardTypes=new class'ForwardTypeA';
    WithinTest=new class'WithinProbe';
    AccessTest=new class'AccessReference';
    LiteralTest=new class'ConstantFormsReference';
    ExternalLinkTest=new class'ExternalLinkProbe';
    ExternalLiteralTest=new class'ExternalLiteralProbe';
    if (!ForwardTypes.VerifyProbe() || !WithinTest.VerifyProbe() || AccessTest.PublicProbe()!=0 || LiteralTest.ReadProbe().Z!=3 || LiteralTest.RotationProbe().Roll!=13 || !ExternalLinkTest.VerifyProbe() || !ExternalLiteralTest.VerifyProbe())
    {
        Log("OPEN_UCC_FAIL_TYPE_LINKING");
        return 38;
    }
    SwitchTest=new class'SwitchProbe';
    SwitchResult=SwitchTest.VerifyProbe();
    if (SwitchResult!=0)
    {
        Log("OPEN_UCC_FAIL_SWITCH" $ string(SwitchResult));
        return 39;
    }
    Int64StorageTest=new class'Int64StorageProbe';
    if (!Int64StorageTest.VerifyProbe())
    {
        Log("OPEN_UCC_FAIL_INT64_STORAGE");
        return 40;
    }
    HeaderTest=new class'HeaderReference';
    if (HeaderTest.ReadProbe()!=11 || HeaderTest.FunctionProbe()!=3)
    {
        Log("OPEN_UCC_FAIL_HEADER");
        return 41;
    }
    DelegateTest=new class'DelegateProbe';
    ExternalDelegateTest=new class'ExternalDelegateProbe';
    ShadowTest=new class'PropertyShadowReference';
    SelectorTest=new class'SelectorReference';
    SyntaxTest=new class'SyntaxReference';
    EnumOwner=new class'ContextEnumOwner';
    EnumContext=new class'ContextEnumReference';
    EnumContext.SetProbe(EnumOwner);
    OptionalDelegateTest=new class'DelegateOptionalReference';
    OptionalDelegateTest.BindProbe();
    if (OptionalDelegateTest.CallProbe()!=7)
    {
        Log("OPEN_UCC_FAIL_OPTIONAL_DELEGATE");
        return 48;
    }
    ObjectArrayTest=new class'ObjectArrayReference';
    CollisionTest=new class'FunctionClassCollisionReference';
    DerivedObjects.Length=1;
    DerivedObjects[0]=ObjectArrayTest;
    BaseObjects=ObjectArrayTest.ReturnProbe(DerivedObjects);
    if (ObjectArrayTest.CallProbe(DerivedObjects)!=1 || BaseObjects[0]!=ObjectArrayTest || CollisionTest.CallProbe()!=7 || !EnumContext.CastProbe(EnumOwner))
    {
        Log("OPEN_UCC_FAIL_OBJECT_ARRAY");
        return 47;
    }
    ReplicationTest=new class'ReplicationChild';
    ExternalReplicationTest=new class'ExternalReplicationProbe';
    ReplicationTest.Update(7);
    ExternalReplicationTest.Update(7);
    if (ReplicationTest.Counter!=8 || ExternalReplicationTest.Counter!=9)
    {
        Log("OPEN_UCC_FAIL_REPLICATION_CALL");
        return 46;
    }
    if (!EnumContext.CompareProbe(EnumOwner) || EnumContext.ReturnProbe()!=1 || EnumContext.Numbers[0]!=7 || EnumContext.Numbers[1]!=9)
    {
        Log("OPEN_UCC_FAIL_CONTEXT_ENUM");
        return 45;
    }
    SelectorTest.Yellow03=19;
    if (SelectorTest.FieldProbe()!=19 || SelectorTest.ObjectProbe(SelectorTest,SelectorTest)!=7 || SelectorTest.BoolProbe(false)!=3 || SelectorTest.ConstantProbe(SelectorTest)!=7 || SyntaxTest.PartialProbe(false)!=0 || SyntaxTest.PartialProbe(true)!=7)
    {
        Log("OPEN_UCC_FAIL_SYNTAX");
        return 44;
    }
    if (!DelegateTest.VerifyProbe() || !ExternalDelegateTest.VerifyProbe() || !ShadowTest.VerifyProbe())
    {
        Log("OPEN_UCC_FAIL_DELEGATE");
        return 43;
    }
    Log("OPEN_UCC_VM_PASS");
    return 0;
}

defaultproperties
{
    IsClient=False
    IsEditor=True
    IsServer=False
    LazyLoad=False
    ShowBanner=False
}
