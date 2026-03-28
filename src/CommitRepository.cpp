#include "CommitRepository.h"

std::vector<CommitInfo> CommitRepository::GetCachedRecent(const std::wstring& repoPath, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = cache_.find(repoPath);
    if (it == cache_.end()) {
        return {};
    }

    if (static_cast<int>(it->second.size()) <= limit) {
        return it->second;
    }
    return std::vector<CommitInfo>(it->second.begin(), it->second.begin() + limit);
}

void CommitRepository::UpdateCache(const std::wstring& repoPath, const std::vector<CommitInfo>& commits) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[repoPath] = commits;
}

std::vector<CommitInfo> CommitRepository::LoadRecent(const std::wstring& repoPath, int limit) const {
    const std::vector<CommitInfo> commits = GitRunner::GetRecentCommits(repoPath, limit);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[repoPath] = commits;
    }
    return commits;
}
