class PrimitiveReference extends Object;

function int ByteToIntProbe(byte V) { return int(V); }
function bool ByteToBoolProbe(byte V) { return bool(V); }
function float ByteToFloatProbe(byte V) { return float(V); }
function byte IntToByteProbe(int V) { return byte(V); }
function bool IntToBoolProbe(int V) { return bool(V); }
function float IntToFloatProbe(int V) { return float(V); }
function byte BoolToByteProbe(bool V) { return byte(V); }
function int BoolToIntProbe(bool V) { return int(V); }
function float BoolToFloatProbe(bool V) { return float(V); }
function byte FloatToByteProbe(float V) { return byte(V); }
function int FloatToIntProbe(float V) { return int(V); }
function bool FloatToBoolProbe(float V) { return bool(V); }
function bool NameToBoolProbe(name V) { return bool(V); }
function byte StringToByteProbe(string V) { return byte(V); }
function int StringToIntProbe(string V) { return int(V); }
function bool StringToBoolProbe(string V) { return bool(V); }
function float StringToFloatProbe(string V) { return float(V); }
function string ByteToStringProbe(byte V) { return string(V); }
function string IntToStringProbe(int V) { return string(V); }
function string BoolToStringProbe(bool V) { return string(V); }
function string FloatToStringProbe(float V) { return string(V); }
function string NameToStringProbe(name V) { return string(V); }
