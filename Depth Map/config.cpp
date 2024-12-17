#include "config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

Config readConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open config file");
    }

    Config config;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '{' || line[0] == '}') {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));
        if (value.back() == ',') {
            value.pop_back();
        }
        value = trim(value);

        if (key == "\"depthMapFile\"") {
            config.depthMapFile = value.substr(1, value.size() - 2); // Remove quotes
        }
        else if (key == "\"lightPosition.x\"") {
            config.lightPosition.x = std::stof(value);
        }
        else if (key == "\"lightPosition.y\"") {
            config.lightPosition.y = std::stof(value);
        }
        else if (key == "\"lightPosition.z\"") {
            config.lightPosition.z = std::stof(value);
        }
        else if (key == "\"observerPosition.x\"") {
            config.observerPosition.x = std::stof(value);
        }
        else if (key == "\"observerPosition.y\"") {
            config.observerPosition.y = std::stof(value);
        }
        else if (key == "\"observerPosition.z\"") {
            config.observerPosition.z = std::stof(value);
        }
        else if (key == "\"reflectionModel\"") {
            config.reflectionModel = value.substr(1, value.size() - 2); // Remove quotes
        }
        else if (key == "\"outputFile\"") {
            config.outputFile = value.substr(1, value.size() - 2); // Remove quotes
        }
        else if (key == "\"outputFormat\"") {
            config.outputFormat = value.substr(1, value.size() - 2); // Remove quotes
        }
    }

    return config;
}