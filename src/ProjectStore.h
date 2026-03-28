#pragma once

#include "Config.h"

#include <string>
#include <vector>

class ProjectStore {
public:
    bool Load();
    bool Save() const;

    bool AddProject(const std::wstring& path);
    bool RemoveProject(const std::wstring& path);
    bool Contains(const std::wstring& path) const;

    void SetLastProject(const std::wstring& path);
    const std::wstring& GetLastProject() const;
    const std::vector<std::wstring>& GetProjects() const;

    void SetWindowSize(int width, int height);
    int GetWindowWidth() const;
    int GetWindowHeight() const;
    const std::vector<std::wstring>& GetGitIgnoreTemplateLines() const;

private:
    AppConfig config_;
};
