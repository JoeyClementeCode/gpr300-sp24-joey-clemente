#version 450

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D _ScreenTex;

void main()
{ 
    FragColor = texture(_ScreenTex, TexCoords);
}