#version 300 es
precision highp float;

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

in vec4 FragPosLightSpace;

// A Textura que guardou as profundidades do Sol (O Shadow Map)
uniform sampler2D shadowMap;

uniform sampler2D diffuseMap;
uniform sampler2D specularMap;
uniform bool uHasSpecularMap;

uniform vec4 uBaseColor;
uniform bool uAffectedByLight;

uniform float uSpecularStrength;
uniform float uShininess;

uniform sampler2D normalMap;
uniform bool uHasNormalMap;
in mat3 TBN;

// Sistema de Múltiplas Luzes
#define MAX_LIGHTS 10
uniform vec3 lightPos[MAX_LIGHTS];
uniform vec3 lightColor[MAX_LIGHTS];
uniform int numLights; 

uniform vec3 viewPos;

uniform sampler2D skybox;       // A textura do nosso céu
uniform float uReflectivity;

uniform vec3 uAmbientColor;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Executa a divisão de perspectiva
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transforma as coordenadas de [-1, 1] para [0, 1] (padrão de texturas)
    projCoords = projCoords * 0.5 + 0.5;
    
    // Se o pixel estiver muito longe (fora do alcance do sol), força 0.0 (sem sombra)
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; 
    }
    
    float currentDepth = projCoords.z;
    
    // Calcula o "Bias" baseado no ângulo do Sol para evitar listras pretas
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    // Fazemos uma média de 9 pixels vizinhos (PCF) para a sombra não ficar serrilhada
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

void main()
{
    vec4 texColor = texture(diffuseMap, TexCoord);
    vec4 baseResult = texColor * uBaseColor;

    if (!uAffectedByLight) {
        FragColor = baseResult;
        return;
    }

    vec3 norm = normalize(Normal); // Padrão
    if (uHasNormalMap) {
        // 1. Lê a cor RGB do mapa (tons de azul/roxo)
        norm = texture(normalMap, TexCoord).rgb;
        // 2. Transforma a cor (0.0 a 1.0) em vetor de direção (-1.0 a 1.0)
        norm = normalize(norm * 2.0 - 1.0);
        // 3. Usa a Matriz TBN para girar esse relevo e alinhá-lo com a face do objeto 3D
        norm = normalize(TBN * norm);
    }

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 ambient = uAmbientColor;
    
    vec3 sunDir = normalize(-uSunDirection); 

    float shadow = ShadowCalculation(FragPosLightSpace, norm, sunDir);

    float sunDiff = max(dot(norm, sunDir), 0.0);
    vec3 sunDiffuse = sunDiff * uSunColor * uSunIntensity * (1.0f - shadow);

    vec3 sunReflectDir = reflect(-sunDir, norm);
    float sunSpec = pow(max(dot(viewDir, sunReflectDir), 0.0), uShininess); 
    
    float mapSpecular = 1.0;
    if (uHasSpecularMap) {
        mapSpecular = texture(specularMap, TexCoord).r;
    }
    vec3 sunSpecular = uSpecularStrength * mapSpecular * sunSpec * uSunColor * uSunIntensity * (1.0 - shadow);

    // Inicializamos as "caixas de soma" já com o valor do sol!
    vec3 totalDiffuse = sunDiffuse;
    vec3 totalSpecular = sunSpecular;

    for(int i = 0; i < numLights; i++) {
        vec3 lightDir = normalize(lightPos[i] - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor[i];

        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess); 
        
        float mapSpecular = 1.0; 
        if (uHasSpecularMap) {
            mapSpecular = texture(specularMap, TexCoord).r;
        }
        
        vec3 specular = uSpecularStrength * mapSpecular * spec * lightColor[i];

        float distance = length(lightPos[i] - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

        totalDiffuse += diffuse * attenuation;
        totalSpecular += specular * attenuation;
    }

    vec3 objectIllumination = (ambient + totalDiffuse) * baseResult.rgb;
    
    // 2. O reflexo especular é adicionado POR CIMA (additive blending)
    vec3 finalColor = objectIllumination + totalSpecular;

    vec3 I = -viewDir;
    vec3 R = reflect(I, norm);
    
    // Usamos a mesma fórmula mágica aqui! (Não esqueça de colocar invAtan lá no topo ou aqui)
    vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 envUV = vec2(atan(R.z, R.x), asin(R.y));
    envUV *= invAtan;
    envUV += 0.5;

    // Lê a textura 2D panorâmica
    vec3 reflectionColor = texture(skybox, envUV).rgb;

    // Mistura o reflexo
    finalColor = mix(finalColor, reflectionColor, uReflectivity);

    FragColor = vec4(finalColor, baseResult.a);
}