#include "ProjectStore.h"

#include <algorithm>

bool ProjectStore::Load() {
    config_ = Config::Load();
    return true;
}

bool ProjectStore::Save() const {
    return Config::SaveIfChanged(config_);
}

bool ProjectStore::AddProject(const std::wstring& path) {
    if (Contains(path)) {
        return false;
    }
    config_.projects.push_back(path);
    if (config_.lastProject.empty()) {
        config_.lastProject = path;
    }
    return true;
}

bool ProjectStore::RemoveProject(const std::wstring& path) {
    const size_t oldSize = config_.projects.size();
    config_.projects.erase(
        std::remove(config_.projects.begin(), config_.projects.end(), path),
        config_.projects.end());
    if (config_.lastProject == path) {
        config_.lastProject = config_.projects.empty() ? L"" : config_.projects.front();
    }
    return config_.projects.size() != oldSize;
}

bool ProjectStore::Contains(const std::wstring& path) const {
    return std::find(config_.projects.begin(), config_.projects.end(), path) != config_.projects.end();
}

void ProjectStore::SetLastProject(const std::wstring& path) {
    config_.lastProject = path;
}

const std::wstring& ProjectStore::GetLastProject() const {
    return config_.lastProject;
}

const std::vector<std::wstring>& ProjectStore::GetProjects() const {
    return config_.projects;
}

void ProjectStore::SetWindowSize(int width, int height) {
    config_.windowWidth = width;
    config_.windowHeight = height;
}

int ProjectStore::GetWindowWidth() const {
    return config_.windowWidth;
}

int ProjectStore::GetWindowHeight() const {
    return config_.windowHeight;
}

const std::vector<std::wstring>& ProjectStore::GetGitIgnoreTemplateLines() const {
    return config_.gitIgnoreTemplateLines;
}
