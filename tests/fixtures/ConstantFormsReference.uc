class ConstantFormsReference extends Object;
const Base=3;
const Position=vect(1,2,3);
const Angles=rot(7,11,13);
const Factory=class'Object';
function vector ReadProbe() { return Position; }
function rotator RotationProbe() { return Angles; }
function class<Object> FactoryProbe() { return Factory; }
