#version 450

out vec4 FragColor;

in vec2 UV;

uniform sampler2D _ColorBuffer;

uniform float _Exposure;
uniform float _Contrast;
uniform float _Brightness;
uniform vec3 _ColorFiltering;

float greyscale(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 tonemap(vec3 col)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return ((col*(a*col+b))/(col*(c*col+d)+e));
}

// Color Correction + Narcowicz ACES Tonemapping
void main()
{
    vec3 col = texture(_ColorBuffer, UV).rgb;

    // Exposure
    col = max(vec3(0), col * _Exposure);

    // Contrast + Brightness
    col = max(vec3(0), _Contrast * (col - 0.5) + 0.5 + _Brightness);

    col = clamp(tonemap(col), 0.0, 1.0);
                
    FragColor = vec4(col, 1.0);
}
