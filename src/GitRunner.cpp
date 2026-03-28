#include "GitRunner.h"

#include <windows.h>

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

GitCommandResult GitRunner::RunGitCommand(const std::wstring& repoPath, const std::vector<std::wstring>& args) {
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

    std::string outputBuffer;
    char temp[4096];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, temp, sizeof(temp), &bytesRead, nullptr) && bytesRead > 0) {
        outputBuffer.append(temp, temp + bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.success = (exitCode == 0);
    result.output = Utf8ToWide(outputBuffer);

    CloseHandle(readPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

std::vector<CommitInfo> GitRunner::GetRecentCommits(const std::wstring& repoPath, int limit) {
    std::vector<CommitInfo> commits;
    const std::wstring format = L"%h%x1f%s%x1f%an%x1f%ad";
    const GitCommandResult result = RunGitCommand(
        repoPath,
        {L"log", L"--date=short", L"--pretty=format:" + format, L"-n", std::to_wstring(limit)});
    if (!result.success) {
        return commits;
    }

    for (const std::wstring& line : SplitLines(result.output)) {
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
    for (const std::wstring& line : SplitLines(result.output)) {
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
    for (const std::wstring& line : SplitLines(result.output)) {
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
    return result.success ? result.output : result.output;
}

std::vector<CommitFileDiff> GitRunner::GetCommitFileDiffs(const std::wstring& repoPath, const std::wstring& commitHash) {
    std::vector<CommitFileDiff> diffs;
    const GitCommandResult namesResult = RunGitCommand(
        repoPath,
        {L"diff-tree", L"--no-commit-id", L"--find-renames", L"--name-status", L"-r", commitHash});
    if (!namesResult.success) {
        return diffs;
    }

    for (const std::wstring& line : SplitLines(namesResult.output)) {
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

        const GitCommandResult patchResult = RunGitCommand(
            repoPath, {L"show", L"--format=", commitHash, L"--", diff.patchPath});
        diff.patch = patchResult.output;
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
