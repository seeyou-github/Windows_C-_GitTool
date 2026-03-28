#pragma once

#include <windows.h>

#include <string>
#include <vector>

struct GitCommandResult {
    int exitCode = -1;
    bool success = false;
    bool cancelled = false;
    std::wstring commandLine;
    std::wstring output;
};

struct CommitInfo {
    std::wstring hash;
    std::wstring message;
    std::wstring author;
    std::wstring date;
};

struct CommitFileDiff {
    wchar_t status = L'M';
    std::wstring path;
    std::wstring oldPath;
    std::wstring newPath;
    std::wstring patchPath;
    std::wstring patch;
};

class GitRunner {
public:
    static bool IsGitRepository(const std::wstring& repoPath);

    static GitCommandResult RunGitCommand(
        const std::wstring& repoPath,
        const std::vector<std::wstring>& args,
        HANDLE cancelEvent = nullptr);

    static std::vector<CommitInfo> GetRecentCommits(
        const std::wstring& repoPath,
        int limit);

    static std::vector<std::wstring> GetLocalBranches(const std::wstring& repoPath);
    static std::wstring GetCurrentBranch(const std::wstring& repoPath);
    static std::wstring GetCommitDetails(const std::wstring& repoPath, const std::wstring& commitHash);
    static std::vector<CommitFileDiff> GetCommitFileDiffs(const std::wstring& repoPath, const std::wstring& commitHash);
    static std::wstring GetFileContentAtRevision(
        const std::wstring& repoPath,
        const std::wstring& revision,
        const std::wstring& filePath);
};
