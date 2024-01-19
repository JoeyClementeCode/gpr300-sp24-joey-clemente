#version 450

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D _ScreenTex;

void main()
{ 
    FragColor = vec4(vec3(1.0 - texture(_ScreenTex, TexCoords)), 1.0);
}