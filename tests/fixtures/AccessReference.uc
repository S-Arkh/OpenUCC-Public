class AccessReference extends Object;
var private int Hidden;
var protected int Family;
private function int SecretProbe() { return Hidden; }
protected function int FamilyProbe() { return Family; }
function int PublicProbe() { return SecretProbe()+FamilyProbe(); }
