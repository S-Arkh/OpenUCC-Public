class StructNativeProbe extends StructNativeReference;
function int VerifyProbe()
{
    local Vector A, B;
    A.X=3;
    A.Y=5;
    A.Z=7;
    B=AddProbe(A,A);
    Position=B;
    Rotation.Yaw=17;
    if (ReadProbe(B)!=6 || B.Y!=10 || Position.Z!=14 || Rotation.Yaw!=17 || !EqualProbe(B,Position)) { return 1; }
    return 0;
}
