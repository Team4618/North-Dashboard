uniform mat4 Matrix;

layout(location = POSITION_SLOT) in vec2 InPosition;
layout(location = UV_SLOT) in vec2 InUV;
layout(location = COLOUR_SLOT) in vec4 InColour;

out VertexOut {
   vec2 UV;
   vec4 Colour;
} Out;

void main() {
	gl_Position = Matrix * vec4(InPosition, 0, 1);
	Out.UV = InUV;
   Out.Colour = InColour;
}