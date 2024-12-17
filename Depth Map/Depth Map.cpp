#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <string>
#include <stdexcept>
#include "config.h"


int vertex_count = 0;
struct DepthMap {
    double width;
    double height;
    std::vector<double> data;
};

DepthMap readDepthMap(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }
    DepthMap depthMap;
    file.read(reinterpret_cast<char*>(&depthMap.height), sizeof(double));
    file.read(reinterpret_cast<char*>(&depthMap.width), sizeof(double));
    if (depthMap.width == 0 || depthMap.height == 0) {
        throw std::runtime_error("Invalid width or height in depth map file");
    }
    depthMap.data.resize(depthMap.width * depthMap.height);
    file.read(reinterpret_cast<char*>(depthMap.data.data()), depthMap.data.size() * sizeof(double));
    if (!file) {
        throw std::runtime_error("Error reading depth map data");
    }
    return depthMap;
}

// Шейдеры
//Torrens
// Вершинный шейдер
const char* torrensVertexShader = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos; // Позиция вершины
layout(location = 1) in vec3 aNormal; // Нормаль вершины

out vec3 FragPos; // Позиция фрагмента в мировых координатах
out vec3 Normal;  // Интерполированная нормаль

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal; // Трансформация нормали

    gl_Position = projection * view * vec4(FragPos, 1.0);
})glsl";

// Фрагментный шейдер
const char* torrensFragmentShader = R"glsl(
#version 330 core

in vec3 FragPos; // Позиция фрагмента в мировых координатах
in vec3 Normal;  // Интерполированная нормаль

out vec4 FragColor; // Цвет фрагмента

// Удаление экспоненциальной записи
#define PI 3.14159265359

// Параметры освещения
uniform vec3 lightPos;   // Позиция источника света
uniform vec3 lightColor; // Цвет света
uniform vec3 viewPos;    // Позиция камеры
uniform vec3 objectColor; // Цвет объекта

// Микрофасетная модель
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    // Вектор нормали, направленный от поверхности
    vec3 N = normalize(Normal);
    // Вектор направления к свету
    vec3 L = normalize(lightPos - FragPos);
    // Вектор направления к наблюдателю
    vec3 V = normalize(viewPos - FragPos);
    // Полунаклонный вектор
    vec3 H = normalize(V + L);

    // Параметры материала
    float metallic = 0.4;
    float roughness = 0.32;
    float ao = 1.0; // Ambient Occlusion

    // Базовое отражение (F0)
    vec3 F0 = vec3(0.04); // Непроводники имеют низкое отражение
    F0 = mix(F0, objectColor, metallic);

    // Вычисление отражения по Френелю
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Микрофасетное распределение
    float NDF = distributionGGX(N, H, roughness);

    // Геометрическая функция
    float G = geometrySmith(N, V, L, roughness);

    // Specular BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);

    // Диффузный компонент
    vec3 kD = vec3(1.0) - F;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    // Освещение
    vec3 Lo = (kD * objectColor / PI + specular) * lightColor * NdotL;

    // Ambient освещение
    vec3 ambient = vec3(0.03) * objectColor * ao;

    // Итоговый цвет
    vec3 color = ambient + Lo;

    // Гамма-коррекция
    color = pow(color, vec3(1.0 / 2.0));

    FragColor = vec4(color, 1.0);
})glsl";

//Phong
// Вершинный шейдер
const char* phongVertexShader = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos; // Позиция вершины
layout(location = 1) in vec3 aNormal; // Нормаль вершины

out vec3 FragPos; // Позиция фрагмента в мировом пространстве
out vec3 Normal;  // Нормаль фрагмента в мировом пространстве

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Преобразование позиции вершины в мировое пространство
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Трансформация нормалей
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    // Финальная позиция вершины в отсеченном пространстве
    gl_Position = projection * view * vec4(FragPos, 1.0);
})glsl";

// Фрагментный шейдер
const char* phongFragmentShader = R"glsl(
#version 330 core

in vec3 FragPos; // Позиция фрагмента в мировом пространстве
in vec3 Normal;  // Нормаль фрагмента в мировом пространстве

out vec4 FragColor; // Итоговый цвет фрагмента

uniform vec3 viewPos;      // Позиция камеры
uniform vec3 lightPos;     // Позиция источника света
uniform vec3 lightColor;   // Цвет света
uniform vec3 objectColor;  // Цвет объекта

void main() {
    float depthNormalized = clamp((FragPos.z - 0.5) / (0.63 - 0.5), 0.0, 1.0);

    float specularStrength = 2.1; // Интенсивность зеркального света
    float diffuseStrength = 0.45;  // Интенсивность диффузного света

    // Компонента фонового освещения

    vec3 lightDir = normalize(lightPos - FragPos);

    // Нормализуем нормаль и направление к источнику света
    vec3 norm = normalize(Normal);

    // Компонента диффузного освещения
    float diff = max(dot(norm, lightDir), 0.0);
     vec3 diffuse = diff * lightColor * diffuseStrength;
   
     
    // Компонента зеркального освещения (Phong)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
   
    float shininess =32.0; // Показатель блеска (чем выше, тем ярче и "жёстче" блики)
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

   
    vec3 specular = vec3(specularStrength) * spec * lightColor;

    // Итоговый цвет
    vec3 result = ( diffuse + specular) * objectColor  ;
    FragColor = vec4(result, 1.0);
})glsl";

//Lambert
// Вершинный шейдер
const char* lambertVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

// Фрагментный шейдер
const char* lambertFragmentShader = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main() {
    // Нормализация нормали
    vec3 norm = normalize(Normal);
    
    // Приведение значения FragPos.z к диапазону [0, 1]
    float depthNormalized = clamp((FragPos.z - 0.4) / (0.73 - 0.4), 0.0, 1.0);
    
    // Направление света (не трогаем глубину)
    vec3 lightDir = normalize(lightPos - FragPos);

    // Расчёт диффузного освещения (модель Ламберта)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Итоговый цвет с учётом нормализованной глубины
    vec3 result = diffuse * objectColor*depthNormalized;
    FragColor = vec4(result, 1.0);
})glsl";

// Функция для компиляции шейдера
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}



// Функция для инициализации OpenGL
void initOpenGL(GLFWwindow*& window) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }
    window = glfwCreateWindow(1000, 1000, "Depth Map Visualization", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        exit(EXIT_FAILURE);
    }
    glEnable(GL_DEPTH_TEST);


}

// Функция для загрузки данных карты глубины в буфер OpenGL
GLuint loadDepthMap(const DepthMap& depthMap, std::vector<unsigned int>& indices) {
    std::vector<float> vertices;

    for (size_t y = 0; y < depthMap.height; ++y) {
        for (size_t x = 0; x < depthMap.width; ++x) {
            float nx = (float)x / (depthMap.width - 1) * 2.0f - 1.0f; // Нормализация x
            float ny = (float)y / (depthMap.height - 1) * 2.0f - 1.0f; // Нормализация y
            float z = depthMap.data[y * depthMap.width + x];

            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(z);

            // Вычисление нормали
            float left = (x > 0) ? depthMap.data[y * depthMap.width + (x - 1)] : z;
            float right = (x < depthMap.width - 1) ? depthMap.data[y * depthMap.width + (x + 1)] : z;
            float top = (y > 0) ? depthMap.data[(y - 1) * depthMap.width + x] : z;
            float bottom = (y < depthMap.height - 1) ? depthMap.data[(y + 1) * depthMap.width + x] : z;

            float dzdx = right - left;
            float dzdy = bottom - top;

            glm::vec3 normal(-dzdx, -dzdy, 2.0f);
            normal = glm::normalize(normal);

            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    for (size_t y = 0; y < depthMap.height - 1; ++y) {
        for (size_t x = 0; x < depthMap.width - 1; ++x) {
            unsigned int v1 = y * depthMap.width + x;
            unsigned int v2 = (y + 1) * depthMap.width + x;
            unsigned int v3 = (y + 1) * depthMap.width + (x + 1);
            unsigned int v4 = y * depthMap.width + (x + 1);

            indices.push_back(v1);
            indices.push_back(v2);
            indices.push_back(v3);
            indices.push_back(v1);
            indices.push_back(v3);
            indices.push_back(v4);
        }
    }

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

void exportToPly(const DepthMap& depthMap, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    std::vector<std::vector<unsigned int>> vertexes;
    unsigned int ply_count = 0;
    for (double y = 0; y < depthMap.height; ++y) {
        std::vector<unsigned int> vertex_line;
        for (double x = 0; x < depthMap.width; ++x) {
            double z = depthMap.data[y * depthMap.width + x];
            vertex_line.push_back(ply_count);
            ply_count++;
        }
        if (!vertex_line.empty()) {
            vertexes.push_back(vertex_line);
        }
    }

    std::vector<std::vector<unsigned int>> faces;
    for (int y = 0; y < vertexes.size() - 1; ++y) {
        for (int x = 0; x < vertexes[y].size() - 1; ++x) {
            unsigned int v1 = vertexes[y][x];
            unsigned int v2 = (x < vertexes[y + 1].size()) ? vertexes[y + 1][x] : vertexes[y + 1].back();
            unsigned int v3 = (x + 1 < vertexes[y + 1].size()) ? vertexes[y + 1][x + 1] : vertexes[y + 1].back();
            unsigned int v4 = (x + 1 < vertexes[y].size()) ? vertexes[y][x + 1] : vertexes[y].back();

            std::vector<unsigned int> face1 = { v1, v2, v3 };
            std::vector<unsigned int> face2 = { v1, v4, v3 };

            faces.push_back(face1);
            faces.push_back(face2);
        }
    }

    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "element vertex " << ply_count << "\n";
    file << "property double x\n";
    file << "property double y\n";
    file << "property double z\n";
    file << "property uchar red\n";
    file << "property uchar green\n";
    file << "property uchar blue\n";
    file << "element face " << faces.size() << "\n";
    file << "property list uchar uint vertex_indices\n";
    file << "end_header\n";

    for (double y = 0; y < depthMap.height; ++y) {
        for (double x = 0; x < depthMap.width; ++x) {
            double z = depthMap.data[y * depthMap.width + x];
            file << x << " " << y << " " << z << " " << 255 << " " << 200 << " " << 100 << "\n";
        }
    }

    for (const auto& face : faces) {
        file << "3 " << face[0] << " " << face[1] << " " << face[2] << "\n";
    }
}

void exportToStl(const DepthMap& depthMap, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    file << "solid depthmap\n";

    for (int y = 0; y < depthMap.height - 1; ++y) {
        for (int x = 0; x < depthMap.width - 1; ++x) {
            double z1 = depthMap.data[y * depthMap.width + x];
            double z2 = depthMap.data[y * depthMap.width + (x + 1)];
            double z3 = depthMap.data[(y + 1) * depthMap.width + x];
            double z4 = depthMap.data[(y + 1) * depthMap.width + (x + 1)];

            // First triangle
            file << "facet normal 0 0 0\n";
            file << "  outer loop\n";
            file << "    vertex " << x << " " << y << " " << z1 << "\n";
            file << "    vertex " << x + 1 << " " << y << " " << z2 << "\n";
            file << "    vertex " << x << " " << y + 1 << " " << z3 << "\n";
            file << "  endloop\n";
            file << "endfacet\n";

            // Second triangle
            file << "facet normal 0 0 0\n";
            file << "  outer loop\n";
            file << "    vertex " << x + 1 << " " << y << " " << z2 << "\n";
            file << "    vertex " << x + 1 << " " << y + 1 << " " << z4 << "\n";
            file << "    vertex " << x << " " << y + 1 << " " << z3 << "\n";
            file << "  endloop\n";
            file << "endfacet\n";
        }
    }

    file << "endsolid depthmap\n";
}

void exportToVrml(const DepthMap& depthMap, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    file << "#VRML V2.0 utf8\n";
    file << "Shape {\n";
    file << "  appearance Appearance {\n";
    file << "    material Material {\n";
    file << "      diffuseColor 1 0.78 0.39\n"; // Цвет вершин
    file << "    }\n";
    file << "  }\n";
    file << "  geometry IndexedFaceSet {\n";
    file << "    coord Coordinate {\n";
    file << "      point [\n";

    for (int y = 0; y < depthMap.height; ++y) {
        for (int x = 0; x < depthMap.width; ++x) {
            double z = depthMap.data[y * depthMap.width + x];
            file << "        " << x << " " << y << " " << z << ",\n";
        }
    }

    file << "      ]\n";
    file << "    }\n";
    file << "    coordIndex [\n";

    for (int y = 0; y < depthMap.height - 1; ++y) {
        for (int x = 0; x < depthMap.width - 1; ++x) {
            int v1 = y * depthMap.width + x;
            int v2 = y * depthMap.width + (x + 1);
            int v3 = (y + 1) * depthMap.width + x;
            int v4 = (y + 1) * depthMap.width + (x + 1);

            file << "      " << v1 << ", " << v2 << ", " << v3 << ", -1,\n";
            file << "      " << v2 << ", " << v4 << ", " << v3 << ", -1,\n";
        }
    }

    file << "    ]\n";
    file << "  }\n";
    file << "}\n";
}
std::ostream& operator<<(std::ostream& os, const DepthMap& p) {
    return os << p.width << " " << p.height << std::endl;
}

GLFWwindow* initializeGLFW(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "Не удалось инициализировать GLFW" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Не удалось создать окно GLFW" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Не удалось инициализировать GLAD" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    return window;
}

// Функция для выбора шейдеров
void getShaderSource(const std::string& shaderType, const char*& vertexShader, const char*& fragmentShader) {
    if (shaderType == "Torrens") {
        vertexShader = torrensVertexShader;
        fragmentShader = torrensFragmentShader;
    }
    else if (shaderType == "Phong") {
        vertexShader = phongVertexShader;
        fragmentShader = phongFragmentShader;
    }
    else if (shaderType == "Lambert") {
        vertexShader = lambertVertexShader;
        fragmentShader = lambertFragmentShader;
    }
    else {
        std::cerr << "Unknown shader type: " << shaderType << std::endl;
        vertexShader = nullptr;
        fragmentShader = nullptr;
    }
}

void generateDepthMapVertices(const DepthMap& depthMap, std::vector<float>& vertices, float scale, float maxDepth = 500.0f) {

    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest(); 
    for (int y = 0; y < depthMap.height - 1; ++y) {
        for (int x = 0; x < depthMap.width - 1; ++x) {
            float z1 = depthMap.data[y * depthMap.width + x] / maxDepth;
            float z2 = depthMap.data[y * depthMap.width + x + 1] / maxDepth;
            float z3 = depthMap.data[(y + 1) * depthMap.width + x] / maxDepth;
            float z4 = depthMap.data[(y + 1) * depthMap.width + x + 1] / maxDepth;


            // Обновление минимального и максимального значений
            if (z1 != 0) {
                minZ = std::min(minZ, z1);
                maxZ = std::max(maxZ, z1);
            }
            if (z2 != 0) {
                minZ = std::min(minZ, z2);
                maxZ = std::max(maxZ, z2);
            }
            if (z3 != 0) {
                minZ = std::min(minZ, z3);
                maxZ = std::max(maxZ, z3);
            }
            if (z4 != 0) {
                minZ = std::min(minZ, z4);
                maxZ = std::max(maxZ, z4);
            }


            if (z1 != 0 && z2 != 0 && z3 != 0) {
                vertices.push_back(x * scale); vertices.push_back(y * scale); vertices.push_back(z1);
                vertices.push_back((x + 1) * scale); vertices.push_back(y * scale); vertices.push_back(z2);
                vertices.push_back(x * scale); vertices.push_back((y + 1) * scale); vertices.push_back(z3);
            }
            if (z4 != 0 && z2 != 0 && z3 != 0) {
                vertices.push_back(x * scale); vertices.push_back((y + 1) * scale); vertices.push_back(z3);
                vertices.push_back((x + 1) * scale); vertices.push_back(y * scale); vertices.push_back(z2);
                vertices.push_back((x + 1) * scale); vertices.push_back((y + 1) * scale); vertices.push_back(z4);
            }
        }
    }
    std::cout << minZ << std::endl;//Требуется для понимания разности глубины и корекции параметров в шейдере
    std::cout << maxZ << std::endl;
}

void generateNormals(const std::vector<float>& vertices, std::vector<float>& normals) {
    for (size_t i = 0; i < vertices.size(); i += 9) {
        glm::vec3 v1(vertices[i], vertices[i + 1], vertices[i + 2]);
        glm::vec3 v2(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
        glm::vec3 v3(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

        glm::vec3 edge1 = v2 - v1;
        glm::vec3 edge2 = v3 - v1;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        for (int j = 0; j < 3; ++j) {
            normals.push_back(normal.x);
            normals.push_back(normal.y);
            normals.push_back(normal.z);
        }
    }
}

void setupBuffers(const std::vector<float>& vertices, const std::vector<float>& normals, GLuint& VAO, GLuint& VBO, GLuint& NBO) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &NBO);

    glBindVertexArray(VAO);

    // Вершины
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Нормали
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

GLuint loadShader(const char* shaderSource, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Ошибка компиляции шейдера: " << infoLog << std::endl;
    }
    return shader;
}

// Функция создания шейдеров
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = loadShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = loadShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void setTransformationMatrices(GLuint shaderProgram, const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, glm::vec3& cameraPosition, glm::vec3& lightPosition) {
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLuint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Передача позиции камеры
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPosition));


    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), lightPosition.x, lightPosition.y, lightPosition.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 0.64f, 0.57f, 0.88f);
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.88f, 0.71f, 0.53f);

}

int main() {
    try {
        Config config = readConfig("config.json");  // Чтение конфигурации из JSON файла

        DepthMap depthMap = readDepthMap(config.depthMapFile);

        glm::vec3 lightPosition = glm::vec3(200.0f, 200.0f, 200.0f);//glm::vec3(config.lightPosition.x, config.lightPosition.y, config.lightPosition.z);
        glm::vec3 cameraPosition = glm::vec3(100.0f, 100.0f, 60.0f);//glm::vec3(config.observerPosition.x, config.observerPosition.y, config.observerPosition.z);

        const char* vertexShader;
        const char* fragmentShader;
        getShaderSource(config.reflectionModel, vertexShader, fragmentShader);

        GLFWwindow* window = initializeGLFW(1200, 1200, "Depth Map Visualization");
        if (!window) return -1;

        std::vector<float> vertices;
        float scale = 0.2f;
        generateDepthMapVertices(depthMap, vertices, scale);

        std::vector<float> normals;
        generateNormals(vertices, normals);

        GLuint VAO, VBO, NBO;
        setupBuffers(vertices, normals, VAO, VBO, NBO);

        GLuint shaderProgram = createShaderProgram(vertexShader, fragmentShader);
        //glm::vec3 viewPosition = glm::vec3(depthMap.width * scale / 2.0f, depthMap.height * scale / 2.0f, 150.0f);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::lookAt(
            cameraPosition,
            glm::vec3(depthMap.width * scale / 2.0f, depthMap.height * scale / 2.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f)
        );
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)depthMap.width / (float)depthMap.height, 0.1f, 1000.0f);

        while (!glfwWindowShouldClose(window)) {
            glClearColor(0.4, 0.4f, 0.4f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(shaderProgram);
            setTransformationMatrices(shaderProgram, model, view, projection, cameraPosition, lightPosition);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &NBO);
        glDeleteProgram(shaderProgram);

        glfwTerminate();
        if (config.outputFormat == "ply") {
            exportToPly(depthMap, config.outputFile + ".ply");
        }
        else if (config.outputFormat == "stl") {
            exportToStl(depthMap, config.outputFile + ".stl");
        }
        else if (config.outputFormat == "vrml") {
            exportToVrml(depthMap, config.outputFile + ".vrml");
        }
        else {
            throw std::runtime_error("Unsupported output format: " + config.outputFormat);
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}