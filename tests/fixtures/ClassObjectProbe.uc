class ClassObjectProbe extends Object;
function int VerifyProbe()
{
    local class<ClassObjectReference> T;
    local ClassObjectReference A, B;
    T=class'ClassObjectChild';
    A=new T;
    B=new(self,"NamedByOpenUCC",0) class'ClassObjectReference';
    if (A==None || B==None || A.Value!=63 || B.Value!=41) { return 1; }
    if (B.Name!='NamedByOpenUCC' || B.Outer!=self) { return 2; }
    if (T.default.Value!=63 || T.static.StaticProbe(7)!=107) { return 3; }
    if (A.OwnDefaultProbe()!=63 || A.DefaultProbe(T)!=63 || A.CallProbe(T)!=107) { return 4; }
    if (A.Factory!=class'ClassObjectReference' || A.LiteralProbe()!=class'ClassObjectReference') { return 5; }
    if (A.CastProbe(T)!=T || A.CastProbe(self)!=None) { return 6; }
    if (ClassObjectReference(A.FactoryProbe(T)).Value!=63 || ClassObjectReference(A.MakeProbe()).Value!=41) { return 7; }
    return 0;
}
