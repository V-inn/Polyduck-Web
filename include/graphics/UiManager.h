#ifndef UIMANAGER_H
#define UIMANAGER_H

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>
#include "SceneState.h"
#include "scene/Scene.h"
class UIManager {
public:
    UIManager(GLFWwindow* window); // Inicializa o ImGui
    ~UIManager();                  // Limpa a memória do ImGui no final

    void beginFrame();             // Prepara para desenhar
    void render(SceneState& state, Scene& scene, unsigned int sceneTexture);
    void endFrame();               // Envia para a placa de vídeo
    void DrawHierarchyNode(SceneNode* node, SceneState& state, SceneNode* parent);
};

#endif