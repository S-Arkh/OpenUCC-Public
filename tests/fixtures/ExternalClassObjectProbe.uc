class ExternalClassObjectProbe extends ParentPkg.ClassObjectReference;
function bool VerifyProbe()
{
    local class<ParentPkg.ClassObjectReference> T;
    local ParentPkg.ClassObjectReference Created;
    T=class'ParentPkg.ClassObjectReference';
    Created=new T;
    return T.default.Value==41 && T.static.StaticProbe(7)==7 && Created.Value==41 && Factory==T;
}
