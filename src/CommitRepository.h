#pragma once

#include "GitRunner.h"

#include <string>
#include <vector>

class CommitRepository {
public:
    std::vector<CommitInfo> LoadRecent(const std::wstring& repoPath, int limit) const;
};
