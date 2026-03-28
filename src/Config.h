#pragma once

#include <string>
#include <vector>

struct AppConfig {
    int windowWidth = 1280;
    int windowHeight = 820;
    std::wstring lastProject;
    std::vector<std::wstring> projects;
    std::vector<std::wstring> gitIgnoreTemplateLines;
};

class Config {
public:
    static std::wstring GetConfigPath();
    static AppConfig Load();
    static bool SaveIfChanged(const AppConfig& config);
};
