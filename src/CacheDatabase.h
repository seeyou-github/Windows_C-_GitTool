#pragma once

#include "GitRunner.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class CacheDatabase {
public:
    void SetPath(const std::wstring& path);
    bool Load();
    bool Save();

    std::vector<CommitInfo> GetCommitList(const std::wstring& repoPath, int limit) const;
    void PutCommitList(const std::wstring& repoPath, const std::vector<CommitInfo>& commits);

    bool GetCommitDetail(
        const std::wstring& repoPath,
        const std::wstring& commitHash,
        std::wstring* summary,
        std::vector<CommitFileDiff>* diffs) const;
    void PutCommitDetail(
        const std::wstring& repoPath,
        const std::wstring& commitHash,
        const std::wstring& summary,
        const std::vector<CommitFileDiff>& diffs);

    bool GetDiffContent(
        const std::wstring& repoPath,
        const std::wstring& commitHash,
        const std::wstring& diffPath,
        std::wstring* beforeContent,
        std::wstring* afterContent) const;
    void PutDiffContent(
        const std::wstring& repoPath,
        const std::wstring& commitHash,
        const std::wstring& diffPath,
        const std::wstring& beforeContent,
        const std::wstring& afterContent);

    void RemoveRepository(const std::wstring& repoPath);

private:
    struct CommitDetailRecord {
        std::wstring summary;
        std::vector<CommitFileDiff> diffs;
    };

    struct DiffContentRecord {
        std::wstring beforeContent;
        std::wstring afterContent;
    };

    static std::wstring BuildCommitKey(const std::wstring& repoPath, const std::wstring& commitHash);
    static std::wstring BuildDiffKey(
        const std::wstring& repoPath,
        const std::wstring& commitHash,
        const std::wstring& diffPath);

    std::wstring path_;
    mutable std::mutex mutex_;
    std::unordered_map<std::wstring, std::vector<CommitInfo>> commitLists_;
    std::unordered_map<std::wstring, CommitDetailRecord> commitDetails_;
    std::unordered_map<std::wstring, DiffContentRecord> diffContents_;
};
