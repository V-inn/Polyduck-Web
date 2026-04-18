#version 300 es
precision highp float;

out vec4 FragColor;

// Recebe a cor sólida da CPU
uniform vec3 uColor; 

void main()
{
    FragColor = vec4(uColor, 1.0); // Output de cor sólida
}