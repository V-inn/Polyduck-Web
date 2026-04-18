//TODO: - Refatorar o código para separar melhor as responsabilidades:
// - main.cpp: Apenas o loop de renderização e callbacks
// - Criar uma classe "Renderer" para lidar com a configuração do OpenGL, shaders e renderização em geral
// - Criar uma classe "InputManager" para lidar com o teclado e mouse

#include <GLFW/glfw3.h>
#include <iostream>
#include "graphics/Shader.h"
#include "graphics/Primitives.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include "graphics/Model.h"
#include "graphics/Camera.h"
#include "graphics/SceneState.h"
#include "graphics/UiManager.h"
#include "scene/Scene.h"
#include "graphics/TextureLoader.h"
#include <filesystem>
#include <functional> 
#include <imgui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

// Isso faz o compilador apagar qualquer chamada de glPolygonMode na Web!
#define glPolygonMode(face, mode) 
#define GL_LINE 0
#else
#include <glad/glad.h>
#endif

namespace fs = std::filesystem;

// Instancia a câmera globalmente
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

// Variáveis de controle do mouse e tempo
float lastX = 800.0f / 2.0;
float lastY = 600.0f / 2.0;
bool firstMouse = true;
bool isDragging = false;

// Variáveis de tempo (DeltaTime garante que a velocidade seja a mesma em qualquer PC)
float deltaTime = 0.0f; 
float lastFrame = 0.0f;

bool uiMode = false; // Começa no modo Câmera (falso)
bool tabKeyPressed = false; // Para evitar que a tecla ative várias vezes num único clique

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            isDragging = true;
            firstMouse = true; // Previne o pulo inicial
        } else if (action == GLFW_RELEASE) {
            isDragging = false;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    // Só ignora a UI se estivermos apenas passeando com o mouse
    if (!isDragging && ImGui::GetIO().WantCaptureMouse) return;

    // Se não estivermos segurando o botão, não faz nada
    if (!isDragging) return; 

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 

    lastX = xpos;
    lastY = ypos;
    
    camera.ProcessMouseMovement(xoffset*2.0f, yoffset*2.0f);
}

void processInput(GLFWwindow *window) {
    // Tecla ESC para sair (útil no Desktop)
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Só permite o WASD se o usuário NÃO estiver digitando num campo de texto da UI
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera.ProcessKeyboard(DOWN, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camera.ProcessKeyboard(UP, deltaTime);
    }
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {
#ifndef __EMSCRIPTEN__
    // Garante que a pasta existe (CÓDIGO ORIGINAL DESKTOP)
    if (!fs::exists("./user_assets")) {
        fs::create_directory("./user_assets");
    }

    for (int i = 0; i < count; i++) {
        fs::path sourcePath(paths[i]); 
        fs::path destinationPath = fs::path("./user_assets") / sourcePath.filename();

        try {
            fs::copy(sourcePath, destinationPath, fs::copy_options::overwrite_existing);
            std::cout << "[Importacao] Arquivo copiado: " << destinationPath << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[Erro] Falha ao copiar arquivo: " << e.what() << std::endl;
        }
    }
#else
    std::cout << "[Web] O Drag & Drop nativo de arquivos do SO não é suportado no navegador." << std::endl;
#endif
}

#ifdef __EMSCRIPTEN__
// Função chamada automaticamente pelo navegador ao redimensionar a tela
EM_BOOL window_size_changed(int eventType, const EmscriptenUiEvent *uiEvent, void *userData) {
    GLFWwindow* window = (GLFWwindow*)userData;
    double width, height;
    
    // Pergunta ao HTML qual é o tamanho real do canvas na tela agora
    emscripten_get_element_css_size("#canvas", &width, &height);

    double pixel_ratio = emscripten_get_device_pixel_ratio();
    
    // Atualiza a janela "Mãe" (que segura o ImGui)
    glfwSetWindowSize(window, (int)(width * pixel_ratio), (int)(height * pixel_ratio));
    
    return EM_TRUE;
}
#endif

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "PolyDuck Engine", NULL, NULL);
    glfwMakeContextCurrent(window);

    #ifdef __EMSCRIPTEN__
    // 1. Pega o tamanho do navegador na primeira vez que abre (evita aquele piscar distorcido)
    double css_w, css_h;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    glfwSetWindowSize(window, (int)css_w, (int)css_h);

    // 2. Avisa o Emscripten para chamar a nossa função quando a janela mudar
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, window, false, window_size_changed);
    #endif

    #ifndef __EMSCRIPTEN__
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "Falha ao inicializar o GLAD" << std::endl;
            return -1;
        }
    #endif

    GLFWimage images[1];
    // Coloque o caminho correto para a sua arte em pixel do Pateco!
    images[0].pixels = stbi_load("assets/PolyDuck.png", &images[0].width, &images[0].height, 0, 4); 
    
    if (images[0].pixels) {
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(images[0].pixels); // Libera a memória RAM logo em seguida
    } else {
        std::cout << "Aviso: Nao foi possivel carregar o icone da janela!" << std::endl;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Constrói o nosso Shader Program lendo os arquivos
    Shader nossoShader("shaders/shader.vs", "shaders/shader.fs");
    Shader unshadedShader("shaders/unshaded.vs", "shaders/unshaded.fs");
    Shader shadowShader("shaders/shadow.vs", "shaders/shadow.fs");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");

    // --- CARREGAR O ÍCONE DA LUZ ---
    unsigned int lightIconTexture;
    glGenTextures(1, &lightIconTexture);
    glBindTexture(GL_TEXTURE_2D, lightIconTexture);
    
    // Configurações para imagens transparentes (Clamp to Edge evita bordas estranhas)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int w, h, nrC;
    // Opcional: Se a sua imagem estiver de cabeça para baixo, mantenha o stbi_set_flip_vertically_on_load(true);
    unsigned char *iconData = stbi_load("assets/light-icon.png", &w, &h, &nrC, 0);
    
    if (iconData) {
        GLenum format = (nrC == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, iconData);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        // O C++ AGORA VAI DEDURAR O MOTIVO REAL!
        std::cout << "Aviso: Falha ao carregar light-icon.png!" << std::endl;
        std::cout << "Motivo do erro: " << stbi_failure_reason() << std::endl;
    }
    stbi_image_free(iconData);

    // Instanciação das Primitivas (Agora elas ficam na memória aguardando serem usadas)
    Plane groundPlane(10.0f, 10.0f, 20, 20); 

    // Criando a Cena e Populando o Grafo!
    Scene minhaCena;

    Sphere originDot(0.02f, 10, 5);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Vértices de um cubo simples de tamanho 1x1x1 (Apenas posições X, Y, Z)
    float skyboxVertices[] = {
        // posições          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // 1. Carrega uma textura 2D Normal (mas gigante e panorâmica!)
    unsigned int panoramaTexture;
    glGenTextures(1, &panoramaTexture);
    glBindTexture(GL_TEXTURE_2D, panoramaTexture);

    int widthP, heightP, nrChannelsP;
    // Opcional, mas panoramas costumam vir invertidos no eixo Y. Se o chão do céu 
    // ficar no topo, mude para 'true'.
    stbi_set_flip_vertically_on_load(true); 
    
    // Coloque um panorama .jpg na sua pasta
    unsigned char *dataP = stbi_load("./assets/skyboxes/sky_05_2k.png", &widthP, &heightP, &nrChannelsP, 0);
    
    if (dataP) {
        GLenum format = (nrChannelsP == 4) ? GL_RGBA : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0, format, widthP, heightP, 0, format, GL_UNSIGNED_BYTE, dataP);
        
        // Wrap S e T como Repetição evita aquela linha preta vertical na emenda das bordas
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cout << "Falha ao carregar o Panorama!" << std::endl;
    }
    stbi_image_free(dataP);

    // 2. Avisa os dois Shaders qual é a porta correta!
    skyboxShader.use();
    skyboxShader.setInt("skybox", 15); // Usa o novo nome da variável

    nossoShader.use();
    nossoShader.setInt("skybox", 15);  // Usa o novo nome da variável

    // =======================================================
    // --- CRIAÇÃO DO FRAMEBUFFER (O VIEWPORT DA ENGINE) ---
    // =======================================================
    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // 1. Cria a Textura onde a cena colorida será "pintada"
    unsigned int textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    // Por enquanto, criamos do tamanho da janela (1920x1080)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1920, 1080, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    // 2. Cria o Z-Buffer (Renderbuffer) para o cálculo de profundidade funcionar dentro da textura
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1920, 1080);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERRO::FRAMEBUFFER:: Nao esta completo!" << std::endl;
    
    // Desliga o FBO para não bagunçar nada antes do loop
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    // =======================================================

    // =======================================================
    // --- SHADOW MAPPING: O FRAMEBUFFER DO SOL ---
    // =======================================================
    // 1. Resolução do Mapa de Sombras (2048x2048 é um ótimo padrão de qualidade)
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048; 
    
    // 2. Cria o Framebuffer da Sombra
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    
    // 3. Cria a Textura que vai guardar as distâncias (Profundidade/Depth)
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    
    #ifdef __EMSCRIPTEN__
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    #else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    #endif
    
    // 4. Configuração dos filtros de textura
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // 5. CLAMP_TO_BORDER: Isso é crucial! Garante que as coisas que estão fora da 
    // "visão do Sol" não projetem uma sombra infinita esticada pelo cenário.
    #ifdef __EMSCRIPTEN__
    // WebGL não suporta borda customizada, usamos as bordas da própria imagem
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    #else
        // Desktop continua usando a borda perfeita
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    #endif
    
    // 6. Conecta a textura ao Framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    
    // 7. Avisa explicitamente o OpenGL: "Não tente desenhar cores aqui!"
    #ifndef __EMSCRIPTEN__
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    #endif
    
    // 8. Desliga o Framebuffer para não bagunçar o resto da inicialização
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    // =======================================================

    minhaCena.environment->environment->skyboxTexture = panoramaTexture;

    SceneState sceneState;
    glfwSetWindowUserPointer(window, &sceneState);
    UIManager uiManager(window);

    // Loop de Renderização
    std::function<void()> mainLoop = [&]() {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        uiManager.beginFrame();

        static float lastViewportWidth = 1920.0f;
        static float lastViewportHeight = 1080.0f;

        // Se o usuário esticou ou encolheu a janela do ImGui...
        if (sceneState.viewportWidth != lastViewportWidth || sceneState.viewportHeight != lastViewportHeight) {
            
            // 1. Redimensiona a Textura de Cor do FBO
            glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sceneState.viewportWidth, sceneState.viewportHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            
            // 2. Redimensiona o Z-Buffer (Profundidade)
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, sceneState.viewportWidth, sceneState.viewportHeight);
            
            // Atualiza a memória
            lastViewportWidth = sceneState.viewportWidth;
            lastViewportHeight = sceneState.viewportHeight;
        }

        // Usa o tamanho EXATO da janela do ImGui para calcular a Lente da Câmera
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), sceneState.viewportWidth / sceneState.viewportHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // =====================================================================
        // +++ NOVA FASE 1: O PONTO DE VISTA DO SOL (SHADOW MAP) +++
        // =====================================================================
        
        // 1. Matrizes da "Câmera do Sol"
        glm::vec3 sunDir = glm::normalize(minhaCena.environment->environment->sunDirection);
        glm::vec3 lightPos = -sunDir * 20.0f; // Afasta a câmera do sol para ela "ver" o chão
        glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 1.0f, 50.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // 2. Prepara o OpenGL para desenhar a Sombra
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO); // Liga a "Câmera do Sol"
        glClear(GL_DEPTH_BUFFER_BIT); // Limpa só o Z-Buffer

        // 3. Desenha os modelos (sem cor, só profundidade)
        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        minhaCena.draw(shadowShader, lightView); 

        // ---------------------------------------------------------------------

        // =======================================================
        // 2. LIGA O FRAMEBUFFER DO JOGADOR: Tudo a partir daqui vai para a Textura do ImGui!
        // =======================================================
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        
        // Limpa o fundo do Universo 3D
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Volta a resolução para o tamanho exato da aba do ImGui!
        glViewport(0, 0, sceneState.viewportWidth, sceneState.viewportHeight);

        std::vector<SceneNode*> listaDeLuzes;
        for (auto node : minhaCena.root->children) {
            if (node->type == NodeType::LIGHT) {
                listaDeLuzes.push_back(node);
            }
        }

        // ---------------------------------------------------------------------
        // DESENHAR O EDITOR (Ground Grid e Ponto Central)
        // ---------------------------------------------------------------------
        unshadedShader.use();
        unshadedShader.setMat4("projection", projection);
        unshadedShader.setMat4("view", view);

        // B. Desenhar o Ponto Central (Origin Dot)
        glm::mat4 dotModel = glm::mat4(1.0f);
        unshadedShader.setMat4("model", dotModel);
        unshadedShader.setVec3("uColor", glm::vec3(1.0f, 0.0f, 0.0f)); 
        originDot.draw();

        // ---------------------------------------------------------------------
        // FASE 2 (ATUALIZADA): DESENHAR A CENA 3D COLORIDA COM A SOMBRA
        // ---------------------------------------------------------------------
        if (!sceneState.wireframeMode) {
            nossoShader.use();
            
            nossoShader.setMat4("projection", projection);
            nossoShader.setMat4("view", view);
            nossoShader.setVec3("viewPos", camera.Position);
            
            // --- AMBIENT COLOR E REFLEXOS DO ENVIRONMENT ---
            nossoShader.setVec3("uAmbientColor", minhaCena.environment->environment->ambientColor);
            nossoShader.setVec3("uSunDirection", minhaCena.environment->environment->sunDirection);
            nossoShader.setVec3("uSunColor", minhaCena.environment->environment->sunColor);
            nossoShader.setFloat("uSunIntensity", minhaCena.environment->environment->sunIntensity);

            // Passamos a matriz da foto do sol para o shader poder comparar as distâncias
            nossoShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

            // Conecta a Textura do Skybox (Porta 15)
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, minhaCena.environment->environment->skyboxTexture);
            
            // Conecta a Textura do Shadow Map (Porta 16)
            glActiveTexture(GL_TEXTURE16);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            nossoShader.setInt("shadowMap", 16);
            
            glActiveTexture(GL_TEXTURE0); // Reseta para a porta 0
            // -----------------------------------------------

            int countLuzes = std::min((int)listaDeLuzes.size(), 10);
            nossoShader.setInt("numLights", countLuzes);

            for (int i = 0; i < countLuzes; i++) {
                std::string posName = "lightPos[" + std::to_string(i) + "]";
                std::string colName = "lightColor[" + std::to_string(i) + "]";
                
                nossoShader.setVec3(posName, listaDeLuzes[i]->position);
                nossoShader.setVec3(colName, listaDeLuzes[i]->lightColor * listaDeLuzes[i]->lightIntensity);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        } else {
            // MODO WIREFRAME
            unshadedShader.use();
            unshadedShader.setMat4("projection", projection);
            unshadedShader.setMat4("view", view);
            unshadedShader.setVec3("uColor", glm::vec3(1.0f, 0.5f, 0.0f)); 
            
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        // Desenha todos os objetos da árvore de uma vez (Agora aplicando o shader com sombra)
        minhaCena.draw(sceneState.wireframeMode ? unshadedShader : nossoShader, view);

        // --- RENDERIZAÇÃO DO SKYBOX ---
        // 1. Muda a matemática de profundidade para aceitar pixels com Z = 1.0 (O fundo absoluto)
        if (!sceneState.wireframeMode && sceneState.showSkybox) {
            glDepthFunc(GL_LEQUAL);  
            skyboxShader.use();
        
            // Isso prende o cubo gigante exatamente na cabeça da câmera. Você pode girar para olhar, 
            // mas se andar para frente, o céu "anda" junto com você, parecendo infinito.
            glm::mat4 viewSkybox = glm::mat4(glm::mat3(view)); 
            
            skyboxShader.setMat4("view", viewSkybox); // Passa a matriz sem posição!
            skyboxShader.setMat4("projection", projection);

            skyboxShader.setVec3("uSunDirection", minhaCena.environment->environment->sunDirection);
            skyboxShader.setVec3("uSunColor", minhaCena.environment->environment->sunColor);

            // 3. Desenha o Cubo
            glBindVertexArray(skyboxVAO);
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, minhaCena.environment->environment->skyboxTexture);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);

            // 4. Devolve o OpenGL para o comportamento de profundidade normal
            glDepthFunc(GL_LESS);
        }

        // ---------------------------------------------------------------------
        // FASE 3: DESENHAR OS ÍCONES DE LÂMPADA (Billboards Transparentes)
        // ---------------------------------------------------------------------
        // Desenhamos por último para a transparência do .png não "furar" o cenário 3D!
        if (!sceneState.wireframeMode) {
            nossoShader.use();
            nossoShader.setMat4("projection", projection);
            nossoShader.setMat4("view", view);
            
            nossoShader.setBool("uAffectedByLight", false); 
            nossoShader.setBool("uHasTexture", true);
            nossoShader.setBool("uHasSpecularMap", false); // Desliga os mapas do objeto anterior
            nossoShader.setBool("uHasNormalMap", false);   // Desliga os mapas do objeto anterior
            nossoShader.setInt("diffuseMap", 0);           // Obriga a ler da Porta 0
            // -------------------------------------------------------------------------

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, lightIconTexture);

            for (auto luz : listaDeLuzes) {
                glm::mat4 iconModel = glm::mat4(1.0f);
                iconModel = glm::translate(iconModel, luz->position);
                
                // Matemática do Billboard (Encarar a câmara)
                iconModel[0][0] = view[0][0]; iconModel[0][1] = view[1][0]; iconModel[0][2] = view[2][0];
                iconModel[1][0] = view[0][1]; iconModel[1][1] = view[1][1]; iconModel[1][2] = view[2][1];
                iconModel[2][0] = view[0][2]; iconModel[2][1] = view[1][2]; iconModel[2][2] = view[2][2];
                
                iconModel = glm::rotate(iconModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                iconModel = glm::scale(iconModel, glm::vec3(0.025f)); 
                
                nossoShader.setMat4("model", iconModel);
                
                // Pinta o ícone usando a cor da luz (Alpha forçado em 1.0f para não ficar invisível)
                glUniform4f(glGetUniformLocation(nossoShader.ID, "uBaseColor"), luz->lightColor.r, luz->lightColor.g, luz->lightColor.b, 1.0f);
                
                groundPlane.draw(); 
            }
        }

        // =======================================================
        // 3. DESLIGA O FRAMEBUFFER: Voltamos a desenhar na tela real!
        // =======================================================
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Garante que o OpenGL está no modo preenchido antes de entregar pro ImGui
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); 

        // 4. Desenha a Interface (Passando a nossa textura como parâmetro!)
        uiManager.render(sceneState, minhaCena, textureColorbuffer);
        uiManager.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    };

    #ifdef __EMSCRIPTEN__
    // Na Web, entregamos a função para o Emscripten gerenciar os frames
    emscripten_set_main_loop_arg([](void* arg) {
        (*static_cast<std::function<void()>*>(arg))();
    }, &mainLoop, 0, 1);
    #else
        // No Desktop, rodamos o while infinito clássico
        while (!glfwWindowShouldClose(window)) {
            mainLoop();
        }
    #endif

    // Encerramento (só é chamado no Desktop quando o usuário fecha a janela)
    glfwTerminate();
    return 0;
}