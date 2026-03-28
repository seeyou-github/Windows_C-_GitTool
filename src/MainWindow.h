#pragma once

#include "CacheDatabase.h"
#include "CommitRepository.h"
#include "ProjectStore.h"

#include <richedit.h>
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

class MainWindow {
public:
    MainWindow();
    bool Create(HINSTANCE instance, int nCmdShow);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LogEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LogScrollBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI AsyncGitCommandThread(LPVOID param);
    static DWORD WINAPI AsyncCommitRefreshThread(LPVOID param);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void ShowControls();
    void LayoutControls(int width, int height);
    void SetCommandUiState(bool running, const std::wstring& statusText = L"");
    void UpdateProjectListColumnWidth(int listWidth);
    void UpdateLogScrollBar();
    void ScrollLogToPosition(int position);
    void ApplyFonts();
    void LoadProjectsIntoList();
    void RefreshCurrentRepository(bool runStatusCommand);
    void RefreshCommitList();
    void PopulateCommitList(const std::vector<CommitInfo>& commits);
    void ShowCommitPlaceholder(const std::wstring& message);
    void ClearLog();
    void AppendLog(const std::wstring& text);
    void AppendLogRichText(const std::wstring& text, COLORREF color);
    void AppendCommandResult(const GitCommandResult& result);
    void SelectProjectByPath(const std::wstring& path);
    std::wstring GetSelectedProjectPath() const;
    std::wstring GetSelectedCommitHash() const;
    std::wstring GetSelectedProjectDisplayName() const;
    void AddFolder();
    void RemoveSelectedProject();
    void ShowProjectContextMenu(POINT screenPoint);
    void ShowCommitContextMenu(POINT screenPoint);
    void ShowCommitDetails();
    void OpenSelectedInExplorer();
    void OpenSelectedInTerminal();
    void RunSimpleCommand(
        const std::vector<std::wstring>& args,
        bool refreshStatusAfter = true,
        bool refreshCommitsAfter = true,
        const std::wstring& cleanupFilePath = L"");
    void StartAsyncGitCommand(
        const std::wstring& repoPath,
        const std::vector<std::wstring>& args,
        bool refreshStatusAfter,
        bool refreshCommitsAfter,
        const std::wstring& cleanupFilePath);
    void RunGitInit(const std::wstring& repoPath = L"");
    void RunCommit();
    void ShowBranchMenu();
    void HandleBranchMenuCommand(UINT commandId);
    void ShowRemoteMenu();
    void HandleRemoteMenuCommand(UINT commandId);
    void HandleCommitMenuCommand(UINT commandId);
    bool CanEditSelectedCommitMessage() const;
    std::wstring PromptForCommitMessage(const std::wstring& repoPath, bool* accepted);
    void SetButtonText(int controlId, int stringId);
    void SetWindowTextFromString(int stringId);
    void UpdateWindowTitle();
    std::wstring PromptForText(
        int titleId,
        int promptId,
        const std::wstring& initialValue = L"",
        bool multiline = false,
        bool* accepted = nullptr);
    std::wstring LoadStringResource(int resourceId) const;
    std::wstring GetExecutableDirectory() const;
    RECT CalculateCenteredWindowRect(int width, int height) const;
    void PrepareInitialShow();
    void FinalizeInitialShow(int nCmdShow);
    void PersistWindowSize();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND addFolderButton_ = nullptr;
    HWND projectList_ = nullptr;
    HWND commitList_ = nullptr;
    HWND logEdit_ = nullptr;
    HWND logScrollBar_ = nullptr;
    HWND buttonStatus_ = nullptr;
    HWND buttonAddAll_ = nullptr;
    HWND buttonCommit_ = nullptr;
    HWND buttonPush_ = nullptr;
    HWND buttonPull_ = nullptr;
    HWND buttonFetch_ = nullptr;
    HWND buttonBranch_ = nullptr;
    HWND buttonRemote_ = nullptr;
    HWND statusLabel_ = nullptr;
    HWND stopButton_ = nullptr;
    HFONT uiFont_ = nullptr;
    HFONT projectListFont_ = nullptr;
    HFONT logFont_ = nullptr;
    HFONT menuFont_ = nullptr;
    WNDPROC defaultLogEditProc_ = nullptr;
    CacheDatabase cacheDatabase_;
    ProjectStore projectStore_;
    CommitRepository commitRepository_;
    bool commandRunning_ = false;
    std::vector<std::wstring> cachedBranches_;
    HMENU commitContextMenu_ = nullptr;
    bool logScrollDragging_ = false;
    int logScrollDragOffsetY_ = 0;
    bool initialShowPrepared_ = false;
    bool suppressProjectSelectionRefresh_ = false;
    HANDLE currentCancelEvent_ = nullptr;
    unsigned long long commitRefreshToken_ = 0;
    std::wstring currentProjectPath_;
};
