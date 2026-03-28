#include "CommitRepository.h"

std::vector<CommitInfo> CommitRepository::LoadRecent(const std::wstring& repoPath, int limit) const {
    return GitRunner::GetRecentCommits(repoPath, limit);
}
