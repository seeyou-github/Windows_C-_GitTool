#pragma once

#include <string>
#include <vector>

struct GitCommandResult {
    int exitCode = -1;
    bool success = false;
    std::wstring commandLine;
    std::wstring output;
};

struct CommitInfo {
    std::wstring hash;
    std::wstring message;
    std::wstring author;
    std::wstring date;
};

class GitRunner {
public:
    static bool IsGitRepository(const std::wstring& repoPath);

    static GitCommandResult RunGitCommand(
        const std::wstring& repoPath,
        const std::vector<std::wstring>& args);

    static std::vector<CommitInfo> GetRecentCommits(
        const std::wstring& repoPath,
        int limit);

    static std::vector<std::wstring> GetLocalBranches(const std::wstring& repoPath);
    static std::wstring GetCurrentBranch(const std::wstring& repoPath);
};
