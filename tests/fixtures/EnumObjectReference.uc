class EnumObjectReference extends Object;
enum Choice { First, Second };
function Object LocalProbe() { return Enum'ReferencePkg.EnumObjectReference.Choice'; }
function Object ExternalProbe() { return Enum'Core.Object.ECamOrientation'; }
