#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "resource.h"
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <thread>
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

struct SteamGame {
    std::wstring name;
    uint32_t appid;
    std::wstring exePath;
    std::wstring workingDir;
    int imageIndex; 
    bool isRunning; 
};

struct LocalizedStrings {
    std::wstring windowTitle;
    std::wstring activeUser;
    std::wstring searchPlaceholder;
    std::wstring overlayCheckbox;
    std::wstring closeSteamCheckbox;
    std::wstring killerCheckbox;
    std::wstring forceSteamCheckbox;
    std::wstring btnPlay;
    std::wstring btnStop;
    std::wstring warningMessage;
};

#define IDC_SEARCH_EDIT           101
#define IDC_GAME_LISTBOX          102
#define IDC_PLAY_BUTTON           103
#define IDC_ACCOUNT_STATIC        104
#define IDC_OVERLAY_CHECKBOX      105
#define IDC_OVERLAY_STATIC        106
#define IDC_CLOSE_STEAM_CHECKBOX  107
#define IDC_CLOSE_STEAM_STATIC    108
#define IDC_KILLER_CHECKBOX       109
#define IDC_KILLER_STATIC         110
#define IDC_FORCE_STEAM_CHECKBOX  111
#define IDC_FORCE_STEAM_STATIC    112

HWND hEditSearch = NULL;
HWND hListView = NULL;
HWND hButtonPlay = NULL;
HWND hStaticAccount = NULL;
HWND hCheckboxOverlay = NULL;
HWND hLabelOverlay = NULL;
HWND hCheckboxCloseSteam = NULL;
HWND hLabelCloseSteam = NULL;
HWND hCheckboxKiller = NULL;
HWND hLabelKiller = NULL;
HWND hCheckboxForceSteam = NULL;
HWND hLabelForceSteam = NULL;
HFONT hFont = NULL;
HIMAGELIST hImgList = NULL;

HBRUSH hBgBrush = NULL;
HBRUSH hEditBrush = NULL;

std::vector<SteamGame> allGames;
std::vector<SteamGame> displayedGames;

void UpdatePlayButtonText(HWND hWnd);
void UpdateControlStates(HWND hWnd);

LocalizedStrings GetStrings() {
    LocalizedStrings s;
    if (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN) {
        s.windowTitle = L"Steam Compact";
        s.activeUser = L"Активный аккаунт: ";
        s.searchPlaceholder = L"Искать игры...";
        s.overlayCheckbox = L"Включить внутриигровой оверлей";
        s.closeSteamCheckbox = L"Закрывать Steam после выхода из игры";
        s.killerCheckbox = L"Закрывать WebHelper (БЕТА)";
        s.forceSteamCheckbox = L"Принудительное подключение к Steam";
        s.btnPlay = L"Запустить";
        s.btnStop = L"Закрыть";
        s.warningMessage = L"Пожалуйста, выберите игру из списка для запуска";
    } else {
        s.windowTitle = L"Steam Compact";
        s.activeUser = L"Active User: ";
        s.searchPlaceholder = L"Search games...";
        s.overlayCheckbox = L"Enable Game Overlay";
        s.closeSteamCheckbox = L"Close Steam after exiting game";
        s.killerCheckbox = L"Close WebHelper (BETA)";
        s.forceSteamCheckbox = L"Force Steam Connection";
        s.btnPlay = L"Play";
        s.btnStop = L"Stop";
        s.warningMessage = L"Please select a game from the list to launch";
    }
    return s;
}

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring GetSteamInstallPath() {
    HKEY hKey;
    std::wstring steamPath = L"";
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueEx(hKey, L"SteamPath", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            steamPath = buffer;
        }
        RegCloseKey(hKey);
    }
    return steamPath;
}

std::wstring GetSteamActiveUser() {
    HKEY hKey;
    std::wstring username = L"Unknown";
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[256];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueEx(hKey, L"AutoLoginUser", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            username = buffer;
        }
        RegCloseKey(hKey);
    }
    return username;
}

std::wstring ExtractValue(const std::wstring& line, const std::wstring& key) {
    size_t keyPos = line.find(L"\"" + key + L"\"");
    if (keyPos == std::wstring::npos) return L"";
    
    size_t searchStart = keyPos + key.length() + 2;
    size_t firstQuote = line.find(L"\"", searchStart);
    if (firstQuote == std::wstring::npos) return L"";
    size_t secondQuote = line.find(L"\"", firstQuote + 1);
    if (secondQuote == std::wstring::npos) return L"";
    
    return line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::vector<std::wstring> GetLibraryPaths(const std::wstring& steamPath) {
    std::vector<std::wstring> paths;
    std::wstring vdfPath = steamPath + L"\\steamapps\\libraryfolders.vdf";
    std::ifstream file(vdfPath);
    if (!file.is_open()) {
        paths.push_back(steamPath);
        return paths;
    }

    std::string line8;
    while (std::getline(file, line8)) {
        std::wstring line = Utf8ToWstring(line8);
        size_t pos = line.find(L"\"path\"");
        if (pos != std::wstring::npos) {
            size_t firstQuote = line.find(L"\"", pos + 6);
            if (firstQuote != std::wstring::npos) {
                size_t secondQuote = line.find(L"\"", firstQuote + 1);
                if (secondQuote != std::wstring::npos) {
                    std::wstring path = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                    size_t bpos = 0;
                    while ((bpos = path.find(L"\\\\", bpos)) != std::wstring::npos) {
                        path.replace(bpos, 2, L"\\");
                        bpos += 1;
                    }
                    if (!path.empty() && path.back() == L'\\') {
                        path.pop_back();
                    }
                    if (std::find_if(paths.begin(), paths.end(), [&](const std::wstring& p) {
                        return _wcsicmp(p.c_str(), path.c_str()) == 0;
                    }) == paths.end()) {
                        paths.push_back(path);
                    }
                }
            }
        }
    }
    if (paths.empty()) paths.push_back(steamPath);
    return paths;
}

fs::path FindExecutable(const fs::path& gameDir, const std::wstring& gameName, const std::wstring& installDir) {
    std::vector<fs::path> candidates;
    if (!fs::exists(gameDir)) return "";

    auto IsInvalidExe = [](std::wstring filename) {
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        return (filename.find(L"uninstall") != std::wstring::npos ||
                filename.find(L"unins") != std::wstring::npos ||
                filename.find(L"setup") != std::wstring::npos ||
                filename.find(L"crash") != std::wstring::npos ||
                filename.find(L"redist") != std::wstring::npos ||
                filename.find(L"config") != std::wstring::npos ||
                filename.find(L"tool") != std::wstring::npos ||
                filename.find(L"server") != std::wstring::npos);
    };

    try {
        for (const auto& entry : fs::directory_iterator(gameDir)) {
            if (entry.is_regular_file() && entry.path().extension() == L".exe") {
                if (!IsInvalidExe(entry.path().filename().wstring())) {
                    candidates.push_back(entry.path());
                }
            }
        }
    } catch (...) {}

    if (candidates.empty()) {
        try {
            for (const auto& entry : fs::directory_iterator(gameDir)) {
                if (entry.is_directory()) {
                    std::wstring dirName = entry.path().filename().wstring();
                    std::transform(dirName.begin(), dirName.end(), dirName.begin(), ::tolower);
                    if (dirName.find(L"redist") != std::wstring::npos || 
                        dirName.find(L"directx") != std::wstring::npos) {
                        continue;
                    }
                    for (const auto& subEntry : fs::directory_iterator(entry.path())) {
                        if (subEntry.is_regular_file() && subEntry.path().extension() == L".exe") {
                            if (!IsInvalidExe(subEntry.path().filename().wstring())) {
                                candidates.push_back(subEntry.path());
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }

    if (candidates.empty()) return "";
    if (candidates.size() == 1) return candidates[0];

    fs::path bestMatch = candidates[0];
    int bestScore = -1;

    auto IsNotAlnum = [](wchar_t c) {
        return !((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'));
    };

    std::wstring cleanName = gameName;
    std::transform(cleanName.begin(), cleanName.end(), cleanName.begin(), ::tolower);
    cleanName.erase(std::remove_if(cleanName.begin(), cleanName.end(), IsNotAlnum), cleanName.end());

    std::wstring cleanInstallDir = installDir;
    std::transform(cleanInstallDir.begin(), cleanInstallDir.end(), cleanInstallDir.begin(), ::tolower);
    cleanInstallDir.erase(std::remove_if(cleanInstallDir.begin(), cleanInstallDir.end(), IsNotAlnum), cleanInstallDir.end());

    for (const auto& cand : candidates) {
        std::wstring fname = cand.filename().stem().wstring();
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
        fname.erase(std::remove_if(fname.begin(), fname.end(), IsNotAlnum), fname.end());
        
        int score = 0;
        if (fname == cleanInstallDir) score += 50;
        if (fname == cleanName) score += 40;
        if (cleanName.find(fname) != std::wstring::npos || fname.find(cleanName) != std::wstring::npos) score += 20;
        if (cleanInstallDir.find(fname) != std::wstring::npos || fname.find(cleanInstallDir) != std::wstring::npos) score += 15;

        try {
            auto size = fs::file_size(cand);
            if (size > 50 * 1024 * 1024) score += 10;
            else if (size > 10 * 1024 * 1024) score += 5;
        } catch (...) {}

        if (score > bestScore) {
            bestScore = score;
            bestMatch = cand;
        }
    }
    return bestMatch;
}

HICON GetGameIcon(const std::wstring& exePath) {
    HICON hIcon = NULL;
    if (!exePath.empty() && fs::exists(exePath)) {
        SHFILEINFO sfi = {0};
        if (SHGetFileInfo(exePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON)) {
            hIcon = sfi.hIcon;
        }
    }
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    return hIcon;
}

void ScanSteamGames() {
    std::wstring steamPath = GetSteamInstallPath();
    if (steamPath.empty()) {
        steamPath = L"C:\\Program Files (x86)\\Steam";
    }
    for (auto& ch : steamPath) {
        if (ch == L'/') ch = L'\\';
    }

    std::vector<std::wstring> libraries = GetLibraryPaths(steamPath);

    for (const auto& lib : libraries) {
        fs::path steamappsPath = fs::path(lib) / L"steamapps";
        if (!fs::exists(steamappsPath)) continue;

        for (const auto& entry : fs::directory_iterator(steamappsPath)) {
            if (entry.is_regular_file() && entry.path().extension() == L".acf") {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::string line8;
                SteamGame game = { L"", 0, L"", L"", -1, false };
                std::wstring installDir = L"";

                while (std::getline(file, line8)) {
                    std::wstring line = Utf8ToWstring(line8);
                    std::wstring appidStr = ExtractValue(line, L"appid");
                    if (!appidStr.empty()) {
                        try { game.appid = std::stoul(appidStr); } catch(...) {}
                    }
                    std::wstring nameStr = ExtractValue(line, L"name");
                    if (!nameStr.empty()) {
                        game.name = nameStr;
                    }
                    std::wstring installDirStr = ExtractValue(line, L"installdir");
                    if (!installDirStr.empty()) {
                        installDir = installDirStr;
                    }
                }

                if (game.appid != 0 && !game.name.empty() && game.appid != 228980) {
                    fs::path gameDir = fs::path(lib) / L"steamapps" / L"common" / installDir;
                    fs::path exe = FindExecutable(gameDir, game.name, installDir);
                    if (!exe.empty()) {
                        game.exePath = exe.wstring();
                        game.workingDir = gameDir.wstring();
                    }
                    
                    HICON hIcon = GetGameIcon(game.exePath);
                    if (hIcon) {
                        game.imageIndex = ImageList_AddIcon(hImgList, hIcon);
                        DestroyIcon(hIcon); 
                    }
                    allGames.push_back(game);
                }
            }
        }
    }

    std::sort(allGames.begin(), allGames.end(), [](const SteamGame& a, const SteamGame& b) {
        return a.name < b.name;
    });
}

void PopulateListView(HWND hList, const std::wstring& filterText) {
    ListView_DeleteAllItems(hList);
    displayedGames.clear();
    
    std::wstring query = filterText;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    
    int index = 0;
    for (const auto& game : allGames) {
        bool match = false;
        if (query.empty()) {
            match = true;
        } else {
            std::wstring nameLower = game.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(query) != std::wstring::npos) {
                match = true;
            }
        }

        if (match) {
            displayedGames.push_back(game);
            
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.iItem = index;
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<wchar_t*>(game.name.c_str());
            lvi.iImage = game.imageIndex;
            lvi.lParam = index;
            
            ListView_InsertItem(hList, &lvi);
            index++;
        }
    }
}

bool UsesSteamworks(const fs::path& gameDir) {
    try {
        if (!fs::exists(gameDir)) return false;
        for (auto it = fs::recursive_directory_iterator(gameDir); it != fs::recursive_directory_iterator(); ++it) {
            if (it.depth() > 8) { 
                it.disable_recursion_pending();
                continue;
            }
            const auto& entry = *it;
            if (entry.is_regular_file() && entry.path().extension() == L".dll") {
                std::wstring filename = entry.path().filename().wstring();
                std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
                if (filename.find(L"steam_api") != std::wstring::npos) {
                    return true;
                }
            }
        }
    } catch (...) {}
    return false;
}

bool IsSteamRunning() {
    HKEY hKey;
    DWORD activeProcessID = 0;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD bufferSize = sizeof(activeProcessID);
        RegQueryValueEx(hKey, L"ActiveProcessId", NULL, NULL, (LPBYTE)&activeProcessID, &bufferSize);
        RegCloseKey(hKey);
    }
    return (activeProcessID != 0);
}

uint32_t GetRunningAppID() {
    HKEY hKey;
    DWORD appid = 0;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD bufferSize = sizeof(appid);
        RegQueryValueEx(hKey, L"RunningAppID", NULL, NULL, (LPBYTE)&appid, &bufferSize);
        RegCloseKey(hKey);
    }
    return appid;
}

std::unordered_set<std::wstring> GetRunningProcessNames() {
    std::unordered_set<std::wstring> names;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                std::wstring fname = pe.szExeFile;
                std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
                names.insert(fname);
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return names;
}

void KillProcessByName(const std::wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess != NULL) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
}

void LoadToggleStates(bool& overlayEnabled, bool& closeSteamEnabled, bool& killerEnabled, bool& forceSteamEnabled) {
    overlayEnabled = true;       
    closeSteamEnabled = false;  
    killerEnabled = false;
    forceSteamEnabled = false;
    
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\SteamCompact", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD overlayVal = 1, closeVal = 0, killerVal = 0, forceVal = 0;
        DWORD bufferSize = sizeof(DWORD);
        
        if (RegQueryValueEx(hKey, L"OverlayEnabled", NULL, NULL, (LPBYTE)&overlayVal, &bufferSize) == ERROR_SUCCESS) {
            overlayEnabled = (overlayVal != 0);
        }
        bufferSize = sizeof(DWORD);
        if (RegQueryValueEx(hKey, L"CloseSteamEnabled", NULL, NULL, (LPBYTE)&closeVal, &bufferSize) == ERROR_SUCCESS) {
            closeSteamEnabled = (closeVal != 0);
        }
        bufferSize = sizeof(DWORD);
        if (RegQueryValueEx(hKey, L"KillerEnabled", NULL, NULL, (LPBYTE)&killerVal, &bufferSize) == ERROR_SUCCESS) {
            killerEnabled = (killerVal != 0);
        }
        bufferSize = sizeof(DWORD);
        if (RegQueryValueEx(hKey, L"ForceSteamEnabled", NULL, NULL, (LPBYTE)&forceVal, &bufferSize) == ERROR_SUCCESS) {
            forceSteamEnabled = (forceVal != 0);
        }
        
        RegCloseKey(hKey);
    }
}

void SaveToggleStates(bool overlayEnabled, bool closeSteamEnabled, bool killerEnabled, bool forceSteamEnabled) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\SteamCompact", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD overlayVal = overlayEnabled ? 1 : 0;
        DWORD closeVal = closeSteamEnabled ? 1 : 0;
        DWORD killerVal = killerEnabled ? 1 : 0;
        DWORD forceVal = forceSteamEnabled ? 1 : 0;
        
        RegSetValueEx(hKey, L"OverlayEnabled", 0, REG_DWORD, (const BYTE*)&overlayVal, sizeof(DWORD));
        RegSetValueEx(hKey, L"CloseSteamEnabled", 0, REG_DWORD, (const BYTE*)&closeVal, sizeof(DWORD));
        RegSetValueEx(hKey, L"KillerEnabled", 0, REG_DWORD, (const BYTE*)&killerVal, sizeof(DWORD));
        RegSetValueEx(hKey, L"ForceSteamEnabled", 0, REG_DWORD, (const BYTE*)&forceVal, sizeof(DWORD));
        
        RegCloseKey(hKey);
    }
}

void MonitorAndKillLoop(uint32_t targetAppID) {
    bool gameStarted = false;
    for (int i = 0; i < 30; ++i) {
        if (GetRunningAppID() == targetAppID) {
            gameStarted = true;
            break;
        }
        Sleep(1000);
    }
    if (!gameStarted) return;

    Sleep(15000); 

    while (GetRunningAppID() == targetAppID) {
        KillProcessByName(L"steamwebhelper.exe");
        KillProcessByName(L"GameOverlayUI.exe");
        Sleep(1500); 
    }
}

void MonitorAndCloseSteamLoop(uint32_t targetAppID, std::wstring steamExe) {
    bool gameStarted = false;
    for (int i = 0; i < 30; ++i) {
        if (GetRunningAppID() == targetAppID) {
            gameStarted = true;
            break;
        }
        Sleep(1000);
    }
    if (!gameStarted) return;

    while (GetRunningAppID() == targetAppID) {
        Sleep(1000);
    }

    ShellExecute(NULL, L"open", steamExe.c_str(), L"-shutdown", NULL, SW_SHOWNORMAL);
}

void LaunchGame(const SteamGame& game) {
    bool requiresSteam = false;
    bool isForceSteamChecked = (SendMessage(hCheckboxForceSteam, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (isForceSteamChecked) {
        requiresSteam = true;
    } else if (!game.workingDir.empty()) {
        requiresSteam = UsesSteamworks(game.workingDir);
    }

    std::wstring steamPath = GetSteamInstallPath();
    if (steamPath.empty()) {
        steamPath = L"C:\\Program Files (x86)\\Steam";
    }
    std::wstring steamExe = steamPath + L"\\steam.exe";

    bool isOverlayEnabled = (SendMessage(hCheckboxOverlay, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool isCloseSteamChecked = (SendMessage(hCheckboxCloseSteam, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool isKillerEnabled = (SendMessage(hCheckboxKiller, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (!isOverlayEnabled || isKillerEnabled) { 
        SetEnvironmentVariable(L"STEAM_DISABLE_OVERLAY", L"1");
        SetEnvironmentVariable(L"STEAM_NO_OVERLAY", L"1");
    } else {
        SetEnvironmentVariable(L"STEAM_DISABLE_OVERLAY", NULL);
        SetEnvironmentVariable(L"STEAM_NO_OVERLAY", NULL);
    }

    if (requiresSteam) {
        if (isOverlayEnabled || isKillerEnabled) {
            std::wstring params = 
                L"-silent -dev -console -nofriendsui -no-dwrite -nointro -nobigpicture -nofasthtml "
                L"-nocrashmonitor -noshaders -no-shared-textures -disablehighdpi -cef-single-process "
                L"-cef-in-process-gpu -single_core -cef-disable-d3d11 -cef-disable-sandbox -disable-winh264 "
                L"-cef-force-32bit -no-cef-sandbox -vrdisable -cef-disable-breakpad -applaunch " + std::to_wstring(game.appid);
            
            ShellExecute(NULL, L"open", steamExe.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);
        } else {
            if (!IsSteamRunning()) {
                std::wstring params = 
                    L"-silent -dev -console -nofriendsui -no-dwrite -nointro -nobigpicture -nofasthtml "
                    L"-nocrashmonitor -noshaders -no-shared-textures -disablehighdpi -cef-single-process "
                    L"-cef-in-process-gpu -single_core -cef-disable-d3d11 -cef-disable-sandbox -disable-winh264 "
                    L"-cef-force-32bit -no-cef-sandbox -vrdisable -cef-disable-breakpad";
                ShellExecute(NULL, L"open", steamExe.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);
                Sleep(2500); 
            }

            fs::path appidFilePath = fs::path(game.workingDir) / L"steam_appid.txt";
            if (!fs::exists(appidFilePath)) {
                std::ofstream appidFile(appidFilePath);
                if (appidFile.is_open()) {
                    appidFile << game.appid;
                    appidFile.close();
                }
            }

            if (!game.exePath.empty()) {
                ShellExecute(NULL, L"open", game.exePath.c_str(), NULL, game.workingDir.c_str(), SW_SHOWNORMAL);
            } else {
                std::wstring params = L"-silent -applaunch " + std::to_wstring(game.appid);
                ShellExecute(NULL, L"open", steamExe.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);
            }
        }

        if (isKillerEnabled) {
            std::thread t(MonitorAndKillLoop, game.appid);
            t.detach();
        }
        if (isCloseSteamChecked) {
            std::thread t(MonitorAndCloseSteamLoop, game.appid, steamExe);
            t.detach();
        }
    } else {
        if (!game.exePath.empty()) {
            fs::path appidFilePath = fs::path(game.workingDir) / L"steam_appid.txt";
            if (!fs::exists(appidFilePath)) {
                std::ofstream appidFile(appidFilePath);
                if (appidFile.is_open()) {
                    appidFile << game.appid;
                    appidFile.close();
                }
            }
            ShellExecute(NULL, L"open", game.exePath.c_str(), NULL, game.workingDir.c_str(), SW_SHOWNORMAL);
        } else {
            std::wstring params = 
                L"-silent -dev -console -nofriendsui -no-dwrite -nointro -nobigpicture -nofasthtml "
                L"-nocrashmonitor -noshaders -no-shared-textures -disablehighdpi -cef-single-process "
                L"-cef-in-process-gpu -single_core -cef-disable-d3d11 -cef-disable-sandbox -disable-winh264 "
                L"-cef-force-32bit -no-cef-sandbox -vrdisable -cef-disable-breakpad -applaunch " + std::to_wstring(game.appid);
            
            ShellExecute(NULL, L"open", steamExe.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);

            if (isCloseSteamChecked) {
                std::thread t(MonitorAndCloseSteamLoop, game.appid, steamExe);
                t.detach();
            }
        }
    }
}

void UpdatePlayButtonText(HWND hWnd) {
    LocalizedStrings strings = GetStrings();
    int idx = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    if (idx != -1 && idx < (int)displayedGames.size()) {
        if (displayedGames[idx].isRunning) {
            SetWindowText(hButtonPlay, strings.btnStop.c_str());
        } else {
            SetWindowText(hButtonPlay, strings.btnPlay.c_str());
        }
    } else {
        SetWindowText(hButtonPlay, strings.btnPlay.c_str());
    }
    InvalidateRect(hButtonPlay, NULL, TRUE);
}

void UpdateControlStates(HWND hWnd) {
    bool anyGameRunning = false;
    for (const auto& game : allGames) {
        if (game.isRunning) {
            anyGameRunning = true;
            break;
        }
    }

    if (anyGameRunning) {
        EnableWindow(hCheckboxOverlay, FALSE);
        EnableWindow(hCheckboxCloseSteam, FALSE);
        EnableWindow(hCheckboxKiller, FALSE);
        EnableWindow(hCheckboxForceSteam, FALSE);
    } else {
        bool killerChecked = (SendMessage(hCheckboxKiller, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool overlayChecked = (SendMessage(hCheckboxOverlay, BM_GETCHECK, 0, 0) == BST_CHECKED);

        EnableWindow(hCheckboxOverlay, !killerChecked); 
        EnableWindow(hCheckboxCloseSteam, TRUE);
        EnableWindow(hCheckboxKiller, !overlayChecked);
        EnableWindow(hCheckboxForceSteam, TRUE);
    }

    InvalidateRect(hLabelOverlay, NULL, TRUE);
    InvalidateRect(hLabelCloseSteam, NULL, TRUE);
    InvalidateRect(hLabelKiller, NULL, TRUE);
    InvalidateRect(hLabelForceSteam, NULL, TRUE);
    InvalidateRect(hCheckboxOverlay, NULL, TRUE);
    InvalidateRect(hCheckboxCloseSteam, NULL, TRUE);
    InvalidateRect(hCheckboxKiller, NULL, TRUE);
    InvalidateRect(hCheckboxForceSteam, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hWnd, 20, &useDarkMode, sizeof(useDarkMode));
            DwmSetWindowAttribute(hWnd, 19, &useDarkMode, sizeof(useDarkMode));

            hBgBrush = CreateSolidBrush(RGB(20, 20, 20));     
            hEditBrush = CreateSolidBrush(RGB(35, 35, 35));   

            LocalizedStrings strings = GetStrings();

            std::wstring username = GetSteamActiveUser();
            std::wstring labelText = strings.activeUser + username;

            hStaticAccount = CreateWindowEx(0, L"STATIC", labelText.c_str(), 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                10, 10, 300, 20, hWnd, (HMENU)IDC_ACCOUNT_STATIC, NULL, NULL);

            hEditSearch = CreateWindowEx(0, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 
                10, 35, 300, 24, hWnd, (HMENU)IDC_SEARCH_EDIT, NULL, NULL);

            hListView = CreateWindowEx(0, WC_LISTVIEW, NULL, 
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | WS_TABSTOP, 
                10, 65, 300, 310, hWnd, (HMENU)IDC_GAME_LISTBOX, NULL, NULL);

            ListView_SetExtendedListViewStyle(hListView, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
            ListView_SetBkColor(hListView, RGB(28, 28, 28));
            ListView_SetTextColor(hListView, RGB(230, 230, 230));
            ListView_SetTextBkColor(hListView, RGB(28, 28, 28));

            LVCOLUMN lvc = {0};
            lvc.mask = LVCF_WIDTH | LVCF_TEXT;
            lvc.cx = 280;
            lvc.pszText = const_cast<wchar_t*>(L"Game");
            ListView_InsertColumn(hListView, 0, &lvc);

            hCheckboxForceSteam = CreateWindowEx(0, L"BUTTON", L"", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 
                10, 351, 20, 24, hWnd, (HMENU)IDC_FORCE_STEAM_CHECKBOX, NULL, NULL);

            hLabelForceSteam = CreateWindowEx(0, L"STATIC", strings.forceSteamCheckbox.c_str(), 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                35, 351, 280, 24, hWnd, (HMENU)IDC_FORCE_STEAM_STATIC, NULL, NULL);

            hCheckboxOverlay = CreateWindowEx(0, L"BUTTON", L"", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 
                10, 383, 20, 24, hWnd, (HMENU)IDC_OVERLAY_CHECKBOX, NULL, NULL);

            hLabelOverlay = CreateWindowEx(0, L"STATIC", strings.overlayCheckbox.c_str(), 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                35, 383, 280, 24, hWnd, (HMENU)IDC_OVERLAY_STATIC, NULL, NULL);

            hCheckboxCloseSteam = CreateWindowEx(0, L"BUTTON", L"", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 
                10, 415, 20, 24, hWnd, (HMENU)IDC_CLOSE_STEAM_CHECKBOX, NULL, NULL);

            hLabelCloseSteam = CreateWindowEx(0, L"STATIC", strings.closeSteamCheckbox.c_str(), 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                35, 415, 280, 24, hWnd, (HMENU)IDC_CLOSE_STEAM_STATIC, NULL, NULL);

            hCheckboxKiller = CreateWindowEx(0, L"BUTTON", L"", 
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 
                10, 445, 20, 24, hWnd, (HMENU)IDC_KILLER_CHECKBOX, NULL, NULL);

            hLabelKiller = CreateWindowEx(0, L"STATIC", strings.killerCheckbox.c_str(), 
                WS_CHILD | WS_VISIBLE | SS_LEFT, 
                35, 445, 280, 24, hWnd, (HMENU)IDC_KILLER_STATIC, NULL, NULL);

            hButtonPlay = CreateWindowEx(0, L"BUTTON", strings.btnPlay.c_str(), 
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 
                10, 475, 300, 32, hWnd, (HMENU)IDC_PLAY_BUTTON, NULL, NULL);
 
            bool overlayDefault = true, closeSteamDefault = false, killerDefault = false, forceSteamDefault = false;
            LoadToggleStates(overlayDefault, closeSteamDefault, killerDefault, forceSteamDefault);

            SendMessage(hCheckboxOverlay, BM_SETCHECK, overlayDefault ? BST_CHECKED : BST_UNCHECKED, 0); 
            SendMessage(hCheckboxCloseSteam, BM_SETCHECK, closeSteamDefault ? BST_CHECKED : BST_UNCHECKED, 0); 
            SendMessage(hCheckboxKiller, BM_SETCHECK, killerDefault ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessage(hCheckboxForceSteam, BM_SETCHECK, forceSteamDefault ? BST_CHECKED : BST_UNCHECKED, 0);

            hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            SendMessage(hStaticAccount, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditSearch, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hListView, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hCheckboxOverlay, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLabelOverlay, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hCheckboxCloseSteam, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLabelCloseSteam, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hCheckboxKiller, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLabelKiller, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hCheckboxForceSteam, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLabelForceSteam, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hButtonPlay, WM_SETFONT, (WPARAM)hFont, TRUE);

            SendMessage(hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search games...");

            hImgList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 100);
            ListView_SetImageList(hListView, hImgList, LVSIL_SMALL);

            ScanSteamGames();
            PopulateListView(hListView, L"");

            UpdateControlStates(hWnd); 

            SetTimer(hWnd, 1, 1000, NULL);
            break;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                auto runningProcesses = GetRunningProcessNames();
                bool anyGameRunning = false;

                for (auto& game : allGames) {
                    if (!game.exePath.empty()) {
                        fs::path p(game.exePath);
                        std::wstring exeName = p.filename().wstring();
                        std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::tolower);
                        game.isRunning = (runningProcesses.count(exeName) > 0);
                    } else {
                        game.isRunning = false;
                    }
                    if (game.isRunning) anyGameRunning = true;
                }

                for (auto& game : displayedGames) {
                    if (!game.exePath.empty()) {
                        fs::path p(game.exePath);
                        std::wstring exeName = p.filename().wstring();
                        std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::tolower);
                        game.isRunning = (runningProcesses.count(exeName) > 0);
                    } else {
                        game.isRunning = false;
                    }
                }

                UpdateControlStates(hWnd);

                InvalidateRect(hListView, NULL, FALSE);
                UpdatePlayButtonText(hWnd);
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;

            if (hwndStatic == hLabelOverlay) {
                BOOL overlayEnabled = IsWindowEnabled(hCheckboxOverlay);
                SetTextColor(hdcStatic, overlayEnabled ? RGB(230, 230, 230) : RGB(100, 100, 100));
            } else if (hwndStatic == hLabelCloseSteam) {
                BOOL closeEnabled = IsWindowEnabled(hCheckboxCloseSteam);
                SetTextColor(hdcStatic, closeEnabled ? RGB(230, 230, 230) : RGB(100, 100, 100));
            } else if (hwndStatic == hLabelKiller) {
                BOOL killerEnabled = IsWindowEnabled(hCheckboxKiller);
                SetTextColor(hdcStatic, killerEnabled ? RGB(230, 230, 230) : RGB(100, 100, 100));
            } else if (hwndStatic == hLabelForceSteam) {
                BOOL forceEnabled = IsWindowEnabled(hCheckboxForceSteam);
                SetTextColor(hdcStatic, forceEnabled ? RGB(230, 230, 230) : RGB(100, 100, 100));
            } else if (hwndStatic == hCheckboxOverlay || hwndStatic == hCheckboxCloseSteam || hwndStatic == hCheckboxKiller || hwndStatic == hCheckboxForceSteam) {
                SetTextColor(hdcStatic, RGB(230, 230, 230));
            } else {
                SetTextColor(hdcStatic, RGB(150, 150, 150)); 
            }
            SetBkColor(hdcStatic, RGB(20, 20, 20));
            return (INT_PTR)hBgBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(230, 230, 230));
            SetBkColor(hdcEdit, RGB(35, 35, 35));
            return (INT_PTR)hEditBrush;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == IDC_PLAY_BUTTON) {
                wchar_t btnText[64];
                GetWindowText(pDIS->hwndItem, btnText, 64);
                
                bool isStop = (_wcsicmp(btnText, L"Stop") == 0 || _wcsicmp(btnText, L"Остановить") == 0);

                COLORREF bgNormal = isStop ? RGB(180, 40, 40) : RGB(45, 45, 45); 
                COLORREF bgSelected = isStop ? RGB(210, 60, 60) : RGB(60, 60, 60);

                HBRUSH hBtnBrush = CreateSolidBrush(pDIS->itemState & ODS_SELECTED ? bgSelected : bgNormal);
                FillRect(pDIS->hDC, &pDIS->rcItem, hBtnBrush);
                DeleteObject(hBtnBrush);
                
                HPEN hPen = CreatePen(PS_SOLID, 1, isStop ? RGB(230, 70, 70) : RGB(70, 70, 70));
                HPEN hOldPen = (HPEN)SelectObject(pDIS->hDC, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(pDIS->hDC, GetStockObject(NULL_BRUSH));
                
                Rectangle(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, pDIS->rcItem.right, pDIS->rcItem.bottom);
                
                SelectObject(pDIS->hDC, hOldPen);
                SelectObject(pDIS->hDC, hOldBrush);
                DeleteObject(hPen);

                SetTextColor(pDIS->hDC, RGB(230, 230, 230));
                SetBkMode(pDIS->hDC, TRANSPARENT);
                
                DrawText(pDIS->hDC, btnText, -1, &pDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_SEARCH_EDIT && wmEvent == EN_CHANGE) {
                wchar_t filter[256];
                GetWindowText(hEditSearch, filter, 256);
                PopulateListView(hListView, filter);
                UpdatePlayButtonText(hWnd);
            }
            else if (wmId == IDC_OVERLAY_CHECKBOX || wmId == IDC_CLOSE_STEAM_CHECKBOX || wmId == IDC_KILLER_CHECKBOX || wmId == IDC_FORCE_STEAM_CHECKBOX) {
                bool overlay = (SendMessage(hCheckboxOverlay, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool closeSteam = (SendMessage(hCheckboxCloseSteam, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool killer = (SendMessage(hCheckboxKiller, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool forceSteam = (SendMessage(hCheckboxForceSteam, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveToggleStates(overlay, closeSteam, killer, forceSteam);
                
                UpdateControlStates(hWnd);
            }
            else if (wmId == IDC_PLAY_BUTTON) {
                int idx = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
                if (idx != -1 && idx < (int)displayedGames.size()) {
                    if (displayedGames[idx].isRunning) {
                        if (!displayedGames[idx].exePath.empty()) {
                            fs::path p(displayedGames[idx].exePath);
                            std::wstring exeName = p.filename().wstring();
                            if (!exeName.empty()) {
                                KillProcessByName(exeName);
                                SendMessage(hWnd, WM_TIMER, 1, 0); 
                            }
                        }
                    } else {
                        LaunchGame(displayedGames[idx]);
                    }
                } else {
                    LocalizedStrings strings = GetStrings();
                    MessageBox(hWnd, strings.warningMessage.c_str(), strings.windowTitle.c_str(), MB_OK | MB_ICONINFORMATION);
                }
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            if (pnmhdr->idFrom == IDC_GAME_LISTBOX) {
                if (pnmhdr->code == NM_DBLCLK) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    int idx = pnmv->iItem;
                    if (idx != -1 && idx < (int)displayedGames.size()) {
                        if (!displayedGames[idx].isRunning) {
                            LaunchGame(displayedGames[idx]);
                        }
                    }
                }
                else if (pnmhdr->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (pnmv->uChanged & LVIF_STATE) {
                        UpdatePlayButtonText(hWnd);
                    }
                }
                else if (pnmhdr->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW pLVCD = (LPNMLVCUSTOMDRAW)lParam;
                    switch (pLVCD->nmcd.dwDrawStage) {
                        case CDDS_PREPAINT:
                            return CDRF_NOTIFYITEMDRAW;
                        case CDDS_ITEMPREPAINT: {
                            int itemIndex = (int)pLVCD->nmcd.lItemlParam;
                            if (itemIndex >= 0 && itemIndex < (int)displayedGames.size()) {
                                if (displayedGames[itemIndex].isRunning) {
                                    pLVCD->clrText = RGB(50, 150, 255); 
                                } else {
                                    pLVCD->clrText = RGB(230, 230, 230); 
                                }
                            }
                            return CDRF_DODEFAULT;
                        }
                    }
                }
            }
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            MoveWindow(hStaticAccount, 10, 10, width - 20, 20, TRUE);
            MoveWindow(hEditSearch, 10, 35, width - 20, 24, TRUE);
            
            MoveWindow(hListView, 10, 65, width - 20, height - 237, TRUE);
            MoveWindow(hCheckboxForceSteam, 10, height - 166, 20, 24, TRUE);
            MoveWindow(hLabelForceSteam, 35, height - 163, width - 45, 24, TRUE);
            MoveWindow(hCheckboxOverlay, 10, height - 136, 20, 24, TRUE);
            MoveWindow(hLabelOverlay, 35, height - 133, width - 45, 24, TRUE);
            MoveWindow(hCheckboxCloseSteam, 10, height - 106, 20, 24, TRUE);
            MoveWindow(hLabelCloseSteam, 35, height - 103, width - 45, 24, TRUE);
            MoveWindow(hCheckboxKiller, 10, height - 76, 20, 24, TRUE);
            MoveWindow(hLabelKiller, 35, height - 73, width - 45, 24, TRUE);
            MoveWindow(hButtonPlay, 10, height - 42, width - 20, 32, TRUE);
            
            ListView_SetColumnWidth(hListView, 0, width - 40);
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 240;
            mmi->ptMinTrackSize.y = 300;
            break;
        }

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            if (hFont) DeleteObject(hFont);
            if (hBgBrush) DeleteObject(hBgBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            if (hImgList) ImageList_Destroy(hImgList);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wcex.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wcex.hbrBackground = CreateSolidBrush(RGB(20, 20, 20)); 
    wcex.lpszClassName = L"SteamCompactLauncherClass";

    if (!RegisterClassEx(&wcex)) {
        return 1;
    }

    HWND hWnd = CreateWindowW(L"SteamCompactLauncherClass", GetStrings().windowTitle.c_str(), 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 345, 625, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}