#version 450
layout (location = 0) in vec3 vPos;

uniform mat4 _LightViewMatrix;

uniform mat4 _Model;

void main()
{
	gl_Position = _LightViewMatrix * _Model * vec4(vPos, 1.0);
}
