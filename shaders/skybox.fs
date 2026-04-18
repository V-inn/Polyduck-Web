#version 300 es
precision highp float;

out vec4 FragColor;

in vec3 TexCoords;

uniform sampler2D skybox; 

// --- VARIÁVEIS DO SOL ---
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
// ------------------------

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 viewDir = normalize(TexCoords);
    
    // 1. Lê a cor normal do panorama
    vec2 uv = SampleSphericalMap(viewDir);
    vec3 skyColor = texture(skybox, uv).rgb;

    // 2. A MATEMÁTICA DO SOL
    vec3 sunDir = normalize(-uSunDirection); // A posição da fonte de luz
    
    // O dot product retorna 1.0 se estivermos olhando exatamenta para o sol
    float sunAlignment = max(dot(viewDir, sunDir), 0.0);

    // pow() espreme esse valor. Quanto maior o expoente, menor e mais duro fica o disco do sol.
    // 800.0 cria um disco sólido, 100.0 cria um halo de brilho ao redor.
    float sunDisk = pow(sunAlignment, 800.0); 
    float sunGlow = pow(sunAlignment, 100.0) * 0.3; // 0.3 para o brilho ser suave

    // 3. Adiciona o sol por cima da imagem do céu (Additive Blending)
    vec3 finalColor = skyColor + (uSunColor * sunDisk) + (uSunColor * sunGlow);

    FragColor = vec4(finalColor, 1.0);
}