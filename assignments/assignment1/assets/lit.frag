#version 450

out vec4 FragColor;

in Surface{
	vec3 WorldPos;
	vec3 WorldNormal;
	vec2 TexCoord;
}fs_in;

uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);

// Coefficients for editor
struct Material
{
	float AmbientCo;
	float DiffuseCo;
	float SpecualarCo;
	float Shininess;
};

uniform Material _Material;

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

	FragColor = vec4(lightColor, 1.0);
}