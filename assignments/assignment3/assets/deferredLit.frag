#version 450
layout (location = 0) out vec4 FragColor;

in vec2 UV;

vec4 LightSpacePos;
uniform mat4 _LightViewProjection;

uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);
uniform sampler2D _ShadowMap;
uniform sampler2D _MainTex;

uniform float _MinBias = 0.02;
uniform float _MaxBias = 0.2;

// Coefficients for editor
struct Material
{
	float AmbientCo;
	float DiffuseCo;
	float SpecualarCo;
	float Shininess;
};

uniform Material _Material;

uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;


float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias)
{
	vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
	sampleCoord = sampleCoord * 0.5 + 0.5;

	float myDepth = sampleCoord.z - bias;

	float totalShadow = 0;
	vec2 texelOffset = 1.0 / textureSize(_ShadowMap,0);

	for(int y = -1; y <=1; y++)
	{
		for(int x = -1; x <=1; x++)
		{
			vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow+=step(texture(_ShadowMap,uv).r,myDepth);
		}
	}

	totalShadow /= 9.0;

	return totalShadow;
}


vec3 calculateLighting(vec3 normal, vec3 worldPos, vec3 albedo, vec4 LightSpacePos)
{

	vec3 toLight = -_LightDirection;
	vec3 toEye = normalize(_EyePos - worldPos);

	// Diffuse
	float diffuseFactor = max(dot(normal, toLight), 0.0);
	vec3 diffuseColor = _LightColor * diffuseFactor;

	// Specular
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	vec3 lightColor = (_Material.DiffuseCo * diffuseFactor + _Material.SpecualarCo * specularFactor) * _LightColor;

	// Ambient
	lightColor += _AmbientColor * _Material.AmbientCo;

	float bias = max(_MaxBias * (1.0 - dot(normal, toLight)), _MinBias);
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias);

	vec3 objectColor = albedo;

	vec3 light = lightColor * (1.0 - shadow);

	return objectColor * light;
}

void main()
{
	//Sample surface properties for this screen pixel
	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 albedo = texture(_gAlbedo,UV).xyz;

	// Light Space
	LightSpacePos = _LightViewProjection * vec4(worldPos, 1);

	//Worldspace lighting calculations, same as in forward shading
	vec3 lightColor = calculateLighting(normal,worldPos,albedo,LightSpacePos);
	FragColor = vec4(albedo * lightColor,1.0);
}




