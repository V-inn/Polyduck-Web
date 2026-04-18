#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos; // Num Cubemap, a própria posição 3D serve como coordenada UV!
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // Truque para o céu ficar sempre no fundo
}