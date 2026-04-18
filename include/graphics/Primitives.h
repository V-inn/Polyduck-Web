#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <vector>
#include <cmath>

// --- A CLASSE MÃE ---
class Primitive {
public:
    unsigned int VAO, VBO, EBO;
    int indexCount;

    virtual ~Primitive();

    // Todas as formas herdam a capacidade de se desenhar
    void draw() const; 

protected:
    // Uma função protegida que apenas as "filhas" podem chamar para configurar a GPU
    void setupMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, int floatCount = 8);
    void addVertex(std::vector<float>& vertices, float x, float y, float z, float nx, float ny, float nz, float u, float v);
    std::vector<float> computeTangents(const std::vector<float>& vertices8, const std::vector<unsigned int>& indices);
};

class Sphere : public Primitive {
public:
    Sphere(float radius, int sectorCount, int stackCount);
};

class Cylinder : public Primitive {
public:
    Cylinder(float baseRadius, float topRadius, float height, int sectorCount);
};

class Plane : public Primitive {
public:
    Plane(float width, float depth, int resX, int resZ, float uvScale = 1.0f);
};

class Box : public Primitive {
public:
    Box(float width, float height, float length);
};

#endif