#version 450

out vec4 FragColor;

in vec2 UV;

uniform sampler2D _ColorBuffer;

// Narcowicz ACES Tonemapping
void main()
{
    vec3 col = texture(_ColorBuffer, UV).rgb;

    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    vec3 result = ((col*(a*col+b))/(col*(c*col+d)+e));

	FragColor = vec4(clamp(result, 0.0, 1.0), 1.0f);

    //FragColor = vec4(col, 1.0);
}
