#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "Dependecies/server/httplib.h"
using namespace httplib;

#include "Dependecies/server/nlohmann/json.hpp"
using json = nlohmann::json;

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>
#include <regex> 
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>

#include <ixwebsocket/IXWebSocket.h>


#ifdef _WIN32
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#endif

#include <ShlObj_core.h>

#include "Utils/Process.hpp"
#include "Utils/Instance.hpp"
#include "Utils/Bytecode.hpp"

inline std::string script = "";
inline uintptr_t order = 0;
inline std::unordered_map<DWORD, uintptr_t> orders;
inline std::unordered_set<std::string> invalidatedInstances;
inline std::unordered_map<DWORD, std::vector<std::string>> teleportQueues;
inline bool consoleClosable = false;
inline bool consoleHandlerSet = false;
inline std::queue<std::string> outputQueue;
inline std::mutex outputMutex;

inline std::unordered_map<std::string, uintptr_t> fflagOffsets;
inline std::string cachedApiVersion = "";
inline bool fflagsLoaded = false;
inline std::mutex fflagMutex;
inline std::unordered_set<std::string> fflagBlacklist = { "EnableLoadModule" };

inline std::string GetRobloxVersionFromPid(DWORD pid) {
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (hSnap == INVALID_HANDLE_VALUE) return "";

	MODULEENTRY32W modEntry;
	modEntry.dwSize = sizeof(modEntry);
	std::string versionStr = "";

	if (Module32FirstW(hSnap, &modEntry)) {
		do {
			if (_wcsicmp(modEntry.szModule, L"RobloxPlayerBeta.exe") == 0) {
				std::wstring exePath(modEntry.szExePath);

				size_t versionPos = exePath.find(L"version-");
				if (versionPos != std::wstring::npos) {
					size_t endPos = exePath.find(L'\\', versionPos);
					if (endPos == std::wstring::npos) endPos = exePath.find(L'/', versionPos);
					if (endPos == std::wstring::npos) endPos = exePath.length();

					std::wstring versionWStr = exePath.substr(versionPos, endPos - versionPos);
					versionStr = std::string(versionWStr.begin(), versionWStr.end());
				}
				break;
			}
		} while (Module32NextW(hSnap, &modEntry));
	}
	CloseHandle(hSnap);
	return versionStr;
}

inline std::string FetchAPIVersion() {
	Client client("offsets.ntgetwritewatch.workers.dev");
	client.set_follow_location(true);
	auto res = client.Get("/version");
	if (res && res->status == 200) {
		std::string body = res->body;
		while (!body.empty() && (body.back() == '\n' || body.back() == '\r' || body.back() == ' ')) {
			body.pop_back();
		}
		return body;
	}
	return "";
}

inline void FetchFFlags() {
	std::lock_guard<std::mutex> lock(fflagMutex);
	if (fflagsLoaded) return;

	Client client("offsets.ntgetwritewatch.workers.dev");
	client.set_follow_location(true);
	auto res = client.Get("/FFlags.hpp");
	if (res && res->status == 200) {
		std::string body = res->body;
		std::regex pattern(R"(uintptr_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+);)");
		std::smatch match;
		std::string::const_iterator searchStart(body.cbegin());
		while (std::regex_search(searchStart, body.cend(), match, pattern)) {
			std::string name = match[1];
			std::string offsetStr = match[2];
			uintptr_t offset = std::stoull(offsetStr, nullptr, 16);
			if (fflagBlacklist.find(name) == fflagBlacklist.end()) {
				fflagOffsets[name] = offset;
			}
			searchStart = match.suffix().first;
		}
		cachedApiVersion = FetchAPIVersion();
		fflagsLoaded = true;
	}
}

inline bool CheckVersionMatch(DWORD pid) {
	if (cachedApiVersion.empty()) {
		cachedApiVersion = FetchAPIVersion();
	}
	std::string robloxVersion = GetRobloxVersionFromPid(pid);
	return !robloxVersion.empty() && !cachedApiVersion.empty() && robloxVersion == cachedApiVersion;
}

namespace fs = std::filesystem;

inline fs::path WorkspaceRoot() {
	static fs::path workspace = []() {
		HMODULE hModule = nullptr;
		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&WorkspaceRoot),
			&hModule
		);

		wchar_t modulePath[MAX_PATH]{};
		DWORD len = GetModuleFileNameW(hModule, modulePath, MAX_PATH);
		fs::path mod(modulePath, modulePath + (len ? len : 0));

		fs::path baseDir = fs::current_path();
		if (!mod.empty()) {
			fs::path moduleDir = mod.parent_path();
			if (!moduleDir.empty() && !moduleDir.parent_path().empty()) {
				baseDir = moduleDir.parent_path();
			}
			else if (!moduleDir.empty()) {
				baseDir = moduleDir;
			}
		}

		fs::path workspaceDir = baseDir / "workspace";
		std::error_code ec;
		fs::create_directories(workspaceDir, ec);
		return workspaceDir;
		}();
	return workspace;
}

inline std::vector<std::string> SplitLines(const std::string& str) {
	std::stringstream ss(str);
	std::string line;
	std::vector<std::string> lines;
	while (std::getline(ss, line, '\n'))
		lines.push_back(line);
	return lines;
}

inline fs::path ResolvePath(const std::string& rawPath) {
	fs::path rel(rawPath);
	if (rel.is_absolute()) {
		rel = rel.lexically_relative(rel.root_path());
		if (rel.empty()) {
			rel = rel.filename();
		}
	}
	rel = rel.lexically_normal();

	auto base = WorkspaceRoot();
	fs::path combined = (base / rel).lexically_normal();

	std::error_code ec;
	auto normalized = fs::weakly_canonical(combined, ec);
	if (ec) {
		normalized = combined;
	}
	const auto root = WorkspaceRoot();
	auto normalizedRoot = root.lexically_normal();
	if (normalizedRoot.empty() || normalizedRoot == "/") {
		return normalized;
	}
	auto normalizedStr = normalized.lexically_normal().wstring();
	auto rootStr = normalizedRoot.wstring();
	bool inRoot = normalizedStr.size() >= rootStr.size() &&
		normalizedStr.compare(0, rootStr.size(), rootStr) == 0 &&
		(normalizedStr.size() == rootStr.size() || normalizedStr[rootStr.size()] == L'\\' || normalizedStr[rootStr.size()] == L'/');
	if (!inRoot) {
		normalized = root / rel.filename();
	}
	return normalized;
}

inline bool SendMouseInput(DWORD flags, LONG dx = 0, LONG dy = 0, DWORD mouseData = 0, bool absolute = false) {
	INPUT input{};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;
	input.mi.dx = dx;
	input.mi.dy = dy;
	input.mi.mouseData = mouseData;
	if (absolute) {
		input.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
	}
	UINT sent = SendInput(1, &input, sizeof(INPUT));
	return sent == 1;
}

inline bool SendMouseClick(DWORD downFlag, DWORD upFlag) {
	return SendMouseInput(downFlag) && SendMouseInput(upFlag);
}

inline void DisableConsoleClose() {
	HWND hwnd = GetConsoleWindow();
	if (!hwnd) return;
	HMENU hMenu = GetSystemMenu(hwnd, FALSE);
	if (hMenu) {
		DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
	}
}

inline void EnableConsoleClose() {
	HWND hwnd = GetConsoleWindow();
	if (!hwnd) return;

	GetSystemMenu(hwnd, TRUE);
	GetSystemMenu(hwnd, FALSE);
}

inline BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
	if (ctrlType == CTRL_CLOSE_EVENT) {
		if (consoleClosable) {
			FreeConsole();
		}

		return TRUE;
	}
	return FALSE;
}

inline bool EnsureConsole() {
	static bool initialized = false;
	if (GetConsoleWindow() && initialized)
		return true;

	if (!GetConsoleWindow()) {
		if (!AllocConsole())
			return false;
	}

	FILE* dummy;
	freopen_s(&dummy, "CONOUT$", "w", stdout);
	freopen_s(&dummy, "CONOUT$", "w", stderr);
	freopen_s(&dummy, "CONIN$", "r", stdin);

	SetConsoleOutputCP(CP_UTF8);
	initialized = true;
	consoleClosable = false;
	DisableConsoleClose();
	if (!consoleHandlerSet) {
		SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
		consoleHandlerSet = true;
	}
	return true;
}

inline WORD DefaultConsoleAttributes() {
	static WORD attrs = []() -> WORD {
		EnsureConsole();
		CONSOLE_SCREEN_BUFFER_INFO info{};
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
			return info.wAttributes;
		}
		return static_cast<WORD>(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		}();
	return attrs;
}

inline void SetConsoleColor(WORD color) {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

inline std::wstring ToWide(const std::string& utf8) {
	if (utf8.empty()) return L"";
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring wide(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), size_needed);
	return wide;
}

inline std::string ConsoleReadLine() {
	if (!EnsureConsole()) return "";
	std::string line;
	std::getline(std::cin, line);
	return line;
}

inline void ConsolePrint(const std::string& msg, WORD color = DefaultConsoleAttributes()) {
	if (!EnsureConsole()) return;
	auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD original = DefaultConsoleAttributes();
	SetConsoleColor(color);
	std::wcout << ToWide(msg);
	if (!msg.empty() && msg.back() != '\n') {
		std::wcout << L"\n";
	}
	SetConsoleColor(original);
}

inline void ConsoleClear() {
	if (!EnsureConsole()) return;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
	DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
	DWORD count;
	FillConsoleOutputCharacter(hConsole, (TCHAR)' ', cellCount, { 0,0 }, &count);
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, { 0,0 }, &count);
	SetConsoleCursorPosition(hConsole, { 0,0 });
}

inline Instance GetPointerInstance(std::string name, DWORD ProcessID) {
	uintptr_t Base = Process::GetModuleBase(ProcessID);
	Instance Datamodel = FetchDatamodel(Base, ProcessID);
	Instance CoreGui = Datamodel.FindFirstChild("CoreGui");
	Instance ExternalExecutor = CoreGui.FindFirstChild("ExternalExecutor");
	Instance Pointers = ExternalExecutor.FindFirstChild("Pointer");
	Instance Pointer = Pointers.FindFirstChild(name);
	uintptr_t Target = ReadMemory<uintptr_t>(Pointer.GetAddress() + Offsets::Value, ProcessID);
	return Instance(Target, ProcessID);
}

inline std::unordered_map<std::string, std::function<std::string(std::string, nlohmann::json, DWORD)>> env;

struct WSConnection {
	std::shared_ptr<ix::WebSocket> ws;
	std::vector<std::string> msgQueue;
	std::mutex queueMutex;
	std::atomic<bool> isOpen{ false };

	void pushMessage(const std::string& msg) {
		std::lock_guard<std::mutex> lock(queueMutex);
		msgQueue.push_back(msg);
	}

	std::vector<std::string> popAllMessages() {
		std::lock_guard<std::mutex> lock(queueMutex);
		std::vector<std::string> ret = std::move(msgQueue);
		msgQueue.clear();
		return ret;
	}
};

inline std::unordered_map<std::string, std::shared_ptr<WSConnection>> wsConnections;
inline std::mutex wsMapMutex;

inline void Load() {
	env["websocket_connect"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string url = set.contains("url") ? set["url"].get<std::string>() : "";
		if (url.empty()) return "";

		auto conn = std::make_shared<WSConnection>();
		conn->ws = std::make_shared<ix::WebSocket>();

		std::string id = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(conn.get()));


		conn->ws->setOnMessageCallback([conn](const ix::WebSocketMessagePtr& msg) {
			if (msg->type == ix::WebSocketMessageType::Message) {
				conn->pushMessage("MSG:" + msg->str);
			}
			else if (msg->type == ix::WebSocketMessageType::Open) {
				conn->isOpen = true;
			}
			else if (msg->type == ix::WebSocketMessageType::Close) {
				conn->isOpen = false;
				conn->pushMessage("CLOSE:");
			}
			else if (msg->type == ix::WebSocketMessageType::Error) {
				conn->pushMessage("ERROR:" + msg->errorInfo.reason);
			}
			});


		conn->ws->setUrl(url);
		conn->ws->start();


		{
			std::lock_guard<std::mutex> lock(wsMapMutex);
			wsConnections[id] = conn;
		}

		return id;
		};

	env["websocket_send"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";
		std::string msg = set.contains("msg") ? set["msg"].get<std::string>() : "";

		std::shared_ptr<WSConnection> conn;
		{
			std::lock_guard<std::mutex> lock(wsMapMutex);
			if (wsConnections.find(id) != wsConnections.end()) {
				conn = wsConnections[id];
			}
		}

		if (conn && conn->isOpen) {
			auto result = conn->ws->send(msg);
			return result.success ? "SUCCESS" : "FAILURE";
		}
		return "FAILURE";
		};

	env["websocket_close"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";

		std::shared_ptr<WSConnection> conn;
		{
			std::lock_guard<std::mutex> lock(wsMapMutex);
			if (wsConnections.find(id) != wsConnections.end()) {
				conn = wsConnections[id];
				wsConnections.erase(id);
			}
		}

		if (conn) {
			conn->ws->stop();
			return "SUCCESS";
		}
		return "FAILURE";
		};

	env["websocket_poll"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";

		std::shared_ptr<WSConnection> conn;
		{
			std::lock_guard<std::mutex> lock(wsMapMutex);
			if (wsConnections.find(id) != wsConnections.end()) {
				conn = wsConnections[id];
			}
		}

		if (conn) {
			auto msgs = conn->popAllMessages();
			json j = json::array();
			for (const auto& m : msgs) {
				j.push_back(m);
			}
			return j.dump();
		}
		return "[]";
		};


	env["listen"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string res;
		if (orders.contains(pid)) {
			if (orders[pid] < order) {
				res = script;
			}
			else {
				res = "";
			}
			orders[pid] = order;
		}
		else {
			orders[pid] = order;
			res = script;
		}
		return res;
		};
	env["compile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		if (set["enc"] == "true") {
			return Bytecode::Compile(dta);
		}
		return Bytecode::NormalCompile(dta);
		};
	env["setscriptbytecode"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		size_t Sized;
		auto Compressed = Bytecode::Sign(dta, Sized);

		Instance TheScript = GetPointerInstance(set["cn"], pid);
		TheScript.SetScriptBytecode(Compressed, Sized);

		return "";
		};
	env["getscriptbytecode"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		Instance TheScript = GetPointerInstance(set["cn"], pid);
		return TheScript.GetScriptBytecode();
		};
	env["request"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string url = set["l"];
		std::string method = set["m"];
		std::string rBody = set["b"];
		json headersJ = set["h"];

		std::regex urlR(R"(^(http[s]?:\/\/)?([^\/]+)(\/.*)?$)");
		std::smatch urlM;
		std::string host;
		std::string path = "/";

		if (std::regex_match(url, urlM, urlR)) {
			host = urlM[2];
			if (urlM[3].matched) path = urlM[3];
		}
		else {
			return std::string("[]");
		}

		Client client(host.c_str());
		client.set_follow_location(true);

		Headers headers;
		for (auto it = headersJ.begin(); it != headersJ.end(); ++it) {
			headers.insert({ it.key(), it.value() });
		}

		Result proxiedRes;
		if (method == "GET") {
			proxiedRes = client.Get(path, headers);
		}
		else if (method == "POST") {
			proxiedRes = client.Post(path, headers, rBody, "application/json");
		}
		else if (method == "PUT") {
			proxiedRes = client.Put(path, headers, rBody, "application/json");
		}
		else if (method == "DELETE") {
			proxiedRes = client.Delete(path, headers, rBody, "application/json");
		}
		else if (method == "PATCH") {
			proxiedRes = client.Patch(path, headers, rBody, "application/json");
		}
		else {
			return std::string("[]");
		}

		if (proxiedRes) {
			json responseJ;
			responseJ["b"] = proxiedRes->body;
			responseJ["c"] = proxiedRes->status;
			responseJ["r"] = proxiedRes->reason;
			responseJ["v"] = proxiedRes->version;

			json rHeadersJ;
			for (const auto& header : proxiedRes->headers) {
				rHeadersJ[header.first] = header.second;
			}
			responseJ["h"] = rHeadersJ;

			return responseJ.dump();
		}
		return std::string("[]");
		};
	env["readfile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
			return std::string("__EE_FNF__");
		}
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			return std::string("__EE_FNF__");
		}
		std::ostringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
		};
	env["writefile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string pathStr = set.contains("path") ? set["path"].get<std::string>() : "";
		if (pathStr.empty()) {
			return std::string("");
		}
		const auto path = ResolvePath(pathStr);
		std::error_code ec;
		if (!path.parent_path().empty()) {
			fs::create_directories(path.parent_path(), ec);
		}
		std::ofstream file(path, std::ios::binary);
		if (!file.is_open()) {
			return std::string("");
		}
		file << dta;
		return std::string("success");
		};
	env["makefolder"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		if (fs::exists(path, ec) && fs::is_directory(path, ec)) {
			return std::string("success");
		}
		fs::create_directories(path, ec);
		return ec ? std::string("") : std::string("success");
		};
	env["isfile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		bool exists = fs::exists(path, ec) && fs::is_regular_file(path, ec);
		return exists ? std::string("true") : std::string("false");
		};
	env["isfolder"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		bool exists = fs::exists(path, ec) && fs::is_directory(path, ec);
		return exists ? std::string("true") : std::string("false");
		};
	env["listfiles"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		json results = json::array();
		if (fs::exists(path, ec) && fs::is_directory(path, ec)) {
			for (const auto& entry : fs::directory_iterator(path, ec)) {
				if (ec) break;
				results.push_back(entry.path().string());
			}
		}
		return results.dump();
		};
	env["delfile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		bool removed = fs::is_regular_file(path, ec) ? fs::remove(path, ec) : false;
		return removed && !ec ? std::string("success") : std::string("");
		};
	env["delfolder"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		const auto path = ResolvePath(dta);
		std::error_code ec;
		if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
			return std::string("");
		}
		fs::remove_all(path, ec);
		return ec ? std::string("") : std::string("success");
		};
	env["mouse1click"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseClick(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP) ? std::string("SUCCESS") : std::string("");
		};
	env["mouse2click"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseClick(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP) ? std::string("SUCCESS") : std::string("");
		};
	env["mouse1press"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseInput(MOUSEEVENTF_LEFTDOWN) ? std::string("SUCCESS") : std::string("");
		};
	env["mouse1release"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseInput(MOUSEEVENTF_LEFTUP) ? std::string("SUCCESS") : std::string("");
		};
	env["mouse2press"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseInput(MOUSEEVENTF_RIGHTDOWN) ? std::string("SUCCESS") : std::string("");
		};
	env["mouse2release"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return SendMouseInput(MOUSEEVENTF_RIGHTUP) ? std::string("SUCCESS") : std::string("");
		};
	env["mousemoveabs"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!set.contains("x") || !set.contains("y")) return std::string("");
		double x = set["x"].get<double>();
		double y = set["y"].get<double>();
		int screenX = GetSystemMetrics(SM_CXSCREEN) - 1;
		int screenY = GetSystemMetrics(SM_CYSCREEN) - 1;
		LONG dx = static_cast<LONG>(x * 65535.0 / screenX);
		LONG dy = static_cast<LONG>(y * 65535.0 / screenY);
		return SendMouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, dx, dy, 0, true) ? std::string("SUCCESS") : std::string("");
		};
	env["mousemoverel"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!set.contains("x") || !set.contains("y")) return std::string("");
		LONG dx = static_cast<LONG>(set["x"].get<double>());
		LONG dy = static_cast<LONG>(set["y"].get<double>());
		return SendMouseInput(MOUSEEVENTF_MOVE, dx, dy) ? std::string("SUCCESS") : std::string("");
		};
	env["mousescroll"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!set.contains("delta")) return std::string("");
		int delta = static_cast<int>(set["delta"].get<double>() * WHEEL_DELTA);
		return SendMouseInput(MOUSEEVENTF_WHEEL, 0, 0, static_cast<DWORD>(delta)) ? std::string("SUCCESS") : std::string("");
		};
	env["invalidateinstance"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!set.contains("address")) return std::string("");
		invalidatedInstances.insert(set["address"].get<std::string>());
		return std::string("SUCCESS");
		};
	env["isinstancecached"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!set.contains("address")) return std::string("false");
		auto addr = set["address"].get<std::string>();
		bool cached = invalidatedInstances.find(addr) == invalidatedInstances.end();
		return cached ? std::string("true") : std::string("false");
		};
	env["setclipboard"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		const std::wstring wide(dta.begin(), dta.end());
		if (!OpenClipboard(nullptr)) return std::string("");
		if (!EmptyClipboard()) {
			CloseClipboard();
			return std::string("");
		}
		const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!hMem) {
			CloseClipboard();
			return std::string("");
		}
		void* memPtr = GlobalLock(hMem);
		if (!memPtr) {
			GlobalFree(hMem);
			CloseClipboard();
			return std::string("");
		}
		memcpy(memPtr, wide.c_str(), bytes);
		GlobalUnlock(hMem);
		SetClipboardData(CF_UNICODETEXT, hMem);
		CloseClipboard();
		return std::string("SUCCESS");
		};
	env["isrbxactive"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		HWND target = Process::GetWindowsProcess(pid);
		HWND foreground = GetForegroundWindow();
		bool active = target != nullptr && foreground == target;
		return active ? std::string("true") : std::string("false");
		};
	env["consolecreate"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return EnsureConsole() ? std::string("SUCCESS") : std::string("");
		};
	env["rconsolecreate"] = env["consolecreate"];
	env["consoleclear"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		ConsoleClear();
		return std::string("SUCCESS");
		};
	env["rconsoleclear"] = env["consoleclear"];
	env["consoleprint"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		ConsolePrint(dta, DefaultConsoleAttributes());
		return std::string("SUCCESS");
		};
	env["rconsoleprint"] = env["consoleprint"];
	env["consoleinfo"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		ConsolePrint(dta, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		return std::string("SUCCESS");
		};
	env["rconsoleinfo"] = env["consoleinfo"];
	env["consolewarn"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		ConsolePrint(dta, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		return std::string("SUCCESS");
		};
	env["rconsolewarn"] = env["consolewarn"];
	env["consoleerr"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		ConsolePrint(dta, FOREGROUND_RED | FOREGROUND_INTENSITY);
		return std::string("SUCCESS");
		};
	env["rconsoleerr"] = env["consoleerr"];
	env["rconsoleerror"] = env["consoleerr"];
	env["consoleinput"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		return ConsoleReadLine();
		};
	env["rconsoleinput"] = env["consoleinput"];
	env["rconsolename"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		char title[256]{};
		DWORD len = GetConsoleTitleA(title, static_cast<DWORD>(sizeof(title)));
		return len ? std::string(title, len) : std::string("");
		};
	env["rconsolesettitle"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string title = set.contains("title") ? set["title"].get<std::string>() : dta;
		if (title.empty()) title = "ExternalExecutor Console";
		EnsureConsole();
		SetConsoleTitleA(title.c_str());
		return std::string("SUCCESS");
		};
	env["rconsoleclose"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		EnsureConsole();
		consoleClosable = true;
		EnableConsoleClose();
		FreeConsole();
		return std::string("SUCCESS");
		};
	env["rconsoledestroy"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		EnsureConsole();
		consoleClosable = true;
		EnableConsoleClose();
		FreeConsole();
		return std::string("SUCCESS");
		};
	env["queueteleport"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!teleportQueues.contains(pid)) {
			teleportQueues[pid] = std::vector<std::string>();
		}
		teleportQueues[pid].push_back(dta);
		return std::string("SUCCESS");
		};
	env["getteleportqueue"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		if (!teleportQueues.contains(pid) || teleportQueues[pid].empty()) {
			return std::string("[]");
		}
		json queue = json::array();
		for (const auto& script : teleportQueues[pid]) {
			queue.push_back(script);
		}
		teleportQueues[pid].clear();
		return queue.dump();
		};
	env["messagebox"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string text = set.contains("text") ? set["text"].get<std::string>() : "";
		std::string caption = set.contains("caption") ? set["caption"].get<std::string>() : "";
		int type = set.contains("type") ? set["type"].get<int>() : 0;

		int result = MessageBoxA(NULL, text.c_str(), caption.c_str(), type | MB_TOPMOST);
		return std::to_string(result);
		};
	env["getcustomasset"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string fileName = set.contains("filePath") ? set["filePath"].get<std::string>() : "";
		if (fileName.empty() || dta.empty()) {
			return std::string("INVALID_PARAMETERS");
		}

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!hProcess) {
			return std::string("ROBLOX_PATH_NOT_FOUND");
		}

		wchar_t exePath[MAX_PATH] = { 0 };
		DWORD size = MAX_PATH;
		BOOL success = QueryFullProcessImageNameW(hProcess, 0, exePath, &size);
		CloseHandle(hProcess);

		if (!success || size == 0) {
			return std::string("ROBLOX_PATH_NOT_FOUND");
		}

		fs::path robloxExePath(exePath);
		fs::path robloxDir = robloxExePath.parent_path();
		fs::path robloxContentPath = robloxDir / "content";

		std::error_code ec;
		if (!fs::exists(robloxContentPath, ec)) {
			fs::create_directories(robloxContentPath, ec);
			if (ec) {
				return std::string("ROBLOX_PATH_NOT_FOUND");
			}
		}

		std::string subFolder = "";
		fs::path filePath(fileName);
		std::string ext = filePath.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".flac") {
			subFolder = "sounds";
		}
		else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".dds") {
			subFolder = "textures";
		}
		else if (ext == ".rbxm" || ext == ".rbxmx") {
			subFolder = "models";
		}
		else if (ext == ".ttf" || ext == ".otf") {
			subFolder = "fonts";
		}

		fs::path targetDir = robloxContentPath;
		if (!subFolder.empty()) {
			targetDir = robloxContentPath / subFolder;
			fs::create_directories(targetDir, ec);
		}

		fs::path targetPath = targetDir / fileName;
		std::ofstream outFile(targetPath, std::ios::binary);
		if (!outFile.is_open()) {
			return std::string("FILE_CREATION_FAILED");
		}

		outFile << dta;
		outFile.close();

		std::string assetPath = subFolder.empty() ? fileName : (subFolder + "/" + fileName);
		return assetPath;
		};
	env["output"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::lock_guard<std::mutex> lock(outputMutex);
		std::string type = set.contains("type") ? set["type"].get<std::string>() : "Info";
		outputQueue.push("[" + type + "] " + dta);
		return "SUCCESS";
		};
	env["setfpscap"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		double fps = set.contains("fps") ? set["fps"].get<double>() : 60.0;

		double frameDelay;
		if (fps <= 0 || fps > 10000) {
			frameDelay = 0.0;
		}
		else {
			frameDelay = 1.0 / fps;
		}

		try {
			uintptr_t Base = Process::GetModuleBase(pid);
			if (!Base) {
				return std::string("FAILED_NO_BASE");
			}

			uintptr_t TaskSchedulerPtr = ReadMemory<uintptr_t>(Base + Offsets::TaskSchedulerPointer, pid);
			if (!TaskSchedulerPtr || TaskSchedulerPtr < 0x10000) {
				return std::string("FAILED_NO_SCHEDULER");
			}

			uintptr_t fpsAddress = TaskSchedulerPtr + Offsets::TaskSchedulerMaxFPS;

			WriteMemory<double>(fpsAddress, frameDelay, pid);

			return std::string("SUCCESS");
		}
		catch (...) {
			return std::string("FAILED_EXCEPTION");
		}
		};
	env["closeroblox"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if (hProcess) {
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
			return std::string("SUCCESS");
		}
		return std::string("FAILED");
		};
	env["getfpscap"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		try {
			uintptr_t Base = Process::GetModuleBase(pid);
			if (!Base) {
				return std::string("60");
			}

			uintptr_t TaskSchedulerPtr = ReadMemory<uintptr_t>(Base + Offsets::TaskSchedulerPointer, pid);
			if (!TaskSchedulerPtr) {
				return std::string("60");
			}

			double frameDelay = ReadMemory<double>(TaskSchedulerPtr + Offsets::TaskSchedulerMaxFPS, pid);

			if (frameDelay <= 0.0001) {
				return std::string("0");
			}

			int fps = static_cast<int>(1.0 / frameDelay + 0.5);

			if (fps > 10000) {
				return std::string("0");
			}

			return std::to_string(fps);
		}
		catch (...) {
			return std::string("60");
		}
		};
	env["setfflag"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string name = set.contains("name") ? set["name"].get<std::string>() : "";
		std::string value = set.contains("value") ? set["value"].get<std::string>() : dta;

		if (name.empty()) {
			return std::string("INVALID_NAME");
		}

		if (fflagBlacklist.find(name) != fflagBlacklist.end()) {
			return std::string("BLACKLISTED");
		}

		FetchFFlags();

		if (!CheckVersionMatch(pid)) {
			return std::string("VERSION_MISMATCH");
		}

		auto it = fflagOffsets.find(name);
		if (it == fflagOffsets.end()) {
			return std::string("NOT_FOUND");
		}

		try {
			uintptr_t Base = Process::GetModuleBase(pid);
			if (!Base) {
				return std::string("NO_BASE");
			}

			uintptr_t addr = Base + it->second;

			bool isBoolFlag = (name.find("Enable") != std::string::npos) ||
				(name.find("Disable") != std::string::npos) ||
				(name.find("Use") == 0) ||
				(name.find("Allow") != std::string::npos) ||
				(name.find("Show") != std::string::npos) ||
				(name.find("Is") == 0);

			if (value == "true" || value == "false" || isBoolFlag) {
				bool boolVal = (value == "true" || value == "1");
				WriteMemory<bool>(addr, boolVal, pid);
			}
			else {
				try {
					long long llVal = std::stoll(value);
					if (llVal >= INT_MIN && llVal <= INT_MAX) {
						WriteMemory<int>(addr, static_cast<int>(llVal), pid);
					}
					else {
						WriteMemory<long long>(addr, llVal, pid);
					}
				}
				catch (...) {
					try {
						double dblVal = std::stod(value);
						WriteMemory<double>(addr, dblVal, pid);
					}
					catch (...) {
						return std::string("INVALID_VALUE");
					}
				}
			}

			return std::string("SUCCESS");
		}
		catch (...) {
			return std::string("FAILED");
		}
		};
	env["getfflag"] = [](std::string dta, nlohmann::json set, DWORD pid) -> std::string {
		std::string name = set.contains("name") ? set["name"].get<std::string>() : dta;

		if (name.empty()) {
			return std::string("INVALID_NAME");
		}

		if (fflagBlacklist.find(name) != fflagBlacklist.end()) {
			return std::string("BLACKLISTED");
		}

		FetchFFlags();

		if (!CheckVersionMatch(pid)) {
			return std::string("VERSION_MISMATCH");
		}

		auto it = fflagOffsets.find(name);
		if (it == fflagOffsets.end()) {
			return std::string("NOT_FOUND");
		}

		try {
			uintptr_t Base = Process::GetModuleBase(pid);
			if (!Base) {
				return std::string("NO_BASE");
			}

			uintptr_t addr = Base + it->second;

			bool isBoolFlag = (name.find("Enable") != std::string::npos) ||
				(name.find("Disable") != std::string::npos) ||
				(name.find("Use") == 0) ||
				(name.find("Allow") != std::string::npos) ||
				(name.find("Show") != std::string::npos) ||
				(name.find("Is") == 0);

			if (isBoolFlag) {
				bool val = ReadMemory<bool>(addr, pid);
				return val ? std::string("true") : std::string("false");
			}

			int intVal = ReadMemory<int>(addr, pid);
			return std::to_string(intVal);
		}
		catch (...) {
			return std::string("FAILED");
		}
		};
}

inline std::string Setup(std::string args) {
	auto lines = SplitLines(args);

	std::string typ = lines.size() > 0 ? lines[0] : "";
	DWORD pid = lines.size() > 1 ? std::stoul(lines[1]) : 0;


	nlohmann::json set;
	if (lines.size() > 2) {
		int retries = 0;
		const int maxRetries = 3;
		bool parsed = false;

		while (!parsed && retries < maxRetries) {
			try {
				set = nlohmann::json::parse(lines[2]);
				parsed = true;
			}
			catch (const nlohmann::json::parse_error& e) {
				retries++;
				if (retries < maxRetries) {
					Sleep(100);
				}
				else {

					set = nlohmann::json{};
				}
			}
		}
	}
	else {
		set = nlohmann::json{};
	}

	std::string dta;

	for (size_t i = 3; i < lines.size(); ++i) {
		dta += lines[i];
		if (i + 1 < lines.size()) dta += "\n";
	}

	return env[typ] ? env[typ](dta, set, pid) : "";
}

inline void StartBridge()
{
	Load();
	Server Bridge;
	Bridge.Post("/handle", [](const Request& req, Response& res) {
		res.status = 200;
		res.set_content(Setup(req.body), "text/plain");
		});
	Bridge.set_exception_handler([](const Request& req, Response& res, std::exception_ptr ep) {
		std::string errorMessage;
		try {
			std::rethrow_exception(ep);
		}
		catch (std::exception& e) {
			errorMessage = e.what();
		}
		catch (...) {
			errorMessage = "Unknown Exception";
		}
		res.set_content("{\"error\":\"" + errorMessage + "\"}", "application/json");
		res.status = 500;
		});
	Bridge.listen("localhost", 9611);
}

inline void Execute(std::string source) {
	script = source;
	order += 1;
}
