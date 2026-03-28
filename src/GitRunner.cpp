#include "GitRunner.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace {

std::wstring QuoteArgument(const std::wstring& value) {
    if (value.find_first_of(L" \t\"") == std::wstring::npos) {
        return value;
    }

    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L'\\';
        }
        quoted += ch;
    }
    quoted += L"\"";
    return quoted;
}

std::wstring JoinArguments(const std::vector<std::wstring>& args) {
    std::wstring result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            result += L' ';
        }
        result += QuoteArgument(args[i]);
    }
    return result;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) {
            return L"";
        }
        std::wstring fallback(size, L'\0');
        MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), fallback.data(), size);
        return fallback;
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

bool DecodeUtf8Prefix(std::string& pendingBytes, std::wstring* decodedText) {
    if (decodedText == nullptr || pendingBytes.empty()) {
        return false;
    }

    const size_t maxTrim = std::min<size_t>(3, pendingBytes.size());
    for (size_t trim = 0; trim <= maxTrim; ++trim) {
        const int byteCount = static_cast<int>(pendingBytes.size() - trim);
        if (byteCount <= 0) {
            continue;
        }
        const int size = MultiByteToWideChar(CP_UTF8, 0, pendingBytes.data(), byteCount, nullptr, 0);
        if (size <= 0) {
            continue;
        }

        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pendingBytes.data(), byteCount, wide.data(), size);
        decodedText->swap(wide);
        pendingBytes.erase(0, byteCount);
        return true;
    }

    if (pendingBytes.size() > 4) {
        *decodedText = Utf8ToWide(pendingBytes);
        pendingBytes.clear();
        return !decodedText->empty();
    }
    return false;
}

HANDLE CreateKillOnCloseJobObject() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        return nullptr;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &limits,
            sizeof(limits))) {
        CloseHandle(job);
        return nullptr;
    }

    return job;
}

std::wstring ReadFileToWide(const std::wstring& filePath) {
    std::ifstream input(filePath.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return L"";
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return Utf8ToWide(buffer.str());
}

std::vector<std::wstring> SplitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::wstring StripAnsiEscapes(const std::wstring& text) {
    std::wstring cleaned;
    cleaned.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\x001b' && i + 1 < text.size() && text[i + 1] == L'[') {
            i += 2;
            while (i < text.size() && text[i] != L'm') {
                ++i;
            }
            continue;
        }
        cleaned.push_back(text[i]);
    }
    return cleaned;
}

std::wstring TrimText(const std::wstring& value) {
    const wchar_t* whitespace = L" \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

}  // namespace

bool GitRunner::IsGitRepository(const std::wstring& repoPath) {
    const GitCommandResult result = RunGitCommand(repoPath, {L"rev-parse", L"--is-inside-work-tree"});
    return result.success && result.output.find(L"true") != std::wstring::npos;
}

GitCommandResult GitRunner::RunGitCommand(
    const std::wstring& repoPath,
    const std::vector<std::wstring>& args,
    HANDLE cancelEvent,
    const std::function<void(const std::wstring&)>& outputCallback) {
    GitCommandResult result;
    result.commandLine = L"git " + JoinArguments(args);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        result.output = L"Failed to create pipe.\r\n";
        return result;
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<std::wstring> processArgs;
    processArgs.push_back(L"-c");
    processArgs.push_back(L"color.ui=always");
    processArgs.insert(processArgs.end(), args.begin(), args.end());
    std::wstring commandLine = L"git " + JoinArguments(processArgs);
    BOOL created = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        repoPath.empty() ? nullptr : repoPath.c_str(),
        &si,
        &pi);

    CloseHandle(writePipe);

    if (!created) {
        CloseHandle(readPipe);
        result.output = L"Failed to start git process.\r\n";
        return result;
    }

    HANDLE job = CreateKillOnCloseJobObject();
    if (job != nullptr) {
        if (!AssignProcessToJobObject(job, pi.hProcess)) {
            CloseHandle(job);
            job = nullptr;
        }
    }

    std::string outputBuffer;
    std::string pendingUtf8Bytes;
    char temp[4096];
    bool processFinished = false;
    while (!processFinished) {
        DWORD available = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            const DWORD toRead = std::min<DWORD>(available, static_cast<DWORD>(sizeof(temp)));
            DWORD bytesRead = 0;
            if (ReadFile(readPipe, temp, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                outputBuffer.append(temp, temp + bytesRead);
                if (outputCallback) {
                    pendingUtf8Bytes.append(temp, temp + bytesRead);
                    std::wstring decodedChunk;
                    while (DecodeUtf8Prefix(pendingUtf8Bytes, &decodedChunk)) {
                        outputCallback(decodedChunk);
                        result.outputStreamed = true;
                        decodedChunk.clear();
                    }
                }
            }
            continue;
        }

        if (cancelEvent != nullptr && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0) {
            if (job != nullptr) {
                TerminateJobObject(job, 1);
            } else {
                TerminateProcess(pi.hProcess, 1);
            }
            result.cancelled = true;
            break;
        }

        const DWORD waitResult = WaitForSingleObject(pi.hProcess, 50);
        processFinished = (waitResult == WAIT_OBJECT_0);
    }

    DWORD bytesRead = 0;
    while (ReadFile(readPipe, temp, sizeof(temp), &bytesRead, nullptr) && bytesRead > 0) {
        outputBuffer.append(temp, temp + bytesRead);
        if (outputCallback) {
            pendingUtf8Bytes.append(temp, temp + bytesRead);
            std::wstring decodedChunk;
            while (DecodeUtf8Prefix(pendingUtf8Bytes, &decodedChunk)) {
                outputCallback(decodedChunk);
                result.outputStreamed = true;
                decodedChunk.clear();
            }
        }
    }
    if (outputCallback && !pendingUtf8Bytes.empty()) {
        outputCallback(Utf8ToWide(pendingUtf8Bytes));
        result.outputStreamed = true;
        pendingUtf8Bytes.clear();
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.success = !result.cancelled && (exitCode == 0);
    result.output = Utf8ToWide(outputBuffer);
    if (result.cancelled) {
        if (!result.output.empty() && result.output.back() != L'\n') {
            result.output += L"\r\n";
        }
        result.output += L"Command cancelled by user.\r\n";
    }

    CloseHandle(readPipe);
    if (job != nullptr) {
        CloseHandle(job);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

std::vector<CommitInfo> GitRunner::GetRecentCommits(const std::wstring& repoPath, int limit, HANDLE cancelEvent) {
    std::vector<CommitInfo> commits;
    const std::wstring format = L"%h%x1f%s%x1f%an%x1f%ad";
    const GitCommandResult result = RunGitCommand(
        repoPath,
        {L"log", L"--date=short", L"--pretty=format:" + format, L"-n", std::to_wstring(limit)},
        cancelEvent);
    if (!result.success) {
        return commits;
    }

    for (const std::wstring& line : SplitLines(StripAnsiEscapes(result.output))) {
        if (line.empty()) {
            continue;
        }
        size_t first = line.find(L'\x001f');
        size_t second = first == std::wstring::npos ? std::wstring::npos : line.find(L'\x001f', first + 1);
        size_t third = second == std::wstring::npos ? std::wstring::npos : line.find(L'\x001f', second + 1);
        if (first == std::wstring::npos || second == std::wstring::npos || third == std::wstring::npos) {
            continue;
        }

        CommitInfo info;
        info.hash = line.substr(0, first);
        info.message = line.substr(first + 1, second - first - 1);
        info.author = line.substr(second + 1, third - second - 1);
        info.date = line.substr(third + 1);
        commits.push_back(info);
    }

    return commits;
}

std::vector<CommitInfo> GitRunner::GetUnpushedCommits(const std::wstring& repoPath, int limit) {
    std::vector<CommitInfo> commits;
    const std::wstring format = L"%h%x1f%s%x1f%an%x1f%ad";
    std::vector<std::wstring> args = {
        L"log", L"--date=short", L"--pretty=format:" + format, L"-n", std::to_wstring(limit)
    };

    const GitCommandResult upstreamResult = RunGitCommand(repoPath, {L"rev-parse", L"--abbrev-ref", L"@{upstream}"});
    if (upstreamResult.success) {
        args.push_back(L"@{upstream}..HEAD");
    }

    const GitCommandResult result = RunGitCommand(repoPath, args);
    if (!result.success) {
        return commits;
    }

    for (const std::wstring& line : SplitLines(StripAnsiEscapes(result.output))) {
        if (line.empty()) {
            continue;
        }
        size_t first = line.find(L'\x001f');
        size_t second = first == std::wstring::npos ? std::wstring::npos : line.find(L'\x001f', first + 1);
        size_t third = second == std::wstring::npos ? std::wstring::npos : line.find(L'\x001f', second + 1);
        if (first == std::wstring::npos || second == std::wstring::npos || third == std::wstring::npos) {
            continue;
        }

        CommitInfo info;
        info.hash = line.substr(0, first);
        info.message = line.substr(first + 1, second - first - 1);
        info.author = line.substr(second + 1, third - second - 1);
        info.date = line.substr(third + 1);
        commits.push_back(info);
    }

    return commits;
}

std::vector<std::wstring> GitRunner::GetLocalBranches(const std::wstring& repoPath) {
    std::vector<std::wstring> branches;
    const GitCommandResult result = RunGitCommand(repoPath, {L"branch", L"--format=%(refname:short)"});
    if (!result.success) {
        return branches;
    }
    for (const std::wstring& line : SplitLines(StripAnsiEscapes(result.output))) {
        if (!line.empty()) {
            branches.push_back(line);
        }
    }
    return branches;
}

std::wstring GitRunner::GetCurrentBranch(const std::wstring& repoPath) {
    const GitCommandResult result = RunGitCommand(repoPath, {L"branch", L"--show-current"});
    if (!result.success) {
        return L"";
    }
    for (const std::wstring& line : SplitLines(StripAnsiEscapes(result.output))) {
        if (!line.empty()) {
            return line;
        }
    }
    return L"";
}

std::wstring GitRunner::GetCommitDetails(const std::wstring& repoPath, const std::wstring& commitHash) {
    const GitCommandResult result = RunGitCommand(
        repoPath,
        {L"show", L"--date=format:%Y-%m-%d %H:%M:%S", L"--format=%H%x1f%cn%x1f%cd%x1f%B", L"--no-patch", commitHash});
    return StripAnsiEscapes(result.output);
}

std::vector<CommitFileDiff> GitRunner::GetCommitFileDiffs(
    const std::wstring& repoPath,
    const std::wstring& commitHash,
    bool includePatch) {
    std::vector<CommitFileDiff> diffs;
    const GitCommandResult namesResult = RunGitCommand(
        repoPath,
        {L"diff-tree", L"--root", L"--no-commit-id", L"--find-renames", L"--name-status", L"-r", commitHash});
    if (!namesResult.success) {
        return diffs;
    }

    for (const std::wstring& line : SplitLines(StripAnsiEscapes(namesResult.output))) {
        if (line.empty()) {
            continue;
        }
        CommitFileDiff diff;
        diff.status = line[0];
        const size_t firstTabPos = line.find(L'\t');
        if (firstTabPos == std::wstring::npos) {
            diff.path = TrimText(line.substr(1));
            diff.oldPath = diff.path;
            diff.newPath = diff.path;
            diff.patchPath = diff.path;
        } else if (diff.status == L'R') {
            const size_t secondTabPos = line.find(L'\t', firstTabPos + 1);
            const std::wstring oldPath = line.substr(firstTabPos + 1, secondTabPos - firstTabPos - 1);
            const std::wstring newPath = secondTabPos == std::wstring::npos
                ? oldPath
                : line.substr(secondTabPos + 1);
            diff.oldPath = oldPath;
            diff.newPath = newPath;
            diff.path = oldPath + L" -> " + newPath;
            diff.patchPath = newPath;
        } else {
            diff.path = line.substr(firstTabPos + 1);
            diff.oldPath = diff.path;
            diff.newPath = diff.path;
            diff.patchPath = diff.path;
        }

        if (includePatch) {
            const GitCommandResult patchResult = RunGitCommand(
                repoPath, {L"show", L"--format=", commitHash, L"--", diff.patchPath});
            diff.patch = patchResult.output;
        }
        diffs.push_back(diff);
    }
    return diffs;
}

std::wstring GitRunner::GetFileContentAtRevision(
    const std::wstring& repoPath,
    const std::wstring& revision,
    const std::wstring& filePath) {
    if (revision.empty() || filePath.empty()) {
        return L"";
    }

    const GitCommandResult result = RunGitCommand(
        repoPath,
        {L"show", revision + L":" + filePath});
    return result.success ? result.output : L"";
}

std::vector<CommitFileDiff> GitRunner::GetWorkingTreeDiffs(const std::wstring& repoPath) {
    std::vector<CommitFileDiff> diffs;
    const GitCommandResult result = RunGitCommand(
        repoPath,
        {L"status", L"--short", L"--untracked-files=all"});
    if (!result.success) {
        return diffs;
    }

    for (const std::wstring& line : SplitLines(StripAnsiEscapes(result.output))) {
        if (line.size() < 3) {
            continue;
        }

        CommitFileDiff diff;
        if (line.rfind(L"?? ", 0) == 0) {
            diff.status = L'A';
            diff.path = line.substr(3);
            diff.oldPath.clear();
            diff.newPath = diff.path;
            diff.patchPath = diff.path;
        } else {
            const wchar_t x = line[0];
            const wchar_t y = line[1];
            const std::wstring pathText = line.substr(3);

            if (x == L'R' || y == L'R') {
                diff.status = L'R';
                const size_t arrowPos = pathText.find(L" -> ");
                if (arrowPos != std::wstring::npos) {
                    diff.oldPath = pathText.substr(0, arrowPos);
                    diff.newPath = pathText.substr(arrowPos + 4);
                    diff.path = diff.oldPath + L" -> " + diff.newPath;
                    diff.patchPath = diff.newPath;
                } else {
                    diff.path = pathText;
                    diff.oldPath = pathText;
                    diff.newPath = pathText;
                    diff.patchPath = pathText;
                }
            } else if (x == L'D' || y == L'D') {
                diff.status = L'D';
                diff.path = pathText;
                diff.oldPath = pathText;
                diff.newPath.clear();
                diff.patchPath = pathText;
            } else if (x == L'A' || y == L'A') {
                diff.status = L'A';
                diff.path = pathText;
                diff.oldPath.clear();
                diff.newPath = pathText;
                diff.patchPath = pathText;
            } else {
                diff.status = L'M';
                diff.path = pathText;
                diff.oldPath = pathText;
                diff.newPath = pathText;
                diff.patchPath = pathText;
            }
        }
        diffs.push_back(diff);
    }
    return diffs;
}

std::wstring GitRunner::ReadWorkingTreeFile(const std::wstring& filePath) {
    return ReadFileToWide(filePath);
}
