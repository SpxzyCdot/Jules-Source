#pragma once
#include "Bridge.hpp"
#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>

std::atomic<bool> Injected = false;
std::set<DWORD> monitoredPids;
std::mutex pidMutex;
bool ProcessExists(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess == NULL)
        return false;

    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);

    return exitCode == STILL_ACTIVE;
}

const std::string DECOY_MARKER = "Hello CraveX skiddies :3";
const unsigned char XOR_KEY[] = { 0x4A, 0x75, 0x6C, 0x65, 0x73, 0x45, 0x78, 0x65 };

std::string XorDecrypt(const std::string& data) {
    std::string result = data;
    size_t keyLen = sizeof(XOR_KEY);
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= XOR_KEY[i % keyLen];
    }
    return result;
}

std::string GetLuaCode(DWORD pid, int idx) {
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetLuaCode, &hModule);

    HRSRC resourceHandle = FindResourceW(hModule, MAKEINTRESOURCEW(idx), RT_RCDATA);
    if (resourceHandle == NULL)
    {
        return "";
    }

    HGLOBAL loadedResource = LoadResource(hModule, resourceHandle);
    if (loadedResource == NULL)
    {
        return "";
    }

    DWORD size = SizeofResource(hModule, resourceHandle);
    void* data = LockResource(loadedResource);

    std::string code = std::string(static_cast<char*>(data), size);
    
    if (code.find(DECOY_MARKER) != std::string::npos) {
        HRSRC encResource = FindResourceW(hModule, MAKEINTRESOURCEW(idx + 100), RT_RCDATA);
        if (encResource != NULL) {
            HGLOBAL encLoaded = LoadResource(hModule, encResource);
            if (encLoaded != NULL) {
                DWORD encSize = SizeofResource(hModule, encResource);
                void* encData = LockResource(encLoaded);
                std::string encrypted = std::string(static_cast<char*>(encData), encSize);
                code = XorDecrypt(encrypted);
            }
        }
    }
    
    size_t pos = code.find("%-PROCESS-ID-%");
    if (pos != std::string::npos) {
        code.replace(pos, 14, std::to_string(pid));
    }

    return code;
}

void MonitorProcess(DWORD pid) {
    uintptr_t base = Process::GetModuleBase(pid);
    if (base == 0) return;

    bool notified = false;

    while (ProcessExists(pid)) {

        Instance Datamodel(0, pid);
        while (ProcessExists(pid)) {
            try {
                Datamodel = FetchDatamodel(base, pid);
                if (Datamodel.GetAddress() != 0 && Datamodel.Name() == "Ugc")
                    break;
            }
            catch (...) {

            }
            
            Sleep(250);
        }
        
        if (!notified) {
            std::thread([]() {
                Beep(500, 500);
            }).detach();
            notified = true;
        }

        if (!ProcessExists(pid)) {
            std::lock_guard<std::mutex> lock(pidMutex);
            monitoredPids.erase(pid);

            if (monitoredPids.empty()) {
                Injected = false;
            }

            return;
        }




        Sleep(2000);

        try {
            Instance CoreGui = Datamodel.FindFirstChild("CoreGui");
            if (CoreGui.GetAddress() == 0) {

                continue;
            }


            Instance RobloxGui = CoreGui.FindFirstChild("RobloxGui");
            if (RobloxGui.GetAddress() == 0) {

                Sleep(1000);
                continue;
            }


            Instance ExistingExecutor = CoreGui.FindFirstChild("ExternalExecutor");
            bool alreadyInjected = (ExistingExecutor.GetAddress() != 0);

            if (!alreadyInjected) {


                size_t inits;
                std::string initLua = GetLuaCode(pid, 1);
                std::vector<char> initb = Bytecode::Sign(Bytecode::Compile(initLua), inits);

                Instance RobloxGui = CoreGui.FindFirstChild("RobloxGui");
                if (RobloxGui.GetAddress() == 0) {
                    continue;
                }

                Instance Modules = RobloxGui.FindFirstChild("Modules");
                if (Modules.GetAddress() == 0) {
                    continue;
                }


                Instance PlayerList = Modules.FindFirstChild("PlayerList");
                Instance PlmModule = PlayerList.FindFirstChild("PlayerListManager");

                bool injectionSuccess = false;

                if (PlmModule.GetAddress() != 0) {
                    Instance CorePackages = Datamodel.FindFirstChild("CorePackages");
                    if (CorePackages.GetAddress() == 0) continue;

                    Instance Packages = CorePackages.FindFirstChild("Packages");
                    if (Packages.GetAddress() == 0) continue;

                    Instance Index = Packages.FindFirstChild("_Index");
                    if (Index.GetAddress() == 0) continue;

                    Instance Cm2D1 = Index.FindFirstChild("CollisionMatchers2D");
                    if (Cm2D1.GetAddress() == 0) continue;

                    Instance Cm2D2 = Cm2D1.FindFirstChild("CollisionMatchers2D");
                    if (Cm2D2.GetAddress() == 0) continue;

                    Instance Jest = Cm2D2.FindFirstChild("Jest");
                    if (Jest.GetAddress() == 0) continue;

                    WriteMemory<BYTE>(base + Offsets::EnableLoadModule, 1, pid);
                    WriteMemory<uintptr_t>(PlmModule.GetAddress() + 0x8, Jest.GetAddress(), pid);

                    auto revert1 = Jest.SetScriptBytecode(initb, inits);
                    HWND hwnd = Process::GetWindowsProcess(pid);
                    HWND old = GetForegroundWindow();

                    int attempts = 0;
                    while (GetForegroundWindow() != hwnd && attempts < 20) {
                        SetForegroundWindow(hwnd);
                        Sleep(50);
                        attempts++;
                    }


                    keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE, 0);
                    Sleep(10);
                    keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);




                    Sleep(500);


                    keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE, 0);
                    Sleep(10);
                    keybd_event(VK_ESCAPE, MapVirtualKey(VK_ESCAPE, 0), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);

                    CoreGui.WaitForChild("ExternalExecutor");

                    SetForegroundWindow(old);
                    WriteMemory<uintptr_t>(PlmModule.GetAddress() + 0x8, PlmModule.GetAddress(), pid);
                    revert1();
                    injectionSuccess = true;
                }
                else {

                    Instance InitModule = Modules.FindFirstChild("AvatarEditorPrompts");
                    if (InitModule.GetAddress() != 0) {
                        WriteMemory<BYTE>(base + Offsets::EnableLoadModule, 1, pid);
                        auto revert1 = InitModule.SetScriptBytecode(initb, inits);
                        CoreGui.WaitForChild("ExternalExecutor");
                        revert1();
                        injectionSuccess = true;
                    }
                }

                if (!injectionSuccess) {

                    continue;
                }

            }



        }
        catch (...) {

            Sleep(1000);
            continue;
        }


        uintptr_t injectedDataModelAddress = Datamodel.GetAddress();
        int consecutiveErrors = 0;
        const int errorThreshold = 5;

        while (ProcessExists(pid)) {
            try {
                Instance CurrentDataModel = FetchDatamodel(base, pid);


                if (CurrentDataModel.GetAddress() != 0 &&
                    CurrentDataModel.GetAddress() != injectedDataModelAddress) {

                    break;
                }


                if (CurrentDataModel.GetAddress() == 0 || CurrentDataModel.Name() != "Ugc") {
                    consecutiveErrors++;
                    if (consecutiveErrors >= errorThreshold) {

                        break;
                    }
                }
                else {

                    consecutiveErrors = 0;
                }

            }
            catch (...) {

                consecutiveErrors++;
                if (consecutiveErrors >= errorThreshold) {

                    break;
                }
            }

            Sleep(500);
        }
    }
    
    std::lock_guard<std::mutex> lock(pidMutex);
    monitoredPids.erase(pid);

    if (monitoredPids.empty()) {
        Injected = false;
    }

}

void ManagerThread() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(pidMutex);
            for (auto it = monitoredPids.begin(); it != monitoredPids.end(); ) {
                if (!ProcessExists(*it)) {
                    it = monitoredPids.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (monitoredPids.empty()) {
                Injected = false;
            }
            else {
                Injected = true;
            }
        }
        Sleep(100);
    }
}

void StartExecutorSystem() {
    static bool started = false;
    if (started) return;
    started = true;

    std::thread(StartBridge).detach();
    std::thread(ManagerThread).detach();
}

std::string Convert(const wchar_t* wideStr) {
    if (!wideStr) return "";
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr
    );
    if (size_needed == 0) return "";
    std::string result(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wideStr, -1, &result[0], size_needed, nullptr, nullptr
    );
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

extern "C" __declspec(dllexport) void Inject() {
    StartExecutorSystem();

    std::vector<DWORD> pids = Process::GetProcessID();
    
    std::lock_guard<std::mutex> lock(pidMutex);
    for (DWORD pid : pids) {
        if (monitoredPids.find(pid) == monitoredPids.end()) {
            monitoredPids.insert(pid);
            std::thread(MonitorProcess, pid).detach();
        }
    }
    
    if (!monitoredPids.empty()) {
        Injected = true;
    }
}

extern "C" __declspec(dllexport) void Execute(const wchar_t* input) {
    std::string source = Convert(input);
    if (source.length() >= 1) {
        Execute(source);
    }
}

extern "C" __declspec(dllexport) bool IsInjected() {
    return Injected.load();
}

extern "C" __declspec(dllexport) void GetOutput(char* buffer, int size) {
    std::lock_guard<std::mutex> lock(outputMutex);
    if (outputQueue.empty()) {
        if (size > 0) buffer[0] = '\0';
        return;
    }
    std::string msg = outputQueue.front();
    outputQueue.pop();

    strncpy_s(buffer, size, msg.c_str(), _TRUNCATE);
}
