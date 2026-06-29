// find.cpp - WORKS EVERYWHERE with MinGW-w64 32-bit
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <shellapi.h>
// Thread
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

// Libraries
#pragma comment(lib, "comctl32")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "shell32")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Object IDs
#define IDC_EDIT_PATH     1001
#define IDC_BTN_BROWSE    1002
#define IDC_EDIT_NAME     1003
#define IDC_RADIO_START   1004
#define IDC_RADIO_CONTAIN 1005
#define IDC_RADIO_EXACT   1006
#define IDC_RADIO_END     1012
#define IDC_CHECK_FILES   1007
#define IDC_CHECK_FOLDERS 1008
#define IDC_BTN_SEARCH    1009
#define IDC_LIST          1010
#define IDC_STATUS        1011

#define WM_UPDATE_STATUS (WM_APP + 1)

// Thread
std::mutex listMutex;
std::atomic<int> activeThreads{0};
std::atomic<bool> searchCancelled{false};

std::queue<std::wstring> dirQueue;
std::mutex queueMutex;
std::condition_variable queueCV;

std::atomic<bool> searchFinished{false};  // ← THE SACRED FLAG

// Core variables
int idx = 0;

int listHeight = 640;
int listWidth = 882;

// Language translation system
std::string lang = "en"; // Select upon compile
std::vector<LPWSTR> de = {
    L"Dateisucher", L"Such in:", L"Name:", L"Suchen", L"Fängt an mit", L"Hat", L"Ganzer Name", L"Dateien", L"Ordner", L"Name", L"Ort", L"Bereit", L"Wird gesucht...", L"Klicken Sie auf einen Ordner", L"Name eingeben", L"Endet mit"
};
std::vector<LPWSTR> fr = {
    L"Chercheur de fichiers", L"Cherche en:", L"Nom:", L"Chercher", L"Commence par", L"Contient", L"Nom entier", L"Fichiers", L"Dossiers", L"Nom", L"Emplacement", L"Prêt", L"Est cherché...", L"Cliquez sur un dossier", L"Entrez le nom", L"Termine par"
};
std::vector<LPWSTR> ru = {
    L"Fajl-iskatélj", L"Iskatj v:", L"Imja:", L"Iskatj", L"Nachajetsja s", L"Jéstj", L"Tseloje imja", L"Fajly", L"Papki", L"Imja", L"Mjesto", L"Gotov", L"Ischét...", L"Nazhmitje na papku", L"Vvéditje imja", L"Konchajetsja s"
};
std::vector<LPWSTR> kb = {
    L"Haroal oû fikeesu", L"Haroalgle vef:", L"Fikelatono:", L"Haroalgle", L"Ri kedamo", L"Vefke", L"Laet filatono", L"Fikees", L"Kees oû fikeesu", L"Fikelatono", L"Ro", L"Kaas", L"Haroalgle...", L"Nuka vef afü keu oû fikeesu", L"Femogle filatono", L"Rimo kedamo"
};
std::vector<LPWSTR> en = {
    L"File searcher", L"Search in:", L"Name:", L"Search", L"Starts with", L"Has", L"Full name", L"Files", L"Folders", L"Name", L"Location", L"Ready", L"Searching...", L"Click on a folder", L"Enter a name", L"Ends with"
};

LPWSTR text(std::string l, int p) {
    if (l == "de") return de[p]; // German
    if (l == "fr") return fr[p]; // French
    if (l == "ru") return ru[p]; // Russian
    if (l == "kb") return kb[p]; // Keda-Batenu
    if (l == "en") return en[p]; // English
}

HWND hWndMain, hEditPath, hEditName, hList, hStatus;
int matchMode = 0;
bool searchFiles = true, searchFolders = true;
std::vector<std::wstring> roots;

// Initialize
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateControls(HWND);
void AddColumns();
void BrowseFolders();
void DoSearch();
bool Matches(const std::wstring&, const std::wstring&);
void SearchDir(const std::wstring&, const std::wstring&);

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM lpData) {
    if (uMsg == BFFM_INITIALIZED) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    // Create window
    InitCommonControls();

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FileSearchClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    hWndMain = CreateWindow(L"FileSearchClass", text(lang, 0),
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 904, 860,
                            NULL, NULL, hInst, NULL);

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void CopyTextToClipboard(HWND hwnd, const std::wstring& text)
{
    if (!OpenClipboard(hwnd))
        return; // failed to open clipboard

    EmptyClipboard(); // clear previous contents

    // Allocate global memory for the text
    size_t sizeInBytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
    if (!hMem) {
        CloseClipboard();
        return;
    }

    // Copy the string into the global memory
    void* ptr = GlobalLock(hMem);
    memcpy(ptr, text.c_str(), sizeInBytes);
    GlobalUnlock(hMem);

    // Set the clipboard data as Unicode text
    SetClipboardData(CF_UNICODETEXT, hMem);

    CloseClipboard();


}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    // On load 
    case WM_ENTERSIZEMOVE: case WM_ENTERMENULOOP: return 0;
    case WM_CREATE: CreateControls(hwnd); AddColumns(); break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_BROWSE: BrowseFolders(); break;
        case IDC_BTN_SEARCH: DoSearch(); break;
        case IDC_RADIO_START:   matchMode = 0; break;
        case IDC_RADIO_CONTAIN: matchMode = 1; break;
        case IDC_RADIO_EXACT:   matchMode = 2; break;
        case IDC_RADIO_END:   matchMode = 3; break;
        }
        break;
    // When double-clicked
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->idFrom == IDC_LIST && ((LPNMHDR)lParam)->code == NM_RDBLCLK) {
            int i = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (i >= 0) {
                WCHAR path[MAX_PATH];
                ListView_GetItemText(hList, i, 1, path, MAX_PATH);
                CopyTextToClipboard(hWndMain, path);
            }
        }
        else if (((LPNMHDR)lParam)->idFrom == IDC_LIST && ((LPNMHDR)lParam)->code == NM_DBLCLK) {
            int i = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (i >= 0) {
                WCHAR path[MAX_PATH];
                ListView_GetItemText(hList, i, 1, path, MAX_PATH);
                ShellExecuteW(hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL);
            }
        }
        break;
    // Resize after drawing
    case WM_SIZE: {
            RECT rc; GetClientRect(hwnd, &rc);
            SetWindowPos(hStatus, NULL, 0, rc.bottom-20, rc.right, 20, SWP_NOZORDER);
            SetWindowPos(hList,   NULL, 10, 160, listWidth, listHeight, SWP_NOZORDER);

            // Adjust column widths dynamically
            int totalWidth = rc.right - 20;  // listview width
            ListView_SetColumnWidth(hList, 0, 260);               // first column fixed
            ListView_SetColumnWidth(hList, 1, totalWidth - 260);  // second column fills remaining
        } break;

    case WM_UPDATE_STATUS: {
            LPCWSTR txt = (LPCWSTR)lParam;
            SendMessageW(hStatus, SB_SETTEXTW, 0, (LPARAM)txt);
            return 0;
        } break;
    // When closing
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Create UI
void CreateControls(HWND hwnd)
{
    int y = 15;
    CreateWindow(L"STATIC", text(lang, 1), WS_VISIBLE|WS_CHILD, 10, y, 130, 23, hwnd, NULL, NULL, NULL);
    hEditPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"C:\\", WS_VISIBLE|WS_CHILD|ES_AUTOHSCROLL,
                               130, y-3, 650, 26, hwnd, (HMENU)IDC_EDIT_PATH, NULL, NULL);
    CreateWindow(L"BUTTON", L"...", WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                 790, y-3, 40, 26, hwnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

    y += 40;
    CreateWindow(L"STATIC", text(lang, 2), WS_VISIBLE|WS_CHILD, 10, y, 90, 23, hwnd, NULL, NULL, NULL);
    hEditName = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE|WS_CHILD,
                               130, y-3, 300, 26, hwnd, (HMENU)IDC_EDIT_NAME, NULL, NULL);

    y += 40;
    CreateWindow(L"BUTTON", text(lang, 4),  WS_VISIBLE|WS_CHILD|BS_AUTORADIOBUTTON|WS_GROUP,
                 20, y, 130, 23, hwnd, (HMENU)IDC_RADIO_START, NULL, NULL);
    CreateWindow(L"BUTTON", text(lang, 5),    WS_VISIBLE|WS_CHILD|BS_AUTORADIOBUTTON,
                 150, y, 100, 23, hwnd, (HMENU)IDC_RADIO_CONTAIN, NULL, NULL);
    CreateWindow(L"BUTTON", text(lang, 6), WS_VISIBLE|WS_CHILD|BS_AUTORADIOBUTTON,
                 260, y, 110, 23, hwnd, (HMENU)IDC_RADIO_EXACT, NULL, NULL);
    CreateWindow(L"BUTTON", text(lang, 15), WS_VISIBLE|WS_CHILD|BS_AUTORADIOBUTTON,
                 380, y, 130, 23, hwnd, (HMENU)IDC_RADIO_END, NULL, NULL);
    CheckRadioButton(hwnd, IDC_RADIO_START, IDC_RADIO_EXACT, IDC_RADIO_START);

    y += 35;
    CreateWindow(L"BUTTON", text(lang, 7),   WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                 20, y, 80, 23, hwnd, (HMENU)IDC_CHECK_FILES, NULL, NULL);
    CreateWindow(L"BUTTON", text(lang, 8), WS_VISIBLE|WS_CHILD|BS_AUTOCHECKBOX,
                 110, y, 130, 23, hwnd, (HMENU)IDC_CHECK_FOLDERS, NULL, NULL);
    CheckDlgButton(hwnd, IDC_CHECK_FILES, BST_CHECKED);
    CheckDlgButton(hwnd, IDC_CHECK_FOLDERS, BST_CHECKED);

    CreateWindow(L"BUTTON", text(lang, 3), WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,
                 750, y-78, 100, 26, hwnd, (HMENU)IDC_BTN_SEARCH, NULL, NULL);

    y += 60;
    hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
                           WS_VISIBLE|WS_CHILD|LVS_REPORT|LVS_SHOWSELALWAYS,
                           10, y, listWidth, listHeight, hwnd, (HMENU)IDC_LIST, NULL, NULL);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);

    hStatus = CreateWindow(STATUSCLASSNAME, text(lang, 11), WS_VISIBLE|WS_CHILD|SBARS_SIZEGRIP,
                           0,0,0,0, hwnd, (HMENU)IDC_STATUS, NULL, NULL);
}


// Add entries to the list
void AddColumns()
{
    LVCOLUMNW lv = { LVCF_TEXT | LVCF_WIDTH };
    lv.pszText = text(lang, 9);
    lv.cx = 260; 
    ListView_InsertColumn(hList, 0, &lv);

    lv.pszText = text(lang, 10);
    lv.cx = 100; // initial width doesn't matter, will be resized in WM_SIZE
    ListView_InsertColumn(hList, 1, &lv);
}

// Opens a folder browser menu
void BrowseFolders()
{
    WCHAR path[MAX_PATH] = L"C:\\";
    BROWSEINFO bi = { hWndMain, NULL, path, text(lang, 13), BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE, BrowseCallbackProc, (LPARAM)L"C:\\", 0 };
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        SHGetPathFromIDList(pidl, path);
        CoTaskMemFree(pidl);
        SetWindowTextW(hEditPath, path);
    }
}

// Use the default Windows matcher
bool Matches(const std::wstring& name, const std::wstring& pat)
{
    std::wstring pattern = pat;

    // Build wildcard pattern
    if (matchMode == 0)      // Starts with
        pattern += L"*";
    else if (matchMode == 1) // Has
        pattern = L"*" + pattern + L"*";
    else if (matchMode == 3) // Ends with
        pattern = L"*" + pattern;
    // matchMode == 2 → Full name

    // Use Windows' own ultra-fast matcher — case-insensitive by default
    return PathMatchSpecW(name.c_str(), pattern.c_str()) != FALSE;
}

// (Semi-)optimized search function
// FIXED SearchDir() — only change: use SendMessage instead of direct macro
// (this is the ONE thing that makes ListView thread-safe without freezing)
void SearchDir(const std::wstring& dir, const std::wstring& pat)
{
    std::wstring spec = dir;
    if (!spec.empty() && spec.back() != L'\\' && spec.back() != L'/')
        spec += L'\\';
    spec += L"*";

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(spec.c_str(),
                                    FindExInfoBasic,
                                    &fd,
                                    FindExSearchNameMatch,
                                    nullptr,
                                    FIND_FIRST_EX_LARGE_FETCH);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    std::vector<std::wstring> subdirs;
    int filesChecked = 0, matchesFound = 0;

    do {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' ||
            (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;

        std::wstring name = fd.cFileName;
        std::wstring fullPath = dir + L"\\" + name;
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        filesChecked++;

        auto addItem = [&](bool isFolder) {
            if ((isFolder && !searchFolders) || (!isFolder && !searchFiles)) return;
            if (!Matches(name, pat)) return;

            matchesFound++;

            std::lock_guard<std::mutex> lock(listMutex);

            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = 0xFFFFFF;  // append
            item.pszText = (LPWSTR)name.c_str();

            int idx = (int)SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&item);
            if (idx != -1) {
                LVITEMW col{};
                col.mask = LVIF_TEXT;
                col.iItem = idx;
                col.iSubItem = 1;
                if (fullPath[3] == L'\\') fullPath.erase(3, 1);
                col.pszText = (LPWSTR)fullPath.c_str();
                SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&col);
            }
        };

        if (isDir)
            addItem(true);
        else
            addItem(false);

        if (isDir)
            subdirs.push_back(fullPath);

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    if (!subdirs.empty()) {
        std::lock_guard<std::mutex> lock(queueMutex);
        for (const auto& sub : subdirs)
            dirQueue.push(sub);
        queueCV.notify_all();
    }
}

// Start the search
void DoSearch()
{
    WCHAR pat[512] = {};
    GetWindowTextW(hEditName, pat, 512);
    if (!pat[0]) {
        SendMessage(hStatus, SB_SETTEXTW, 0, (LPARAM)text(lang, 14));
        return;
    }

    searchCancelled = false;
    idx = 0;
    ListView_DeleteAllItems(hList);
    SendMessage(hStatus, SB_SETTEXTW, 0, (LPARAM)text(lang, 12));

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::queue<std::wstring>().swap(dirQueue);
    }

    // Parse root paths
    WCHAR buf[8192] = {};
    GetWindowTextW(hEditPath, buf, 8192);
    roots.clear();
    std::wstring s(buf);
    std::wistringstream iss(s);
    std::wstring token;
    while (std::getline(iss, token, L';')) {
        token.erase(0, token.find_first_not_of(L" \t"));
        token.erase(token.find_last_not_of(L" \t") + 1);
        if (!token.empty()) roots.push_back(token);
    }
    if (roots.empty()) roots.push_back(L"C:\\");

    unsigned int workerCount = std::max(1u, std::thread::hardware_concurrency() - 1);
    activeThreads = workerCount;

    // Reset searchFinished in preparation
    searchFinished = false;

    // Spawn worker threads
    for (unsigned i = 0; i < workerCount; ++i) {
        std::thread([pat]() {
            while (!searchFinished) {
                std::wstring nextDir;

                {
                    std::unique_lock<std::mutex> lock(queueMutex);

                    // Wake up if: work available OR someone already finished
                    queueCV.wait(lock, [] {
                        return !dirQueue.empty() || !roots.empty() || searchFinished;
                    });

                    // If someone already declared victory → we die
                    if (searchFinished) {
                        break;
                    }

                    // If no work and no one finished → last thread? keep trying
                    if (dirQueue.empty() && roots.empty()) {
                        // We're the last one alive and nothing left → WE declare victory
                        searchFinished = true;
                        queueCV.notify_all();  // wake others to die
                        break;
                    }

                    // Grab work
                    if (!dirQueue.empty()) {
                        nextDir = std::move(dirQueue.front());
                        dirQueue.pop();
                    } else {
                        nextDir = std::move(roots.back());
                        roots.pop_back();
                    }
                }

                if (!nextDir.empty()) {
                    SearchDir(nextDir, pat);

                    // CRITICAL: After finishing a directory, check if we should declare victory
                    // (only if no one else has, and there's no more work queued)
                    if (!searchFinished) {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if (dirQueue.empty() && roots.empty()) {
                            searchFinished = true;
                            queueCV.notify_all();
                        }
                    }
                }
            }

            // Clean exit
            int remaining = --activeThreads;

            // Last one out turns off the lights
            if (remaining == 0) {
                queueCV.notify_all();
            }

        }).detach();
    }

    // Kick off search by pushing initial roots into queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        for (auto it = roots.rbegin(); it != roots.rend(); ++it)
            dirQueue.push(*it);
        roots.clear();
        queueCV.notify_all();
    }

    std::thread([] {
    auto startTime = std::chrono::steady_clock::now();

    // Wait until ALL worker threads are truly gone
    while (activeThreads.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    int count = ListView_GetItemCount(hList);

    // BUILD THE FINAL STATUS MESSAGE — ONE BUFFER, NO MISTAKES
    WCHAR statusMsg[512] = {0};

    if (lang == "de") {
        wsprintfW(statusMsg, L"%d Datei%s/Ordner gefunden.", ListView_GetItemCount(hList), ListView_GetItemCount(hList) == 1 ? L"" : L"en");
    }
    if (lang == "fr") {
        wsprintfW(statusMsg, L"%d fichier%s/dossier%s trouvé%s.", ListView_GetItemCount(hList), ListView_GetItemCount(hList) == 1 ? L"" : L"s", ListView_GetItemCount(hList) == 1 ? L"" : L"s", ListView_GetItemCount(hList) == 1 ? L"" : L"s");
    }
    if (lang == "ru") {
        wsprintfW(statusMsg, L"%d fajl%s/pap%s najdén%s.", ListView_GetItemCount(hList), ListView_GetItemCount(hList) == 1 ? L"" : ((0 < ListView_GetItemCount(hList) && ListView_GetItemCount(hList) < 5) ? L"a" : L"ov"), ListView_GetItemCount(hList) == 1 ? L"ka" : ((0 < ListView_GetItemCount(hList) && ListView_GetItemCount(hList) < 5) ? L"ki" : L"ok"), ListView_GetItemCount(hList) == 1 ? L"" : L"o");
    }
    if (lang == "kb") {
        wsprintfW(statusMsg, L"%d fike%s/ke%s oû fikeesu dealepa oû harou.", ListView_GetItemCount(hList), ListView_GetItemCount(hList) == 1 ? L"" : L"es", ListView_GetItemCount(hList) == 1 ? L"" : L"es");
    }
    if (lang == "en") {
        wsprintfW(statusMsg, L"%d file%s/folder%s found.", ListView_GetItemCount(hList), ListView_GetItemCount(hList) == 1 ? L"" : L"s", ListView_GetItemCount(hList) == 1 ? L"" : L"s");
    }

    // UPDATE STATUS BAR — ONLY ONCE, WITH THE CORRECT STRING
    SendMessageW(hStatus, SB_SETTEXTW, 0, (LPARAM)statusMsg);

    // No PostMessage needed — debug window is already live and updating in real time
    }).detach();
}
// g++ -m32 -O3 -march=i386 -mtune=generic -flto -ffast-math -fno-exceptions -fno-rtti -static -static-libgcc -static-libstdc++ -mwindows fs.cpp -lcomctl32 -lole32 -lshlwapi -luuid -pthread -latomic -s -o FileSearch32.exe
