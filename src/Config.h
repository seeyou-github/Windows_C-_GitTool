#pragma once

#include <string>
#include <vector>

enum class ProjectSortMode {
    AddedTime = 0,
    Name = 1
};

struct AppConfig {
    int windowWidth = 1280;
    int windowHeight = 820;
    std::wstring lastProject;
    ProjectSortMode projectSortMode = ProjectSortMode::AddedTime;
    std::vector<std::wstring> projects;
    std::vector<std::wstring> gitIgnoreTemplateLines;
};

class Config {
public:
    static std::wstring GetConfigPath();
    static AppConfig Load();
    static bool SaveIfChanged(const AppConfig& config);
};
