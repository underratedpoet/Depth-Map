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
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

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

// Функция для создания шейдерной программы
GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
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
GLuint loadDepthMap(const DepthMap& depthMap) {
    std::vector<float> vertices;
    std::cout << vertex_count << std::endl;
    for (size_t i = 0; i < depthMap.data.size(); ++i) {

        float x = (i % static_cast<int>(depthMap.width)) / depthMap.width;
        float y = (i / static_cast<int>(depthMap.width)) / depthMap.height;
        float z = depthMap.data[i];
        if (z != 0) {

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z/1000);
            vertex_count++;

            
         }


    }
    std::cout << vertex_count << std::endl;
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
            //if (z != 0) {
                vertex_line.push_back(ply_count);
                ply_count++;
            //}
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
            //if (z != 0) {
                file << x << " " << y << " " << z << " " << 255 << " " << 200 << " " << 100 << "\n";
            //}
        }
    }

    for (const auto& face : faces) {
        file << "3 " << face[0] << " " << face[1] << " " << face[2] << "\n";
    }
}

std::ostream& operator<<(std::ostream& os, const DepthMap& p) {
    return os << p.width << " " << p.height << std::endl;
}

int main() {
    try {
        std::cout << "INIT SUCCESSFUL" << std::endl;
        DepthMap depthMap = readDepthMap("C:\\Users\\danie\\source\\repos\\Depth-Map\\Depth Map\\DepthMap_13.dat");
        std::cout << "READ SUCCESSFUL" << std::endl;
        std::cout << depthMap;
        std::cout << "VISUALIZE" << std::endl;


        GLFWwindow* window;
        initOpenGL(window);

        GLuint VAO = loadDepthMap(depthMap);
        GLuint shaderProgram = createShaderProgram();
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(240.0f, 320.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        while (!glfwWindowShouldClose(window)) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(shaderProgram);

            // Настройка матриц модели, вида и проекции
            glm::mat4 model = glm::mat4(1.0f);
            view = glm::lookAt(glm::vec3(0.0f, 0.0f ,1.0f), glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            //view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

            GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
            GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
            GLuint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
            

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));



            glBindVertexArray(VAO);
            glDrawArrays(GL_POINTS, 0, depthMap.data.size());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glfwTerminate();
        exportToPly(depthMap, "output.ply");
        std::cout << "EXPORT SUCCESSFUL" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}