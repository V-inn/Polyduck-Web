cmake_minimum_required(VERSION 3.15)
# 1. Removido o "RC" (Resource Compiler do Windows não existe na Web)
project(PolyDuck LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# ---------------------------------------------------------
# BAIXAR E CONFIGURAR O GLM (MATEMÁTICA)
# O GLM funciona perfeitamente tanto no Windows quanto na Web
# ---------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

include_directories(include)

# ---------------------------------------------------------
# CRIAR O EXECUTÁVEL (SEM GLAD E SEM ÍCONE DO WINDOWS)
# ---------------------------------------------------------
add_executable(PolyDuck src/main.cpp
                src/graphics/Shader.cpp 
                src/stb_image.cpp 
                src/graphics/Primitives.cpp
                src/tiny_obj_loader.cpp
                src/graphics/Model.cpp 
                src/graphics/UiManager.cpp
                src/scene/SceneNode.cpp
                src/scene/Scene.cpp
                src/graphics/Material.cpp
                src/graphics/TextureLoader.cpp
                include/imgui/imgui.cpp 
                include/imgui/imgui_tables.cpp 
                include/imgui/imgui_widgets.cpp 
                include/imgui/imgui_draw.cpp 
                include/imgui/backends/imgui_impl_glfw.cpp 
                include/imgui/backends/imgui_impl_opengl3.cpp
                )

target_include_directories(PolyDuck PRIVATE include include/imgui)


# ---------------------------------------------------------
# CONFIGURAÇÕES ESPECÍFICAS (WEB vs DESKTOP)
# ---------------------------------------------------------
if (EMSCRIPTEN)
    # --- MODO WEB ---
    message(STATUS "Configurando build para WebAssembly (Emscripten)")

    # Flags mágicas do Emscripten:
    # 1. USE_GLFW=3: Usa o GLFW embutido do navegador
    # 2. MAX_WEBGL_VERSION=2 / MIN_WEBGL_VERSION=2: Força o uso do WebGL 2.0 (OpenGL ES 3.0)
    # 3. ALLOW_MEMORY_GROWTH=1: Permite que a RAM do navegador expanda se a cena for muito pesada
    # 4. preload-file: "Empacota" os assets e shaders para dentro do navegador (o navegador não tem Disco C:)
    set_target_properties(PolyDuck PROPERTIES 
        LINK_FLAGS "-s USE_GLFW=3 -s MAX_WEBGL_VERSION=2 -s MIN_WEBGL_VERSION=2 -s ALLOW_MEMORY_GROWTH=1 --preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/shaders@/shaders\" --preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets\" --shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/shell.html\""
        SUFFIX ".html"
    )

    # Linkamos APENAS o GLM (O OpenGL e GLFW já vêm nas flags acima)
    target_link_libraries(PolyDuck PRIVATE glm::glm)

else()
    # --- MODO DESKTOP (WINDOWS/LINUX) ---
    message(STATUS "Configurando build Nativo (Desktop)")

    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    find_package(OpenGL REQUIRED)

    # O Desktop precisa do GLAD e do Ícone! Adicionamos eles de volta aqui.
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/app_icon.rc)
    target_sources(PolyDuck PRIVATE src/glad.c ${APP_ICON})

    target_link_libraries(PolyDuck PRIVATE glfw OpenGL::GL glm::glm)

    # Copia os arquivos soltos para a pasta de build do Windows
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/user_assets")
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/user_assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    else()
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_assets)
    endif()
endif()