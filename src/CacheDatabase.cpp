#include "CacheDatabase.h"

#include <fstream>
#include <sstream>

namespace {

constexpr unsigned int kMagic = 0x31424347;  // GCB1
constexpr unsigned int kVersion = 1;

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

void WriteUInt32(std::ofstream& output, unsigned int value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool ReadUInt32(std::ifstream& input, unsigned int* value) {
    return static_cast<bool>(input.read(reinterpret_cast<char*>(value), sizeof(*value)));
}

void WriteWideString(std::ofstream& output, const std::wstring& value) {
    const std::string utf8 = WideToUtf8(value);
    WriteUInt32(output, static_cast<unsigned int>(utf8.size()));
    if (!utf8.empty()) {
        output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }
}

bool ReadWideString(std::ifstream& input, std::wstring* value) {
    unsigned int size = 0;
    if (!ReadUInt32(input, &size)) {
        return false;
    }
    std::string buffer(static_cast<size_t>(size), '\0');
    if (size > 0 && !input.read(buffer.data(), static_cast<std::streamsize>(size))) {
        return false;
    }
    *value = Utf8ToWide(buffer);
    return true;
}

void WriteCommitInfo(std::ofstream& output, const CommitInfo& info) {
    WriteWideString(output, info.hash);
    WriteWideString(output, info.message);
    WriteWideString(output, info.author);
    WriteWideString(output, info.date);
}

bool ReadCommitInfo(std::ifstream& input, CommitInfo* info) {
    return ReadWideString(input, &info->hash) &&
           ReadWideString(input, &info->message) &&
           ReadWideString(input, &info->author) &&
           ReadWideString(input, &info->date);
}

void WriteCommitDiff(std::ofstream& output, const CommitFileDiff& diff) {
    WriteUInt32(output, static_cast<unsigned int>(diff.status));
    WriteWideString(output, diff.path);
    WriteWideString(output, diff.oldPath);
    WriteWideString(output, diff.newPath);
    WriteWideString(output, diff.patchPath);
    WriteWideString(output, diff.patch);
}

bool ReadCommitDiff(std::ifstream& input, CommitFileDiff* diff) {
    unsigned int status = 0;
    if (!ReadUInt32(input, &status)) {
        return false;
    }
    diff->status = static_cast<wchar_t>(status);
    return ReadWideString(input, &diff->path) &&
           ReadWideString(input, &diff->oldPath) &&
           ReadWideString(input, &diff->newPath) &&
           ReadWideString(input, &diff->patchPath) &&
           ReadWideString(input, &diff->patch);
}

}  // namespace

void CacheDatabase::SetPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
}

bool CacheDatabase::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    commitLists_.clear();
    commitDetails_.clear();
    diffContents_.clear();

    if (path_.empty()) {
        return false;
    }

    std::ifstream input(path_.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    unsigned int magic = 0;
    if (!ReadUInt32(input, &magic) || magic != kMagic) {
        return false;
    }

    unsigned int version = 0;
    if (!ReadUInt32(input, &version) || version != kVersion) {
        commitLists_.clear();
        commitDetails_.clear();
        diffContents_.clear();
        return false;
    }

    unsigned int commitListCount = 0;
    if (!ReadUInt32(input, &commitListCount)) {
        return false;
    }
    for (unsigned int i = 0; i < commitListCount; ++i) {
        std::wstring repoPath;
        unsigned int itemCount = 0;
        if (!ReadWideString(input, &repoPath) || !ReadUInt32(input, &itemCount)) {
            commitLists_.clear();
            commitDetails_.clear();
            diffContents_.clear();
            return false;
        }
        auto& commits = commitLists_[repoPath];
        commits.resize(itemCount);
        for (unsigned int item = 0; item < itemCount; ++item) {
            if (!ReadCommitInfo(input, &commits[item])) {
                return false;
            }
        }
    }

    unsigned int detailCount = 0;
    if (!ReadUInt32(input, &detailCount)) {
        commitLists_.clear();
        commitDetails_.clear();
        diffContents_.clear();
        return false;
    }
    for (unsigned int i = 0; i < detailCount; ++i) {
        std::wstring key;
        if (!ReadWideString(input, &key)) {
            commitLists_.clear();
            commitDetails_.clear();
            diffContents_.clear();
            return false;
        }
        auto& record = commitDetails_[key];
        unsigned int diffCount = 0;
        if (!ReadWideString(input, &record.summary) || !ReadUInt32(input, &diffCount)) {
            return false;
        }
        record.diffs.resize(diffCount);
        for (unsigned int item = 0; item < diffCount; ++item) {
            if (!ReadCommitDiff(input, &record.diffs[item])) {
                commitLists_.clear();
                commitDetails_.clear();
                diffContents_.clear();
                return false;
            }
        }
    }

    unsigned int diffContentCount = 0;
    if (!ReadUInt32(input, &diffContentCount)) {
        commitLists_.clear();
        commitDetails_.clear();
        diffContents_.clear();
        return false;
    }
    for (unsigned int i = 0; i < diffContentCount; ++i) {
        std::wstring key;
        if (!ReadWideString(input, &key)) {
            commitLists_.clear();
            commitDetails_.clear();
            diffContents_.clear();
            return false;
        }
        auto& record = diffContents_[key];
        if (!ReadWideString(input, &record.beforeContent) ||
            !ReadWideString(input, &record.afterContent)) {
            commitLists_.clear();
            commitDetails_.clear();
            diffContents_.clear();
            return false;
        }
    }

    return true;
}

bool CacheDatabase::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty()) {
        commitLists_.clear();
        commitDetails_.clear();
        diffContents_.clear();
        return false;
    }

    std::ofstream output(path_.c_str(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
            commitLists_.clear();
            commitDetails_.clear();
            diffContents_.clear();
            return false;
    }

    WriteUInt32(output, kMagic);
    WriteUInt32(output, kVersion);

    WriteUInt32(output, static_cast<unsigned int>(commitLists_.size()));
    for (const auto& entry : commitLists_) {
        WriteWideString(output, entry.first);
        WriteUInt32(output, static_cast<unsigned int>(entry.second.size()));
        for (const auto& commit : entry.second) {
            WriteCommitInfo(output, commit);
        }
    }

    WriteUInt32(output, static_cast<unsigned int>(commitDetails_.size()));
    for (const auto& entry : commitDetails_) {
        WriteWideString(output, entry.first);
        WriteWideString(output, entry.second.summary);
        WriteUInt32(output, static_cast<unsigned int>(entry.second.diffs.size()));
        for (const auto& diff : entry.second.diffs) {
            WriteCommitDiff(output, diff);
        }
    }

    WriteUInt32(output, static_cast<unsigned int>(diffContents_.size()));
    for (const auto& entry : diffContents_) {
        WriteWideString(output, entry.first);
        WriteWideString(output, entry.second.beforeContent);
        WriteWideString(output, entry.second.afterContent);
    }

    return static_cast<bool>(output);
}

std::vector<CommitInfo> CacheDatabase::GetCommitList(const std::wstring& repoPath, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = commitLists_.find(repoPath);
    if (it == commitLists_.end()) {
        return {};
    }
    if (static_cast<int>(it->second.size()) <= limit) {
        return it->second;
    }
    return std::vector<CommitInfo>(it->second.begin(), it->second.begin() + limit);
}

void CacheDatabase::PutCommitList(const std::wstring& repoPath, const std::vector<CommitInfo>& commits) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commitLists_[repoPath] = commits;
    }
    Save();
}

bool CacheDatabase::GetCommitDetail(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    std::wstring* summary,
    std::vector<CommitFileDiff>* diffs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = commitDetails_.find(BuildCommitKey(repoPath, commitHash));
    if (it == commitDetails_.end()) {
        return false;
    }
    if (summary != nullptr) {
        *summary = it->second.summary;
    }
    if (diffs != nullptr) {
        *diffs = it->second.diffs;
    }
    return true;
}

void CacheDatabase::PutCommitDetail(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    const std::wstring& summary,
    const std::vector<CommitFileDiff>& diffs) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commitDetails_[BuildCommitKey(repoPath, commitHash)] = CommitDetailRecord{summary, diffs};
    }
    Save();
}

bool CacheDatabase::GetDiffContent(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    const std::wstring& diffPath,
    std::wstring* beforeContent,
    std::wstring* afterContent) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = diffContents_.find(BuildDiffKey(repoPath, commitHash, diffPath));
    if (it == diffContents_.end()) {
        return false;
    }
    if (beforeContent != nullptr) {
        *beforeContent = it->second.beforeContent;
    }
    if (afterContent != nullptr) {
        *afterContent = it->second.afterContent;
    }
    return true;
}

void CacheDatabase::PutDiffContent(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    const std::wstring& diffPath,
    const std::wstring& beforeContent,
    const std::wstring& afterContent) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diffContents_[BuildDiffKey(repoPath, commitHash, diffPath)] = DiffContentRecord{beforeContent, afterContent};
    }
    Save();
}

void CacheDatabase::RemoveRepository(const std::wstring& repoPath) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commitLists_.erase(repoPath);

        for (auto it = commitDetails_.begin(); it != commitDetails_.end();) {
            if (it->first.rfind(repoPath + L"\x1f", 0) == 0) {
                it = commitDetails_.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = diffContents_.begin(); it != diffContents_.end();) {
            if (it->first.rfind(repoPath + L"\x1f", 0) == 0) {
                it = diffContents_.erase(it);
            } else {
                ++it;
            }
        }
    }
    Save();
}

std::wstring CacheDatabase::BuildCommitKey(const std::wstring& repoPath, const std::wstring& commitHash) {
    return repoPath + L"\x1f" + commitHash;
}

std::wstring CacheDatabase::BuildDiffKey(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    const std::wstring& diffPath) {
    return repoPath + L"\x1f" + commitHash + L"\x1f" + diffPath;
}
