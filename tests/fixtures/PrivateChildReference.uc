class PrivateChildReference extends PrivateParentReference;
private function int Hidden() { return 2; }
function int ChildProbe() { return Hidden(); }
