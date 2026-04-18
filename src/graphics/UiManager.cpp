#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <stb_image.h>
#include "graphics/UiManager.h"
#include "graphics/Primitives.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <filesystem>
#include "graphics/Model.h"
#include <algorithm>
#include "graphics/IconsFontAwesome7.h"
#include <fstream>
#include <iomanip>
#include <string>

#ifndef __EMSCRIPTEN__
#include <windows.h>
#include <commdlg.h>
#endif

namespace fs = std::filesystem;

char projectNameBuf[128] = "MeuNovoJogo";
char projectPathBuf[256] = "./Projetos";
std::string saveErrorMsg = "";

static SceneNode* nodeToDelete = nullptr;
static SceneNode* parentOfNodeToDelete = nullptr;
static SceneNode* nodeToDuplicate = nullptr;
static SceneNode* parentOfNodeToDuplicate = nullptr;
static SceneNode* nodeToMove = nullptr;
static SceneNode* newParentNode = nullptr;
static SceneNode* oldParentNode = nullptr;

double modulo(double x, double y) {
    return fmod(fmod(x, y) + y, y);
}

std::string OpenFileDialog(const char* filter = "Arquivos de Cena (*.json)\0*.json\0Todos os Arquivos (*.*)\0*.*\0") {
#ifndef __EMSCRIPTEN__
    // --- CÓDIGO ORIGINAL DO WINDOWS ---
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
#else
    // --- CÓDIGO DA WEB ---
    std::cout << "[Web] O pop-up nativo de arquivos nao e suportado no navegador!" << std::endl;
    return ""; 
#endif
}

static unsigned int CarregarTexturaDoArquivo(const char* path, bool isSkybox = false) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
    
    if (data) {
        GLenum format;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        if (!isSkybox) {
            // Textura Normal de Objetos (Com Mipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } else {
            // Textura de Skybox (Sem Mipmaps e com Clamp_To_Edge no eixo vertical)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Falha ao carregar textura no caminho: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }

    return textureID;
}

// Função auxiliar para carregar o projeto e evitar repetição de código
// Agora passamos o SceneState como parâmetro também!
static bool LoadProject(Scene& scene, SceneState& state) {
    std::string filePath = OpenFileDialog("Arquivos de Cena (*.json)\0*.json\0Todos os Arquivos (*.*)\0*.*\0");
    
    if (!filePath.empty()) {
        std::ifstream file(filePath);
        if (file.is_open()) {
            json j;
            file >> j;
            file.close();

            // 1. Limpa a seleção do Inspetor ANTES de deletar os objetos!
            state.selectedNode = nullptr;
            
            // 2. Limpa a cena velha
            scene.root->clearNonSystemChildren();
            fs::path p(filePath);
            
            // 3. Define o caminho do projeto globalmente
            state.currentProjectPath = p.parent_path().generic_string();
            
            // 4. Reconstrói a árvore
            scene.root->fromJson(j, state.currentProjectPath);
            
            std::cout << "Projeto carregado! Diretorio ativo: " << state.currentProjectPath << std::endl;
            return true; 
        } else {
            std::cout << "Erro ao tentar ler o arquivo JSON selecionado!" << std::endl;
        }
    }
    return false;
}

UIManager::UIManager(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // --- INICIALIZAÇÃO DE FONTES E ÍCONES ---
    io.Fonts->AddFontDefault(); // Carrega a fonte de texto padrão primeiro

    ImFontConfig config;
    config.MergeMode = true;  // Diz ao ImGui para colar a próxima fonte na anterior
    config.PixelSnapH = true; // Mantém os ícones nítidos

    // A faixa mágica da versão 7
    static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };

    // Carrega o seu arquivo .otf (Lembre-se: o caminho é a partir de onde o seu .exe roda!)
    const char* fontPath = "assets/fonts/FontAwesome7Free-Solid-900.otf"; 
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 10.0f, &config, ranges);
    
    if (font == nullptr) {
        std::cout << "AVISO: Nao foi possivel carregar a fonte de icones em: " << fontPath << "\n";
    }
    // ----------------------------------------

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    #ifdef __EMSCRIPTEN__
    ImGui_ImplOpenGL3_Init("#version 300 es");
    #else
    ImGui_ImplOpenGL3_Init("#version 330 core");
    #endif
}

UIManager::~UIManager() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(nullptr);
}

void UIManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
}

void UIManager::render(SceneState& state, Scene& scene, unsigned int sceneTexture) {
    static int currentTextureTarget = 0;
    static bool showHubModal = true; 

    // ===================================================
    // PROJECT HUB
    // ===================================================
    if (showHubModal) {
        ImGui::OpenPopup("Project Hub");
    }

    // Configuração de posição e tamanho do Hub
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_FirstUseEver);

    // Se não houver projeto, o usuário é obrigado a interagir com o Hub (p_open = NULL)
    bool* p_open = state.currentProjectPath.empty() ? NULL : &showHubModal;

    if (ImGui::BeginPopupModal("Project Hub", p_open, ImGuiWindowFlags_NoCollapse)) {
        
        if (ImGui::BeginTabBar("HubTabs")) {
            
            // --- ABA: NOVO PROJETO (Lógica unificada aqui) ---
            if (ImGui::BeginTabItem("Novo Projeto")) {
                ImGui::Spacing();
                ImGui::TextWrapped("Configure o local do seu novo projeto:");
                ImGui::Spacing();

                ImGui::InputText("Caminho Destino", projectPathBuf, 256);
                ImGui::InputText("Nome do Projeto", projectNameBuf, 128);
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Criar e Iniciar", ImVec2(150, 40))) {
                    fs::path targetDir = fs::path(projectPathBuf) / projectNameBuf;
                    
                    if (fs::exists(targetDir)) {
                        saveErrorMsg = "Erro: Esta pasta ja existe!";
                    } else {
                        saveErrorMsg = ""; 
                        fs::path assetsDir = targetDir / "user_assets";
                        
                        try {
                            // Cria a estrutura de pastas
                            fs::create_directories(assetsDir);
                            
                            // Salva a cena inicial
                            json j = scene.root->toJson();
                            fs::path jsonPath = targetDir / "cena.json";
                            std::ofstream file(jsonPath);
                            if (file.is_open()) {
                                file << std::setw(4) << j << std::endl;
                                file.close();
                                
                                // Define o diretório de trabalho da Engine
                                state.currentProjectPath = targetDir.generic_string();
                                showHubModal = false; // Fecha o Hub e libera a Engine
                                ImGui::CloseCurrentPopup();
                            }
                        } catch (const fs::filesystem_error& e) {
                            saveErrorMsg = std::string("Erro: ") + e.what();
                        }
                    }
                }

                if (!saveErrorMsg.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", saveErrorMsg.c_str());
                }
                ImGui::EndTabItem();
            }

           if (ImGui::BeginTabItem("Carregar Existente")) {
                ImGui::Spacing();
                if (ImGui::Button(ICON_FA_FOLDER_OPEN " Selecionar arquivo .json", ImVec2(250, 50))) {
                    
                    if (LoadProject(scene, state)) { 
                        showHubModal = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::EndPopup();
    }

    // ---------------------------------------------------
    // JANELA PRINCIPAL: VIEWPORT (A CENA 3D)
    // ---------------------------------------------------
    // Tira as margens de dentro da janela para a imagem colar nas bordas
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Cena 3D");

    // Descobre o tamanho disponível para a imagem
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    state.viewportWidth = viewportPanelSize.x > 0 ? viewportPanelSize.x : 1.0f;
    state.viewportHeight = viewportPanelSize.y > 0 ? viewportPanelSize.y : 1.0f;
    
    ImGui::Image((void*)(intptr_t)sceneTexture, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));

    ImGui::End();
    ImGui::PopStyleVar();

    // ---------------------------------------------------
    // 0. MAIN MENU BAR (A barra superior clássica!)
    // ---------------------------------------------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Arquivo")) {
            // --- NOVA CENA ---
            if (ImGui::MenuItem("Nova Cena", "Ctrl+N")) {
                
                state.selectedNode = nullptr; 
                
                scene.root->clearNonSystemChildren();
                state.currentProjectPath = ""; 
                std::cout << "Nova cena criada! A memoria foi limpa." << std::endl;
                
                showHubModal = true; 
            }

            // --- CARREGAR CENA ---
            if (ImGui::MenuItem("Carregar Cena", "Ctrl+O")) {
                LoadProject(scene, state);
            }

            // --- SALVAR ---
            if (ImGui::MenuItem("Salvar", "Ctrl+S")) {
                if (state.currentProjectPath.empty()) {
                    // Nenhum projeto ainda, força o usuário a escolher onde salvar (mesma janela do Hub)
                    showHubModal = true;
                } else {
                    // Já temos um projeto, salva silenciosamente (Quick Save)
                    json j = scene.root->toJson();
                    fs::path savePath = fs::path(state.currentProjectPath) / "cena.json";
                    
                    std::ofstream file(savePath);
                    if (file.is_open()) {
                        file << std::setw(4) << j << std::endl;
                        file.close();
                        // +++ Melhoria visual: Imprime o caminho com barras certas no console +++
                        std::cout << "Quick Save executado em: " << savePath.generic_string() << std::endl;
                    }
                }
            }
            
            ImGui::Separator();
            
            // --- SAIR ---
            if (ImGui::MenuItem("Sair", "Alt+F4")) {
                // Se a variável 'window' estiver acessível aqui:
                // glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Adicionar")) {
            if (ImGui::MenuItem("Pasta (Folder)")) { 
                scene.root->addChild(new SceneNode("Nova Pasta", NodeType::FOLDER)); 
            }
            if (ImGui::MenuItem("Luz Direcional")) { 
                scene.root->addChild(new SceneNode("Nova Luz", NodeType::LIGHT)); 
            }
            if (ImGui::MenuItem("Billboard (Sprite 2D)")) { 
                scene.root->addChild(new SceneNode("Novo Billboard", NodeType::BILLBOARD, new Plane(1.0f, 1.0f, 1, 1))); 
            }
            if (ImGui::MenuItem("Importar Modelo 3D (.obj)")) {
                std::string filePath = OpenFileDialog("Modelos 3D (*.obj)\0*.obj\0Todos os Arquivos (*.*)\0*.*\0");
                
                if (!filePath.empty()) {
                    fs::path sourcePath(filePath);
                    std::string fileName = sourcePath.filename().generic_string();
                    
                    std::string pathToLoad = filePath; 
                    std::string pathToSave = filePath; 
                    
                    // Se tivermos um projeto aberto, copiamos o arquivo para a pasta dele!
                    if (!state.currentProjectPath.empty()) {
                        fs::path destPath = fs::path(state.currentProjectPath) / "user_assets" / fileName;
                        
                        try {
                            // Copia o arquivo para o nosso projeto (substitui se já existir)
                            fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
                            
                            // +++ CORREÇÃO 2: Caminho do modelo padronizado +++
                            pathToLoad = destPath.generic_string();       
                            
                            // O caminho RELATIVO para salvar no JSON!
                            pathToSave = "user_assets/" + fileName; 
                            
                            std::cout << "[Importacao] Arquivo copiado com sucesso: " << pathToLoad << std::endl;
                        } catch (std::filesystem::filesystem_error& e) {
                            std::cout << "Erro ao copiar asset: " << e.what() << std::endl;
                        }
                    }
                    
                    // 1. Instancia o modelo lendo o arquivo físico
                    Model* novoModelo = new Model(pathToLoad);
                    
                    // 2. O SEGREDO: Sobrescrevemos o caminho que ele vai salvar no JSON
                    novoModelo->filePath = pathToSave; 
                    
                    scene.root->addChild(new SceneNode(fileName, NodeType::MESH, novoModelo));
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ---------------------------------------------------
    // JANELA 1: Configurações Globais da Cena
    // ---------------------------------------------------
    ImGui::Begin("Cena");
    ImGui::Separator();
    ImGui::Checkbox("Modo Wireframe", &state.wireframeMode);
    ImGui::Checkbox("Mostrar Skybox", &state.showSkybox);
    ImGui::End();

    // ---------------------------------------------------
    // JANELA 2: HIERARQUIA (Lista de Objetos)
    // ---------------------------------------------------
    ImGui::Begin("Hierarquia");
    ImGui::Text("Criar Primitivas:");

    // --- BOTÃO DO CUBO ---
    if (ImGui::Button("[+] Cubo")) {
        Primitive* novoCubo = new Box(1.0f, 1.0f, 1.0f);
        SceneNode* novoNode = new SceneNode("Cubo", NodeType::MESH, novoCubo);
        
        scene.root->addChild(novoNode);
        state.selectedNode = novoNode; // Já deixa selecionado no Inspetor!
    }

    ImGui::SameLine(); // Coloca o próximo botão na mesma linha

    // --- BOTÃO DA ESFERA ---
    if (ImGui::Button("[+] Esfera")) {
        Primitive* novaEsfera = new Sphere(1.0f, 36, 18);
        SceneNode* novoNode = new SceneNode("Esfera", NodeType::MESH, novaEsfera);
        
        scene.root->addChild(novoNode);
        state.selectedNode = novoNode;
    }

    ImGui::SameLine();

    if (ImGui::Button("[+] Cilindro")) {
        Primitive* novoCilindro = new Cylinder(1.0f, 1.0f, 2.0f, 36);
        SceneNode* novoNode = new SceneNode("Cilindro", NodeType::MESH, novoCilindro);
        
        scene.root->addChild(novoNode);
        state.selectedNode = novoNode;
    }

    ImGui::SameLine();

    // --- BOTÃO DO PLANO ---
    if (ImGui::Button("[+] Plano")) {
        Primitive* novoPlano = new Plane(10.0f, 10.0f, 10, 10);
        SceneNode* novoNode = new SceneNode("Plano", NodeType::MESH, novoPlano);
        
        scene.root->addChild(novoNode);
        state.selectedNode = novoNode;
    }

    ImGui::Separator();
    
    if (scene.root != nullptr) {
        for (SceneNode* child : scene.root->children) {
            // Passamos scene.root como o pai inicial
            DrawHierarchyNode(child, state, scene.root); 
        }
    }

    // --- PROCESSAMENTO DA FILA DE AÇÕES ---

    // 1. Ação de Deletar
    if (nodeToDelete != nullptr && parentOfNodeToDelete != nullptr) {
        // Se estávamos com o objeto selecionado, limpamos a seleção para o Inspetor não bugar
        if (state.selectedNode == nodeToDelete) state.selectedNode = nullptr;

        auto& lista = parentOfNodeToDelete->children;

        // Procura a posição exata do nosso nó na lista do pai
        auto it = std::find(lista.begin(), lista.end(), nodeToDelete);
        
        // Se encontrou (o iterador não chegou no final da lista), apaga!
        if (it != lista.end()) {
            lista.erase(it);
        }

        // Libera a memória RAM e deleta os filhos que estavam dentro dele
        delete nodeToDelete; 
        
        // Reseta o pedido
        nodeToDelete = nullptr;
        parentOfNodeToDelete = nullptr;
    }

    // 2. Ação de Duplicar
    if (nodeToDuplicate != nullptr && parentOfNodeToDuplicate != nullptr) {
        SceneNode* copia = new SceneNode(nodeToDuplicate->name + " (Copia)", nodeToDuplicate->type, nodeToDuplicate->mesh);
        
        // Copia as propriedades visuais
        copia->material = nodeToDuplicate->material;
        copia->affectedByLight = nodeToDuplicate->affectedByLight;
        
        // --- COPIA AS TRANSFORMAÇÕES (O que faltava!) ---
        copia->position = nodeToDuplicate->position;
        copia->rotation = nodeToDuplicate->rotation;
        copia->scale = nodeToDuplicate->scale;

        // Adiciona à lista do mesmo pai e já deixa selecionado
        parentOfNodeToDuplicate->addChild(copia);
        state.selectedNode = copia;

        // Reseta o pedido
        nodeToDuplicate = nullptr;
        parentOfNodeToDuplicate = nullptr;
    }

    if (nodeToMove != nullptr && newParentNode != nullptr && oldParentNode != nullptr) {
        
        // Passo A: Remove o nó da lista de filhos do pai antigo
        auto& lista = oldParentNode->children;
        auto it = std::find(lista.begin(), lista.end(), nodeToMove);
        if (it != lista.end()) {
            lista.erase(it);
        }

        // Passo B: Adiciona o nó na lista de filhos do novo pai
        newParentNode->addChild(nodeToMove);

        // Reseta os pedidos
        nodeToMove = nullptr;
        newParentNode = nullptr;
        oldParentNode = nullptr;
    }

    ImGui::End();

    // ---------------------------------------------------
    // JANELA 3: INSPETOR DINÂMICO
    // ---------------------------------------------------
    ImGui::Begin("Inspetor");
    if (state.selectedNode != nullptr) {
        char nameBuf[256];
        strcpy(nameBuf, state.selectedNode->name.c_str());
        if (ImGui::InputText("Nome", nameBuf, sizeof(nameBuf))) {
            state.selectedNode->name = std::string(nameBuf); // Atualiza em tempo real!
        }
        
        
        // 2. SLIDER DE SENSIBILIDADE (STEP)
        static float moveStep = 0.05f;
        static float rotateStep = 0.05f;
        
        
        if (state.selectedNode->type == NodeType::MESH 
        || state.selectedNode->type == NodeType::BILLBOARD
        || state.selectedNode->type == NodeType::LIGHT) {
            
            ImGui::Separator();

            ImGui::DragFloat("Move step", &moveStep, 0.01f, 0.01f, 2.0f);
            ImGui::DragFloat("Rotation step", &rotateStep, 1.0f, 0.5f, 2.0f);

            ImGui::Separator();
            // 3. TRANSFORMAÇÕES COM MÓDULO
            ImGui::Text("Transformacoes");
            ImGui::DragFloat3("Posicao", glm::value_ptr(state.selectedNode->position), moveStep);
            // Usamos DragFloat3 normal, mas aplicamos a sua função 'modulo' logo em seguida!
            if (ImGui::DragFloat3("Rotacao", glm::value_ptr(state.selectedNode->rotation), rotateStep)) {
                state.selectedNode->rotation.x = (float)modulo(state.selectedNode->rotation.x, 360.0);
                state.selectedNode->rotation.y = (float)modulo(state.selectedNode->rotation.y, 360.0);
                state.selectedNode->rotation.z = (float)modulo(state.selectedNode->rotation.z, 360.0);
            }
            ImGui::DragFloat3("Escala", glm::value_ptr(state.selectedNode->scale), moveStep);
        }
        
        ImGui::Separator();
        
        // 4. PAINEL DINÂMICO
        if (state.selectedNode->type == NodeType::MESH || state.selectedNode->type == NodeType::BILLBOARD) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[Material]");
            ImGui::ColorEdit4("Cor Base", glm::value_ptr(state.selectedNode->material.baseColor));
            
            if (state.selectedNode->affectedByLight) {
                ImGui::SliderFloat("Forca do Reflexo", &state.selectedNode->material.specularStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("Polimento (Shininess)", &state.selectedNode->material.shininess, 2.0f, 256.0f);
                ImGui::SliderFloat("Refletividade", &state.selectedNode->material.reflectivity, 0.0f, 1.0f);
            }
            
            // --- NOVO: SELETOR DE ALVO DE TEXTURA ---
            ImGui::Separator();
            ImGui::Text("Aplicar textura clicada como:");
            ImGui::RadioButton("Diffuse Map (Cor)", &currentTextureTarget, 0); ImGui::SameLine();
            ImGui::RadioButton("Specular Map (Brilho)", &currentTextureTarget, 1);
            ImGui::RadioButton("Normal Map (Relevo)", &currentTextureTarget, 2);

            ImGui::Checkbox("Afetado pela Iluminacao", &state.selectedNode->affectedByLight);
        }
        else if (state.selectedNode->type == NodeType::LIGHT) {
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[Luz]");
            ImGui::ColorEdit3("Cor da Luz", glm::value_ptr(state.selectedNode->lightColor));
            ImGui::DragFloat("Intensidade", &state.selectedNode->lightIntensity, 0.05f, 0.0f, 10.0f);
        }
        else if (state.selectedNode->type == NodeType::FOLDER) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "[Pasta / Grupo]");
            ImGui::TextDisabled("Use para agrupar outros objetos.");
        }
        else if (state.selectedNode->type == NodeType::BILLBOARD) {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[Billboard 2D]");
            ImGui::Text("Textura: %s", (state.selectedNode->material.diffuseMap == 0) ? "Nenhuma (Clique em um .png abaixo)" : "Carregada!");
            ImGui::ColorEdit4("Cor/Filtro", glm::value_ptr(state.selectedNode->material.baseColor));
            ImGui::Checkbox("Afetado pela Iluminacao", &state.selectedNode->affectedByLight);
        }
        else if (state.selectedNode->type == NodeType::ENVIRONMENT) {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[Configuracoes Globais]");
            ImGui::Separator();
            
            // Adicione ->environment-> antes das variáveis!
            ImGui::ColorEdit3("Cor Ambiente", glm::value_ptr(state.selectedNode->environment->ambientColor));

            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[Luz Direcional (Sol)]");
            ImGui::DragFloat3("Direcao", glm::value_ptr(state.selectedNode->environment->sunDirection), 0.01f);
            ImGui::ColorEdit3("Cor do Sol", glm::value_ptr(state.selectedNode->environment->sunColor));
            ImGui::DragFloat("Intensidade do Sol", &state.selectedNode->environment->sunIntensity, 0.05f, 0.0f, 10.0f);
            
            ImGui::Separator();
            ImGui::Text("Skybox Panoramico:");
            ImGui::RadioButton("Aplicar Imagem ao Skybox", &currentTextureTarget, 3);
            
            if (state.selectedNode->environment->skyboxTexture != 0) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Skybox Carregado!");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Nenhum Skybox!");
            }
        }
    } else {
        ImGui::TextDisabled("Selecione um objeto na Hierarquia.");
    }
    ImGui::End();

    // ---------------------------------------------------
    // JANELA 4: NAVEGADOR DE ARQUIVOS (Asset Browser)
    // ---------------------------------------------------
    ImGui::Begin("Navegador de Arquivos");
    
    // 1. Descobre qual pasta ler (A do projeto atual, ou uma global se for cena nova)
    std::string assetsDir = state.currentProjectPath.empty() ? "./user_assets" : (std::filesystem::path(state.currentProjectPath) / "user_assets").generic_string();

    ImGui::Text("Diretorio: %s", assetsDir.c_str());
    ImGui::Separator();

    // 2. Proteção: Só tenta varrer se a pasta realmente existir no Windows
    if (std::filesystem::exists(assetsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(assetsDir)) {
            std::string path = entry.path().generic_string();
            std::string extension = entry.path().extension().generic_string();
            std::string filename = entry.path().filename().generic_string();
            std::string nodeName = entry.path().stem().generic_string(); 

            if (extension == ".obj") {
                std::string buttonLabel = std::string(ICON_FA_CUBE) + " " + filename;
                if (ImGui::Button(buttonLabel.c_str())) {
                    
                    Model* novoModelo = new Model(path);
                    novoModelo->filePath = "user_assets/" + filename; 
                    
                    SceneNode* novoNode = new SceneNode(nodeName, NodeType::MESH, novoModelo); 
                    scene.root->addChild(novoNode);
                    state.selectedNode = novoNode; 
                }
            }
            else if (extension == ".jpg" || extension == ".png") {
                std::string buttonLabel = std::string(ICON_FA_IMAGE) + " " + filename;
                if (ImGui::Button(buttonLabel.c_str())) {
                    if (state.selectedNode != nullptr) {
                        bool isSkyboxTarget = (currentTextureTarget == 3);
                        
                        // 1. Gera o ID da textura na GPU 
                        unsigned int novaTextura = CarregarTexturaDoArquivo(path.c_str(), isSkyboxTarget);
                        
                        // 2. Cria o caminho relativo que será salvo no JSON
                        // Como os assets sempre ficam na pasta user_assets do projeto:
                        std::string relativePath = "user_assets/" + filename;
                        
                        if (state.selectedNode->type == NodeType::MESH || state.selectedNode->type == NodeType::BILLBOARD) {
                            if (currentTextureTarget == 0) {
                                state.selectedNode->material.diffuseMap = novaTextura;
                                state.selectedNode->material.diffusePath = relativePath; 
                            } else if (currentTextureTarget == 1) {
                                state.selectedNode->material.specularMap = novaTextura;
                                state.selectedNode->material.specularPath = relativePath; 
                            } else if (currentTextureTarget == 2) {
                                state.selectedNode->material.normalMap = novaTextura;
                                state.selectedNode->material.normalPath = relativePath;   
                                std::cout << "Normal Map aplicado!" << std::endl;
                            }
                        }
                        else if (state.selectedNode->type == NodeType::ENVIRONMENT) {
                            if (currentTextureTarget == 3){
                                state.selectedNode->environment->skyboxTexture = novaTextura;
                                // Nota: Para o skybox persistir, você precisaria adicionar uma string skyboxPath 
                                // na struct Environment e salvar aqui também (state.selectedNode->environment->skyboxPath = relativePath;)
                            }
                        }
                    }
                }
            }
        }
    } else {
        // Se a pasta não existir (ex: o usuário acabou de abrir a engine e não salvou o projeto ainda)
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "A pasta de assets do projeto nao foi encontrada.");
    }
    ImGui::End();
}

static const char* GetIconForNode(SceneNode* node) {
    switch (node->type) {
        case NodeType::MESH: 
            return ICON_FA_CUBE;
            
        case NodeType::LIGHT: 
            return ICON_FA_LIGHTBULB;
            
        case NodeType::FOLDER: 
            return ICON_FA_FOLDER;
        
        case NodeType::ENVIRONMENT: 
            return ICON_FA_CLOUD;
            
        default: 
            return ICON_FA_CIRCLE_QUESTION;
    }
}

void UIManager::DrawHierarchyNode(SceneNode* node, SceneState& state, SceneNode* parent) {
    if (node == nullptr) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (state.selectedNode == node) flags |= ImGuiTreeNodeFlags_Selected;
    if (node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    const char* icon = GetIconForNode(node);

    // 2. Formata a string para ter "%s %s" (Ícone e depois o Nome)
    bool nodeOpen = ImGui::TreeNodeEx((void*)node, flags, "%s %s", icon, node->name.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        state.selectedNode = node;
    }

    // --- DRAG (O objeto sendo agarrado e arrastado) ---
    if (node->isDraggable == true) { 
        if (ImGui::BeginDragDropSource()) {
            // Empacotamos o nó atual e o pai dele num array de 2 posições
            SceneNode* payloadData[2] = { node, parent };
            
            // Enviamos 16 bytes (o tamanho de dois ponteiros de 64-bits)
            ImGui::SetDragDropPayload("SCENE_NODE", payloadData, sizeof(SceneNode*) * 2);
            
            // O textinho que aparece flutuando ao lado do mouse
            ImGui::Text("Movendo %s", node->name.c_str()); 
            ImGui::EndDragDropSource();
        }
    }

    // --- DROP (O objeto que está recebendo o outro) ---
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            SceneNode** data = (SceneNode**)payload->Data;
            SceneNode* draggedNode = data[0];
            SceneNode* oldParent = data[1];
            
            // Regra de Ouro: Não pode soltar dentro de si mesmo, nem onde já está
            // E garante que o objeto que está CHEGANDO era arrastável (redundância de segurança)
            if (draggedNode != node && oldParent != node && draggedNode->isDraggable == true) {
                nodeToMove = draggedNode;
                newParentNode = node; // O nó onde soltamos o mouse vira o novo pai
                oldParentNode = oldParent;
            }
        }
        
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem()) {
        state.selectedNode = node; // Seleciona o item para facilitar a visualização

        if (node->hasRightclick == true) {
        
            if (ImGui::MenuItem("Duplicar")) {
                nodeToDuplicate = node;
                parentOfNodeToDuplicate = parent;
            }
            ImGui::Separator(); // Uma linha bonitinha para separar
            if (ImGui::MenuItem("Deletar", "Del")) {
                nodeToDelete = node;
                parentOfNodeToDelete = parent;
            }
        }
        
        ImGui::EndPopup();
    }

    // A Recursão
    if (nodeOpen) {
        for (SceneNode* child : node->children) {
            DrawHierarchyNode(child, state, node); // Passa 'node' como o novo pai!
        }
        ImGui::TreePop(); 
    }
}

void UIManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}