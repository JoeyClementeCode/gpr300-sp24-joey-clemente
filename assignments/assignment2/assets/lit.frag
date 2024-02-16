#version 450

out vec4 FragColor;

in Surface{
	vec3 WorldPos;
	vec3 WorldNormal;
	vec2 TexCoord;
}fs_in;

in vec4 LightSpacePos;

uniform vec3 _EyePos;
uniform vec3 _LightDirection = vec3(0.0, -1.0, 0.0);
uniform vec3 _LightColor = vec3(1.0);
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);
uniform sampler2D _ShadowMap;

uniform float minBias = 0.005;
uniform float maxBias = 0.015;

// Coefficients for editor
struct Material
{
	float AmbientCo;
	float DiffuseCo;
	float SpecualarCo;
	float Shininess;
};

uniform Material _Material;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias)
{
	vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
	sampleCoord = sampleCoord * 0.5 + 0.5;

	float myDepth = sampleCoord.z - bias;
	float shadowMapDepth = texture(shadowMap, sampleCoord.xy).r;

	return step(shadowMapDepth, myDepth);
}

void main()
{
	vec3 normal = normalize(fs_in.WorldNormal);

	vec3 toLight = -_LightDirection;
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);

	// Diffuse
	float diffuseFactor = max(dot(normal, toLight), 0.0);
	vec3 diffuseColor = _LightColor * diffuseFactor;

	// Specular
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	vec3 lightColor = (_Material.DiffuseCo * diffuseFactor + _Material.SpecualarCo * specularFactor) * _LightColor;

	// Ambient
	lightColor += _AmbientColor * _Material.AmbientCo;

	float bias = max(maxBias * (1.0 - dot(normal, toLight)), minBias);
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias);

	vec3 light = lightColor * (1.0 - shadow);

	FragColor = vec4(light, 1.0);
}