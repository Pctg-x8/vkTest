#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 pos;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 color_out;
out gl_PerVertex { vec4 gl_Position; };

void main()
{
	gl_Position = vec4(pos, 0.0f, 1.0f);
	color_out = color;
}
