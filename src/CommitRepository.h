#pragma once

#include "GitRunner.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class CommitRepository {
public:
    std::vector<CommitInfo> GetCachedRecent(const std::wstring& repoPath, int limit) const;
    void UpdateCache(const std::wstring& repoPath, const std::vector<CommitInfo>& commits);
    std::vector<CommitInfo> LoadRecent(const std::wstring& repoPath, int limit) const;
    static bool AreCommitListsEqual(
        const std::vector<CommitInfo>& left,
        const std::vector<CommitInfo>& right);

private:
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::wstring, std::vector<CommitInfo>> cache_;
};
