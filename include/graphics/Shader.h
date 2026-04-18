#ifndef SHADER_H
#define SHADER_H

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // Necessário para o glm::value_ptr
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
public:
    unsigned int ID;

    // Construtor lê e constrói o shader
    Shader(const char* vertexPath, const char* fragmentPath);
    
    // Ativa o shader
    void use();

    // ------------------------------------------------------------------------
    // Funções utilitárias para enviar variáveis (Uniforms) para o OpenGL
    // ------------------------------------------------------------------------
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
};

#endif