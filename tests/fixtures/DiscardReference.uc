class DiscardReference extends Object;
var int Value;
function DiscardReference GetPeer() { return self; }
function Probe() { GetPeer().Value; }
