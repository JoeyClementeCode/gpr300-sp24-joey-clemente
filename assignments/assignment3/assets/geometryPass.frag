#version 450
layout (location = 0) out vec3 gWorldPos;
layout (location = 1) out vec3 gWorldNormal;
layout (location = 2) out vec3 gAlbedo;

in vec2 UV;
in vec3 WorldPos;
in vec3 WorldNormal;

uniform sampler2D _MainTex;

void main()
{
	gWorldPos = WorldPos;
	gWorldNormal = normalize(WorldNormal);
	gAlbedo = texture(_MainTex, UV).rgb;
}
