#include "Config.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace {

const wchar_t* const kDefaultGitIgnoreLines[] = {
    L"build/",
    L"**/build/",
    L"release/",
    L"*.obj",
    L"*.pdb",
    L"*.ilk",
    L"*.log",
    L"*.tlog",
    L"*.idb",
    L"*.ipch",
    L"*.sln.docstates",
    L"*.tmp",
    L"tmp_test.txt",
    L".vs/",
    L"*.apk",
    L".DS_Store",
    L"*/.DS_Store",
    L".vscode/"
};

std::wstring GetExecutableDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring fullPath = path;
    const size_t pos = fullPath.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : fullPath.substr(0, pos);
}

std::wstring Trim(const std::wstring& value) {
    const wchar_t* whitespace = L" \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::wstring ReadAllText(const std::wstring& path) {
    std::wifstream input(path.c_str());
    if (!input.is_open()) {
        return L"";
    }
    std::wstringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool WriteAllText(const std::wstring& path, const std::wstring& content) {
    std::wofstream output(path.c_str(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << content;
    return output.good();
}

const wchar_t* ToConfigSortMode(ProjectSortMode mode) {
    switch (mode) {
    case ProjectSortMode::Name:
        return L"name";
    case ProjectSortMode::AddedTime:
    default:
        return L"added";
    }
}

ProjectSortMode ParseConfigSortMode(const std::wstring& value) {
    if (value == L"name") {
        return ProjectSortMode::Name;
    }
    return ProjectSortMode::AddedTime;
}

std::wstring BuildConfigText(const AppConfig& config) {
    std::wstringstream stream;
    stream << L"[Window]\n";
    stream << L"Width=" << config.windowWidth << L"\n";
    stream << L"Height=" << config.windowHeight << L"\n\n";
    stream << L"[General]\n";
    stream << L"LastProject=" << config.lastProject << L"\n";
    stream << L"ProjectSort=" << ToConfigSortMode(config.projectSortMode) << L"\n\n";
    stream << L"[Projects]\n";
    stream << L"Count=" << config.projects.size() << L"\n";
    for (size_t i = 0; i < config.projects.size(); ++i) {
        stream << L"Item" << i << L"=" << config.projects[i] << L"\n";
    }
    stream << L"\n[GitIgnoreTemplate]\n";
    stream << L"Count=" << config.gitIgnoreTemplateLines.size() << L"\n";
    for (size_t i = 0; i < config.gitIgnoreTemplateLines.size(); ++i) {
        stream << L"Line" << i << L"=" << config.gitIgnoreTemplateLines[i] << L"\n";
    }
    return stream.str();
}

void ApplyDefaultGitIgnoreTemplate(AppConfig& config) {
    if (!config.gitIgnoreTemplateLines.empty()) {
        return;
    }

    config.gitIgnoreTemplateLines.assign(
        std::begin(kDefaultGitIgnoreLines),
        std::end(kDefaultGitIgnoreLines));
}

}  // namespace

std::wstring Config::GetConfigPath() {
    return GetExecutableDirectory() + L"\\GitVisualTool.ini";
}

AppConfig Config::Load() {
    AppConfig config;
    const std::wstring content = ReadAllText(GetConfigPath());
    if (content.empty()) {
        return config;
    }

    std::wstringstream stream(content);
    std::wstring line;
    std::wstring section;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        line = Trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#') {
            continue;
        }
        if (line.front() == L'[' && line.back() == L']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        const size_t equalPos = line.find(L'=');
        if (equalPos == std::wstring::npos) {
            continue;
        }
        const std::wstring key = Trim(line.substr(0, equalPos));
        const std::wstring value = Trim(line.substr(equalPos + 1));

        if (section == L"Window") {
            if (key == L"Width") {
                config.windowWidth = _wtoi(value.c_str());
            } else if (key == L"Height") {
                config.windowHeight = _wtoi(value.c_str());
            }
        } else if (section == L"General") {
            if (key == L"LastProject") {
                config.lastProject = value;
            } else if (key == L"ProjectSort") {
                config.projectSortMode = ParseConfigSortMode(value);
            }
        } else if (section == L"Projects") {
            if (key.rfind(L"Item", 0) == 0 && !value.empty()) {
                config.projects.push_back(value);
            }
        } else if (section == L"GitIgnoreTemplate") {
            if (key.rfind(L"Line", 0) == 0) {
                config.gitIgnoreTemplateLines.push_back(value);
            }
        }
    }

    if (config.windowWidth < 960) {
        config.windowWidth = 960;
    }
    if (config.windowHeight < 640) {
        config.windowHeight = 640;
    }
    ApplyDefaultGitIgnoreTemplate(config);
    return config;
}

bool Config::SaveIfChanged(const AppConfig& config) {
    const std::wstring path = GetConfigPath();
    AppConfig normalized = config;
    ApplyDefaultGitIgnoreTemplate(normalized);
    const std::wstring newContent = BuildConfigText(normalized);
    const std::wstring existingContent = ReadAllText(path);
    if (newContent == existingContent) {
        return true;
    }
    return WriteAllText(path, newContent);
}
