#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
    double width;
    double height;
    file.read(reinterpret_cast<char*>(&depthMap.height), sizeof(double));
    file.read(reinterpret_cast<char*>(&depthMap.width), sizeof(double));

    //depthMap.height = (int) height;
    //depthMap.width = (int)width;

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

void initOpenGL() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Depth Map Visualization", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
}

std::string readShaderSource(const std::string& filePath) {
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint compileShader(const std::string& source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    const char* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void init() { 
    glClearColor(0.3, 0.3, 0.3, 1.0); 
    glEnable(GL_LIGHTING); 
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE); 
    glEnable(GL_NORMALIZE); 
} 
void reshape(int width, int height) { 
    glViewport(0, 0, width, height); 
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); 
    glOrtho(-1.2, 1.2, -1.2, 1.2, -1, 1); glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity(); 
} void 
init_l() { 
    float light0_diffuse[] = { 0.4, 0.7, 0.2 }; 
    float light0_direction[] = { 0.0, 0.0, 1.0, 0.0 }; 
    glEnable(GL_LIGHT0); glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse); 
    glLightfv(GL_LIGHT0, GL_POSITION, light0_direction); 
}

void visualizeDepthMap(const DepthMap& depthMap) {
    std::vector<float> vertices;
    for (int y = 0; y < depthMap.height; ++y) {
        for (int x = 0; x < depthMap.width; ++x) {
            double z = depthMap.data[y * depthMap.width + x];
            if (z != 0) {
                vertices.push_back(static_cast<float>(x));
                vertices.push_back(static_cast<float>(y));
                vertices.push_back(static_cast<float>(z));
            }
        }
    }

    if (vertices.empty()) {
        std::cerr << "No valid depth data to visualize." << std::endl;
        return;
    }
    init();
    init_l();
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



    std::string vertexShaderSource = readShaderSource("vertex_shader.glsl");
    std::string fragmentShaderSource = readShaderSource("fragment_shader.glsl");
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    while (!glfwWindowShouldClose(glfwGetCurrentContext())) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

        GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLuint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, vertices.size() / 3);
        glBindVertexArray(0);

        glfwSwapBuffers(glfwGetCurrentContext());
        glfwPollEvents();

    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}


void exportToPly(const DepthMap& depthMap, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "element vertex " << vertex_count << "\n";
    file << "property double x\n";
    file << "property double y\n";
    file << "property double z\n";
    file << "property uchar red\n";
    file << "property uchar green\n";
    file << "property uchar blue\n";


    //file << "element face " << vertex_count << "\n";
    //file << "property list uchar int vertex_index\n";
    file << "end_header\n";

    for (double y = 0; y < depthMap.height; ++y) {
        for (double x = 0; x < depthMap.width; ++x) {
            double z = depthMap.data[y * depthMap.width + x];
            if (z != 0) {
                file << x << " " << y << " " << z << " "<< 255 << " " << 200 <<" " << 100 <<"\n";
            }
        }
    }
    //for (int i = 0; i < vertex_count-2; ++i) {
    //    file << 4 << " " << i << " " << i << " " << i+1 << " " << i+2 <<"\n";
        //std::cout << i << std::endl;
    //}
}

std::ostream& operator << (std::ostream& os, const DepthMap& p) {
    return os << p.width << " " << p.height << std::endl;
}

int main() {
    try {
        initOpenGL();
        std::cout << "INIT SUCCESSFUL" << std::endl;
        DepthMap depthMap = readDepthMap("C:\\Users\\User\\Desktop\\Visual Studio Projects\\Depth Map\\Depth Map\\DepthMap_13.dat");
        std::cout << "READ SUCCESSFUL" << std::endl;
        std::cout << depthMap;
        visualizeDepthMap(depthMap);
        std::cout << "VISUALIZE" << std::endl;
        exportToPly(depthMap, "output.ply");
        std::cout << "EXPORT SUCCESSFUL" << std::endl;

        glfwTerminate();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}