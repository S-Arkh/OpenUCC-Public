class ConstantReference extends Object;
const Count = 3;
const Caption = "constant";
const Negative = -17;
const Ratio = 1.25;
const Label = 'Marker';
var int Values[Count];
var int Number;
function int CountProbe() { return Count; }
function string CaptionProbe() { return Caption; }
function int NegativeProbe() { return Negative; }
function float RatioProbe() { return Ratio; }
function name LabelProbe() { return Label; }
defaultproperties { Number=Count Values(2)=Negative }
