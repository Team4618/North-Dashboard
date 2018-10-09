#version 450

uniform sampler2D Texture;

in VertexOut {
   vec2 UV;
} In;

layout(location = 0) out vec4 OutColor;

void main()
{
	OutColor = texture(Texture, In.UV);
}