#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <glm/glm.hpp>

struct Config {
    std::string depthMapFile;
    glm::vec3 lightPosition;
    glm::vec3 observerPosition;
    std::string reflectionModel;
    std::string outputFile;
    std::string outputFormat;
};

Config readConfig(const std::string& filename);
std::string trim(const std::string& str);

#endif // CONFIG_H