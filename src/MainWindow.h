#pragma once

#include "CacheDatabase.h"
#include "CommitRepository.h"
#include "ProjectStore.h"

#include "darkui/darkui.h"

#include <richedit.h>
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

class MainWindow {
public:
    MainWindow();
    bool Create(HINSTANCE instance, int nCmdShow);
    static void ReleaseSharedResources();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LogEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LogScrollBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI AsyncGitCommandThread(LPVOID param);
    static DWORD WINAPI AsyncCommitRefreshThread(LPVOID param);
    static LRESULT CALLBACK CommitPagerComboProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId, DWORD_PTR refData);
    static LRESULT CALLBACK CommitPagerPopupListProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR subclassId, DWORD_PTR refData);
    static LRESULT CALLBACK CommitPagerBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR subclassId, DWORD_PTR refData);
    static LRESULT CALLBACK CommitSearchEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId, DWORD_PTR refData);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void ShowControls();
    void LayoutControls(int width, int height);
    void SetCommandUiState(bool running, const std::wstring& statusText = L"");
    void RefreshStatusLabelVisual();
    void UpdateProjectListColumnWidth(int listWidth);
    void UpdateCommitListColumnWidths(int listWidth);
    void UpdateLogScrollBar();
    void ScrollLogToPosition(int position);
    void UpdateProjectSortButtons();
    void ApplyFonts();
    void LoadProjectsIntoList();
    void RefreshCurrentRepository();
    // limit: 0 = 仅读第一页, >0 = 读取指定条数, -1 = 全部读取
    // pageAfterLoad: -1 = 不跳页, >=0 = 跳到该页(0-based), -2 = 跳到最后一页
    void RefreshCommitList(int limit = 0, int pageAfterLoad = -1);
    void CancelCommitRefresh();
    void PopulateCommitList(const std::vector<CommitInfo>& commits);
    void ApplyCommitPage();
    void SetCommitPage(int page);
    void RefreshCommitDisplay();
    void ExecuteCommitSearch();
    void ToggleCommitSearch();
    void SetCommitSearchVisible(bool visible);
    void UpdateCommitPager();
    void SetCommitPagerLastLoading(bool loading);
    void LayoutCommitPagerItems(int availWidth);
    void SyncCommitPagerCombo();
    int HitTestCommitPagerItem(int x) const;
    void ShowCommitPlaceholder(const std::wstring& message);
    void ShowCommitPlaceholderRow(const std::wstring& message);
    void ClearLog();
    void AppendLog(const std::wstring& text);
    void AppendLogRichText(const std::wstring& text, COLORREF color);
    void AppendCommandOutputChunk(const std::wstring& text);
    void FlushPendingCommandOutput();
    void ResetGitProgress();
    void AppendCommandResult(const GitCommandResult& result);
    void SelectProjectByPath(const std::wstring& path);
    std::wstring GetSelectedProjectPath() const;
    std::wstring GetSelectedCommitHash() const;
    std::wstring GetSelectedProjectDisplayName() const;
    ProjectSortMode GetProjectSortMode() const;
    void SetProjectSortMode(ProjectSortMode mode);
    void AddFolder();
    void RunClone();
    void RemoveSelectedProject();
    void ShowProjectContextMenu(POINT screenPoint);
    void ShowCommitContextMenu(POINT screenPoint);
    void ShowCommitDetails();
    void OpenSelectedInExplorer();
    void OpenSelectedInTerminal();
    void OpenCurrentGitHubRepo();
    void RunSimpleCommand(
        const std::vector<std::wstring>& args,
        bool refreshCommitsAfter = true,
        const std::wstring& cleanupFilePath = L"",
        const std::vector<std::vector<std::wstring>>& preCommands = {});
    void StartAsyncGitCommand(
        const std::wstring& repoPath,
        const std::vector<std::wstring>& args,
        bool refreshCommitsAfter,
        const std::wstring& cleanupFilePath,
        const std::vector<std::vector<std::wstring>>& preCommands = {});
    void RunGitInit(const std::wstring& repoPath = L"");
    void RunCommit();
    void RunRevertSelectedCommit(const std::wstring& hash);
    void RunReapplySelectedCommit(const std::wstring& hash);
    bool ShowRevertConflictWindow(const std::wstring& repoPath, const std::wstring& commitHash);
    std::wstring PromptForRevertMessage(
        const std::wstring& title,
        const std::wstring& prompt,
        const std::wstring& initialValue,
        bool* accepted);
    void RunSquashLocalCommits();
    void RunDeleteBranch();
    void ShowBranchMenu();
    void ShowBranchSwitchMenu();
    void HandleBranchMenuCommand(UINT commandId);
    void ShowRemoteMenu();
    void HandleRemoteMenuCommand(UINT commandId);
    void HandleCommitMenuCommand(UINT commandId);
    bool CanEditSelectedCommitMessage() const;
    std::wstring PromptForCommitMessage(
        const std::wstring& repoPath,
        std::vector<std::wstring>* selectedAddPaths,
        std::vector<std::wstring>* selectedCommitPaths,
        bool* accepted);
    void SetButtonText(int controlId, int stringId);
    void SetWindowTextFromString(int stringId);
    void UpdateWindowTitle();
    void UpdateCommandButtonsEnabled(bool running);
    void RefreshCommandToolbar(bool running);
    void UpdateSelectedRepositoryState(const std::wstring& path, bool known, bool isGitRepository);
    HMENU BuildBranchToolbarMenu(bool enabled);
    HMENU BuildRemoteToolbarMenu();
    HMENU BuildOtherToolbarMenu();
    std::wstring GetGitHubWebUrlForRepo(const std::wstring& repoPath) const;
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
    void FinalizeInitialShow();
    void PersistWindowSize();
    void StopAsyncWork();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    darkui::Button addFolderButtonControl_;
    darkui::Button cloneButtonControl_;
    darkui::Button sortNameButtonControl_;
    darkui::Button sortAddedButtonControl_;
    HWND addFolderButton_ = nullptr;
    HWND cloneButton_ = nullptr;
    HWND sortNameButton_ = nullptr;
    HWND sortAddedButton_ = nullptr;
    HWND projectList_ = nullptr;
    HWND commitList_ = nullptr;
    HWND logEdit_ = nullptr;
    darkui::ScrollBar logScrollBarControl_;
    HWND logScrollBar_ = nullptr;
    darkui::Toolbar commandToolbarControl_;
    HWND commandToolbar_ = nullptr;
    darkui::Button buttonStatusControl_;
    darkui::Button buttonCommitControl_;
    darkui::Button buttonPushControl_;
    darkui::Button buttonPullControl_;
    darkui::Button buttonFetchControl_;
    darkui::Button buttonBranchControl_;
    darkui::Button buttonRemoteControl_;
    darkui::Button buttonOpenGitHubControl_;
    darkui::Button stopButtonControl_;
    HWND buttonStatus_ = nullptr;
    HWND buttonCommit_ = nullptr;
    HWND buttonPush_ = nullptr;
    HWND buttonPull_ = nullptr;
    HWND buttonFetch_ = nullptr;
    HWND buttonBranch_ = nullptr;
    HWND buttonRemote_ = nullptr;
    HWND buttonOpenGitHub_ = nullptr;
    HWND stopButton_ = nullptr;
    darkui::ComboBox commitPagerComboControl_;
    HWND commitPagerCombo_ = nullptr;
    HWND commitPagerBar_ = nullptr;
    darkui::Edit commitSearchEditControl_;
    HWND commitSearchEdit_ = nullptr;
    bool commitSearchVisible_ = false;
    int commitSearchAnimHeight_ = 0;
    int commitSearchAnimTarget_ = 0;
    std::wstring commitSearchQuery_;
    std::vector<CommitInfo> searchResults_;
    bool commitSearchActive_ = false;
    const std::vector<CommitInfo>* commitDisplayList_ = nullptr;
    struct CommitPagerItem {
        bool isPage = false;
        int page = 0;     // 1-based page number when isPage
        int action = 0;   // 1 = 下一页, 2 = 尾页 (when !isPage && action != 0)
    };
    std::vector<CommitPagerItem> commitPagerItems_;
    int commitPagerAvailWidth_ = 0;
    int commitPagerBarWidth_ = 0;
    int commitPagerButtonWidth_ = 44;
    int commitPagerEllipsisWidth_ = 24;
    int commitPagerActionWidth_ = 52;
    bool commitPagerCompressed_ = false;
    int pagerHoverIndex_ = -1;
    bool pagerBarTracking_ = false;
    darkui::ProgressBar progressBarControl_;
    HWND progressBar_ = nullptr;
    HFONT uiFont_ = nullptr;
    HFONT projectListFont_ = nullptr;
    HFONT commitListFont_ = nullptr;
    HFONT logFont_ = nullptr;
    HFONT menuFont_ = nullptr;
    WNDPROC defaultLogEditProc_ = nullptr;
    CacheDatabase cacheDatabase_;
    ProjectStore projectStore_;
    CommitRepository commitRepository_;
    bool commandRunning_ = false;
    bool selectedRepositoryStateKnown_ = false;
    bool selectedRepositoryIsGit_ = false;
    bool initialShowPrepared_ = false;
    HMENU branchToolbarMenu_ = nullptr;
    HMENU remoteToolbarMenu_ = nullptr;
    HMENU otherToolbarMenu_ = nullptr;
    std::vector<std::wstring> cachedBranches_;
    HMENU commitContextMenu_ = nullptr;
    bool logScrollDragging_ = false;
    int logScrollDragOffsetY_ = 0;
    bool suppressProjectSelectionRefresh_ = false;
    HANDLE currentCancelEvent_ = nullptr;
    HANDLE currentCommandThread_ = nullptr;
    HANDLE currentCommitRefreshThread_ = nullptr;
    HANDLE currentCommitRefreshCancelEvent_ = nullptr;
    std::wstring currentCommitRefreshPath_;
    unsigned long long commitRefreshToken_ = 0;
    std::vector<CommitInfo> allCommits_;
    std::wstring commitListRepoPath_;
    bool commitListFullyLoaded_ = false;  // 已读取到仓库末尾（或点过尾页全量读取）
    bool commitPagerLastLoading_ = false;  // 「尾页」全量读取进行中，按钮显示读取动画
    int commitPage_ = 0;
    int commitPageCount_ = 0;
    std::wstring currentProjectPath_;
    COLORREF currentCommandOutputColor_ = RGB(170, 176, 184);
    std::wstring pendingCommandOutput_;
    bool commandOutputFlushScheduled_ = false;
    std::wstring pendingCommandLineBuffer_;
    std::wstring baseStatusText_ = L"Ready";
    bool cloneProgressEnabled_ = false;
    bool stopRequested_ = false;
    RECT statusTextRect_{};
    std::wstring paintedStatusText_ = L"Ready";
    bool paintedStatusVisible_ = false;
};
