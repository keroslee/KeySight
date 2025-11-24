#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <codecvt>
#include <mutex>
#include <gdiplus.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include <ShlObj.h> // 添加头文件用于获取AppData路径
#include <thread>

#pragma comment(lib, "gdiplus.lib")
//#include <cstdio>

namespace {
	using std::wstring; using std::to_wstring; using std::map; using std::unordered_map;

	constexpr const wchar_t* APP_NAME = L"KeySight";
	constexpr const wchar_t* MUTEX_NAME = L"Global\\Mutex_KeySight_9F4A8B2E-1234-5678-90AB-CDEF01234567";
	wstring path = L".\\";
	wstring exefile = path + APP_NAME + L".exe";
	wstring configfile = wstring(APP_NAME) + L".ini";
	constexpr const wchar_t* SECTION_CONFIG = L"Config";
	wstring statfile = wstring(APP_NAME) + L".txt";
	wstring logfile = wstring(APP_NAME) + L".log";
	uint32_t total;
	HWND g_hMainWnd = NULL;
	HWND g_hHotkeyWnd = NULL;
	NOTIFYICONDATA g_trayIcon;
	HMENU hMenu = nullptr;
#define ENUM_LAN(X) X(lan_app) X(lan_enable) X(lan_disable) X(lan_onboot) X(lan_stat) X(lan_show) X(lan_hotkey) X(lan_exit) X(lan_total) X(lan_hotkey_tip) X(lan_hotkey_title) X(id_hotkey) X(lan_suc) X(lan_fai) X(lan_dup) X(lan_view) X(lan_viewing) X(lan_view_failed) X(lan_view_success)
#define ENUM_CONF(X) X(boot) X(stats) X(show) X(hotkey) X(lang) X(view_url)
#define ENUM_CMD(X) X(cmd_query) X(cmd_create) X(cmd_del)
#define ENUM_LIST(X)\
	WM_APP_MENU = WM_APP + 1, X(WM_APP_ONBOOT) X(WM_APP_STAT) X(WM_APP_SHOW) X(WM_APP_HOTKEY) X(WM_APP_EXIT) X(WM_APP_TOOLTIP) X(WM_APP_VIEW)\
ENUM_LAN(X) ENUM_CONF(X) ENUM_CMD(X)
	/*IDC_CLOSE_BUTTON,*/
	enum MyEnum {
		start,
#define ENUM_GEN(name) name,
		ENUM_LIST(ENUM_GEN)
#undef ENUM_GEN
	};
	unordered_map<wstring, MyEnum> str2enum = {
	#define GEN_ITEM(key) {L#key, key},
		ENUM_LAN(GEN_ITEM) ENUM_CONF(GEN_ITEM) ENUM_CMD(GEN_ITEM)
	#undef GEN_ITEM
	};
	unordered_map<MyEnum, const wchar_t*> enum2str = {
	#define GEN_ITEM(key) {key, L#key},
		ENUM_LAN(GEN_ITEM) ENUM_CONF(GEN_ITEM) ENUM_CMD(GEN_ITEM)
	#undef GEN_ITEM
	};
	template<typename T>
	struct ConfigItem
	{
		const wchar_t* key;
		T value;
	};
	WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
	WCHAR default_url[2083] = L"https://keros.im/kb.htm";//2083 for IE. IE is the shortest
	struct Config {
		ConfigItem<bool> enableAll{ L"EnableAll", true };
		ConfigItem<bool> boot{ enum2str[MyEnum::boot], false };
		ConfigItem<bool> stats{ enum2str[MyEnum::stats], true };
		ConfigItem<bool> show{ enum2str[MyEnum::show], true };
		ConfigItem<uint32_t> hotkey{ enum2str[MyEnum::hotkey], 0 };
		ConfigItem<wchar_t*> view_url{ enum2str[MyEnum::view_url], default_url };
		ConfigItem<wchar_t*> lang{ enum2str[MyEnum::lang], localeName };
	} g_config;
	map<MyEnum, ConfigItem<bool>*> menuMap = {
		{ WM_APP_ONBOOT,  &g_config.boot },
		{ WM_APP_STAT,    &g_config.stats },
		{ WM_APP_SHOW,    &g_config.show },
	};
	unordered_map<MyEnum, wstring> g_langMap = {
		{lan_app, L"按键统计"},
		{lan_enable, L"启用"},
		{lan_disable, L"禁用"},
		{lan_onboot, L"开机启动"},
		{lan_stat, L"启用统计"},
		{lan_show, L"显示按键"},
		{lan_hotkey, L"设置快捷键"},
		{lan_exit, L"退出"},
		{lan_total, L"总按键数: "},
		{lan_hotkey_tip, L"按下快捷键设置/按Del键删除"},
		{lan_hotkey_title, L"设置快捷键"},
		{lan_suc, L"成功"},
		{lan_fai, L"失败"},
		{lan_dup, L"程序已经在运行中，查看任务栏中的图标"},
		{lan_view, L"查看统计"},
		{lan_viewing, L"正在打开网页，查看按键热度"},
		{lan_view_failed, L"打开网页失败"},
		{lan_view_success, L"打开网页成功"}
	};
	unordered_map<MyEnum, wstring> g_langEnMap = {
		{lan_app, L"Key Statistics" },
		{ lan_enable, L"Enable" },
		{ lan_disable, L"Disable" },
		{ lan_onboot, L"Start on Boot" },
		{ lan_stat, L"Enable Statistics" },
		{ lan_show, L"Show Keys" },
		{ lan_hotkey, L"Set Hotkey" },
		{ lan_exit, L"Exit" },
		{ lan_total, L"Total Keystrokes: " },
		{ lan_hotkey_tip, L"Press hotkey to set / Del to remove" },
		{ lan_hotkey_title, L"Set Hotkey" },
		{ lan_suc, L"Success" },
		{ lan_fai, L"Failed" },
		{ lan_dup, L"Application is already running, check system tray" },
		{ lan_view, L"View Statistics" },
		{ lan_viewing, L"Opening webpage to view key heatmap" },
		{ lan_view_failed, L"Failed to open webpage" },
		{ lan_view_success, L"Webpage opened successfully"}
	};
	static map<wstring, wstring> lanMap = {
		{ L"lan_app",  g_langMap[lan_app] },
	};
	struct Record {
		uint32_t count = 0;
		wstring keyText;
		//wstring keyText2;//修正
	};
	unordered_map<uint32_t, Record> g_keyCounts;
	unordered_map<USHORT, wstring> keyPatch = {
		{VK_LWIN,L"Win"},
		{VK_RWIN,L"Win"},
		{VK_UP,L"↑"},
		{VK_DOWN,L"↓"},
		{VK_LEFT,L"←"},
		{VK_RIGHT,L"→"}
	};

	inline const wchar_t* to_wide(const char* narrow_str) {
		// 使用线程本地存储避免多线程问题
		thread_local static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		thread_local static std::wstring wstr;

		try {
			wstr = converter.from_bytes(narrow_str);
			return wstr.c_str();
		}
		catch (...) {
			return L"<conversion_error>";
		}
	}
#ifdef _DEBUG
	inline void log(const char* format, ...)
	{
		constexpr size_t SIZE = 1024;
		char buffer[SIZE];
		va_list args;
		va_start(args, format);
		_vsnprintf_s(buffer, SIZE, _TRUNCATE, format, args);
		va_end(args);

		// 输出到调试器（如 Visual Studio）
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}
	inline void log(const wchar_t* format, ...)
	{
		constexpr size_t SIZE = 1024;
		wchar_t buffer[SIZE];
		va_list args;
		va_start(args, format);
		_vsnwprintf_s(buffer, SIZE, _TRUNCATE, format, args);
		va_end(args);

		// 输出到调试器（如 Visual Studio）
		OutputDebugString(buffer);
		OutputDebugString(L"\n");
	}
	inline void log(const wchar_t* function, int line, const wchar_t* format, ...)
	{
		constexpr size_t SIZE = 1024;
		wchar_t buffer[SIZE];

		// 格式化函数名和行号前缀
		int prefix_len = swprintf_s(buffer, SIZE, L"[%s @ %d] ", function, line);

		// 处理可变参数
		va_list args;
		va_start(args, format);
		_vsnwprintf_s(buffer + prefix_len, SIZE - prefix_len, _TRUNCATE, format, args);
		va_end(args);

		// 输出到调试器
		OutputDebugString(buffer);
		OutputDebugString(L"\n");
	}
#define LOG(...) log(to_wide(__func__), __LINE__, __VA_ARGS__)
#else
	inline void log(const wchar_t* format, ...)
	{
		std::wofstream logFile(logfile.c_str(), std::ios::app);
		if (logFile.is_open()) {
			constexpr size_t SIZE = 1024;
			wchar_t buffer[SIZE];
			va_list args;
			va_start(args, format);
			_vsnwprintf_s(buffer, SIZE, _TRUNCATE, format, args);
			va_end(args);

			// 设置UTF-8编码
			logFile.imbue(std::locale(std::locale::classic(), new std::codecvt_utf8<wchar_t>));
			logFile << buffer << std::endl;
		}
	}
	inline void log(const wchar_t* function, int line, const wchar_t* format, ...)
	{
		std::wofstream logFile(logfile.c_str(), std::ios::app);
		if (logFile.is_open()) {
			// 设置UTF-8编码
			logFile.imbue(std::locale(std::locale::classic(), new std::codecvt_utf8<wchar_t>));

			// 写入函数名和行号
			logFile << L"[" << function << L" @ " << line << L"]\t";

			// 处理可变参数
			constexpr size_t SIZE = 1024;
			wchar_t buffer[SIZE];
			va_list args;
			va_start(args, format);
			_vsnwprintf_s(buffer, SIZE, _TRUNCATE, format, args);
			va_end(args);

			// 写入日志内容
			logFile << buffer << std::endl;
		}
	}
	inline int checklog() {
		std::wifstream inFile(logfile);
		if (!inFile) {
			return MessageBox(g_hMainWnd, (L"未能打开文件：" + logfile).c_str(), L"文件错误", MB_ICONWARNING);
		}

		// 检查文件行数是否超过100
		unsigned lineCount = 0;
		std::wstring line;
		while (std::getline(inFile, line)) {
			++lineCount;
		}
		inFile.close();
		unsigned maxline = 2000;
		if (lineCount <= maxline) return 0;

		std::wstring tmpFilename = logfile + L".tmp";
		std::wifstream inFileAgain(logfile);
		std::wofstream outFile(tmpFilename);
		if (!inFileAgain || !outFile) {
			return MessageBox(g_hMainWnd, L"无法创建临时文件", L"文件错误", MB_ICONWARNING);
		}

		// 跳过第一行
		for (unsigned i = 0; i < lineCount - maxline; i++)std::getline(inFileAgain, line);

		// 复制剩余内容
		while (std::getline(inFileAgain, line)) {
			outFile << line << '\n';
		}

		// 关闭文件流
		inFileAgain.close();
		outFile.close();

		// 替换原文件
		if (!DeleteFile(logfile.c_str())) {
			return MessageBox(g_hMainWnd, L"删除原文件失败", L"文件错误", MB_ICONWARNING);
		}
		if (!MoveFile(tmpFilename.c_str(), logfile.c_str())) {
			return MessageBox(g_hMainWnd, L"重命名临时文件失败", L"文件错误", MB_ICONWARNING);
		}
		return 0;
	}
#define LOG(...) log(to_wide(__func__), __LINE__, __VA_ARGS__)
#endif
	void initPath() {
		WCHAR exePath[MAX_PATH];
		GetModuleFileName(NULL, exePath, MAX_PATH);

		// 使用标准库函数提取目录路径
		exefile = exePath;
		size_t lastSlash = exefile.find_last_of(L"\\/");

		if (lastSlash != wstring::npos) {
			path = exefile.substr(0, lastSlash + 1); // 包含结尾的反斜杠
			configfile = path + configfile;
			statfile = path + statfile;
			logfile = path + logfile;
		}
	}
	void initIcon(HWND hWnd) {
		g_trayIcon.cbSize = { sizeof(g_trayIcon) };  // 清空并设置结构大小 sizeof(NOTIFYICONDATA);
		g_trayIcon.hWnd = hWnd;
		g_trayIcon.uID = 100;
		g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
		g_trayIcon.uCallbackMessage = WM_APP_MENU;
		g_trayIcon.hIcon = LoadIcon(NULL, IDI_ASTERISK);
		wcscpy_s(g_trayIcon.szTip, (g_langMap[lan_total] + to_wstring(total)).c_str());
		Shell_NotifyIcon(NIM_ADD, &g_trayIcon);
	}
	void initMenu() {
#define MENU(config,menuid,lanid) AppendMenu(hMenu, MF_STRING | (config ? MF_CHECKED : 0), menuid, g_langMap[lanid].c_str())
		hMenu = CreatePopupMenu();
		MENU(g_config.boot.value, WM_APP_ONBOOT, lan_onboot);
		MENU(g_config.stats.value, WM_APP_STAT, lan_stat);
		MENU(g_config.show.value, WM_APP_SHOW, lan_show);
		MENU(g_config.hotkey.value, WM_APP_HOTKEY, lan_hotkey);
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, WM_APP_VIEW, g_langMap[lan_view].c_str()); // 添加查看统计菜单项
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, WM_APP_EXIT, g_langMap[lan_exit].c_str());
#undef MENU
	}
	void LoadLan() {
		wchar_t buf[LOCALE_NAME_MAX_LENGTH];
#define GET_STR(name) if(g_langMap.count(name)){GetPrivateProfileString(g_config.lang.value, enum2str[name], g_langMap[name].c_str(), buf, LOCALE_NAME_MAX_LENGTH,configfile.c_str());g_langMap[name] = buf;}
		ENUM_LAN(GET_STR);
#undef GET_STR
	}
	void LoadConfig(ConfigItem<bool>& item) {
		item.value = GetPrivateProfileInt(SECTION_CONFIG, item.key, item.value, configfile.c_str());
	}

	void LoadConfig() {
#define GET_INT(name) g_config.name.value = GetPrivateProfileInt(SECTION_CONFIG, g_config.name.key, g_config.name.value, configfile.c_str())
		GET_INT(boot);
		GET_INT(stats);
		GET_INT(show);
		GET_INT(hotkey);
#undef GET_INT

		LANGID langId = GetUserDefaultUILanguage(); // 获取用户UI语言（而不是系统语言）

		if (LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), localeName, LOCALE_NAME_MAX_LENGTH, 0)) {
			LOG(localeName);
		}
		else {
			LOG(L"unknown");
		}
		if (PRIMARYLANGID(langId)!=LANG_CHINESE && PRIMARYLANGID(langId)!=LANG_CHINESE_TRADITIONAL)
			g_langMap = g_langEnMap;
		GetPrivateProfileString(SECTION_CONFIG, g_config.view_url.key, default_url, g_config.view_url.value, ARRAYSIZE(default_url), configfile.c_str());
		GetPrivateProfileString(SECTION_CONFIG, g_config.lang.key, localeName, g_config.lang.value, ARRAYSIZE(localeName), configfile.c_str());
	}
	void SaveConfig(const ConfigItem<bool>& item) {
		WritePrivateProfileString(SECTION_CONFIG, item.key, to_wstring(item.value).c_str(), configfile.c_str());
	}
	void SaveConfig(const ConfigItem<uint32_t>& item) {
		WritePrivateProfileString(SECTION_CONFIG, item.key, to_wstring(item.value).c_str(), configfile.c_str());
	}
	void SaveConfig(const ConfigItem<wchar_t*>& item) {
		WritePrivateProfileString(SECTION_CONFIG, item.key, item.value, configfile.c_str());
	}
	void SaveConfig() {
#define STR(config) WritePrivateProfileString(SECTION_CONFIG, g_config.config.key, to_wstring(g_config.config.value).c_str(), configfile.c_str())
		STR(boot);
		STR(stats);
		STR(show);
		STR(hotkey);
		SaveConfig(g_config.lang);
		SaveConfig(g_config.view_url);
#undef STR
	}
	void initConfigFile(const wstring& iniPath) {
		DWORD attr = GetFileAttributesW(iniPath.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
			SaveConfig();
			LANGID langId = GetUserDefaultUILanguage();
			if (PRIMARYLANGID(langId) == LANG_CHINESE || PRIMARYLANGID(langId) == LANG_CHINESE_TRADITIONAL)
				for (const auto& lan : g_langMap)
				{
					WritePrivateProfileString(localeName, enum2str[lan.first], lan.second.c_str(), configfile.c_str());
				}
			for (const auto& lan : g_langEnMap)
			{
				WritePrivateProfileString(L"en-US", enum2str[lan.first], lan.second.c_str(), configfile.c_str());
			}
		}
	}
	bool RunAsAdmin(const wchar_t* param) {
		wchar_t exePath[MAX_PATH];
		GetModuleFileName(NULL, exePath, MAX_PATH);
		SHELLEXECUTEINFOW sei = { sizeof(sei) };
		sei.lpVerb = L"runas";
		sei.lpFile = exePath;
		sei.lpParameters = param;
		sei.nShow = SW_HIDE;
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		return ShellExecuteEx(&sei);
	}
	void HandleTaskArguments(LPWSTR cmdLine) {
		wstring cmd = L"";
		MyEnum ss = (MyEnum)_wtoi(cmdLine);
		if (cmd_create == ss) {
			initPath();
			cmd = L"schtasks /create /tn \"" + wstring(APP_NAME) + L"\" /tr \"\"\"" + exefile + L"\"\"\" /sc onlogon /f >> \"" + path + L"create.log" + L"\" 2>&1";
		}
		else if (cmd_del == ss)cmd = L"schtasks /delete /TN \"" + wstring(APP_NAME) + L"\" /f > NUL 2>&1";
		if (L"" != cmd) {
			_wsystem(cmd.c_str());
			ExitProcess(0);
		}
	}
	bool query() {
		wstring cmd = L"schtasks /query /TN \"" + wstring(APP_NAME) + L"\" > NUL 2>&1";
		return 0 == _wsystem(cmd.c_str());
	}
	void toggle(MyEnum id) {
		if (nullptr != menuMap[id]) {
			(*menuMap[id]).value = !(*menuMap[id]).value;
			CheckMenuItem(hMenu, id, MF_BYCOMMAND | ((*menuMap[id]).value ? MF_CHECKED : MF_UNCHECKED));
		}
	}
	void onboot() {
		if (query() != g_config.boot.value) {
			MyEnum id = g_config.boot.value ? cmd_create : cmd_del;
			if (!RunAsAdmin(to_wstring(id).c_str())) {// if user denied request,set g_config.boot.value by weather task exist
				toggle(WM_APP_ONBOOT);
				SaveConfig(g_config.boot);
			}
		}
	}
	bool IsModifierKey(UINT vk) {
		return (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN);
	}
	wstring keyName(LPARAM lParam, UINT vk) {
		if (!keyPatch[vk].empty())return keyPatch[vk];
		UINT scanCode;
		if (vk == VK_SHIFT || vk == VK_MENU || vk == VK_CONTROL) {
			scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
			scanCode <<= 16;
		}
		else scanCode = lParam & 0x1ff << 16;//1 for Extended,ff for scan code
		WCHAR keyName[64] = {};
		GetKeyNameText(scanCode, keyName, 64);
		LOG(L"%s:%s, vk:0x%X, scan:0x%X, ext:%d", lParam & 1 << 31 ? L"up" : L"down", keyName, vk, scanCode, (lParam & 0x1000000) != 0);
		return wstring(keyName);
	}
	wstring VKeyToString(const RAWKEYBOARD& kb, bool patch = false) {
		/*auto it = g_keyCounts.find(kb.VKey);
		if (it != g_keyCounts.end()&& !it->second.keyText.empty()){
			if (patch) return it->second.keyText2.empty() ? it->second.keyText : it->second.keyText2;
			else return it->second.keyText;
		}*/
		if (patch && !keyPatch[kb.VKey].empty())return keyPatch[kb.VKey];
		if (patch && (kb.VKey == VK_LWIN || kb.VKey == VK_RWIN)) return keyPatch[VK_LWIN];

		switch (kb.VKey) {
		case VK_PAUSE:case VK_CANCEL:   return L"Pause/Break";
		case VK_VOLUME_MUTE:            return L"Vx";
		case VK_VOLUME_DOWN:            return L"V-";
		case VK_VOLUME_UP:              return L"V+";
		}

		bool isExtended = kb.Flags & RI_KEY_E0;
		if (kb.VKey == VK_CONTROL || kb.VKey == VK_MENU) isExtended = isExtended && !patch;
		USHORT makeCode = (patch && kb.VKey == VK_SHIFT) ? MapVirtualKey(kb.VKey, MAPVK_VK_TO_VSC) : kb.MakeCode;//right shift
		LONG lParam = (static_cast<LONG>(makeCode) << 16) | (isExtended ? (1 << 24) : 0);
		WCHAR keyName[128] = { 0 };
		if (GetKeyNameText(lParam, keyName, ARRAYSIZE(keyName)) > 0) {
			return keyName;
		}
		return L"unkown";
	}
	void UpdateTrayTooltip() {
		g_trayIcon.uFlags = NIF_TIP;  // 只更新提示信息
		wstring tip = g_langMap[lan_total] + to_wstring(total);
		wcscpy_s(g_trayIcon.szTip, tip.c_str());
		Shell_NotifyIcon(NIM_MODIFY, &g_trayIcon);
	}
	void ShowContextMenu(HWND hwnd) {
		total++;
		POINT pt;
		GetCursorPos(&pt);
		PostMessage(hwnd, WM_NULL, 0, 0);
		SetForegroundWindow(hwnd);
		PostMessage(hwnd, WM_NULL, 0, 0);
		TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
	}
	void RegisterRawInput(HWND hWnd) {
		RAWINPUTDEVICE rid[2] = {};

		rid[0].usUsagePage = 0x01;
		rid[0].usUsage = 0x06;
		rid[0].dwFlags = RIDEV_INPUTSINK;
		rid[0].hwndTarget = hWnd;

		rid[1].usUsagePage = 0x01;
		rid[1].usUsage = 0x02;
		rid[1].dwFlags = RIDEV_INPUTSINK;
		rid[1].hwndTarget = hWnd;

		RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
	}
	using std::locale; using std::codecvt_utf8;
	void LoadStatistics() {
		std::wifstream file(statfile);
		//if (!file.is_open())MessageBox(g_hMainWnd, (L"未能打开文件：" + statfile).c_str(), L"文件错误", MB_ICONWARNING);
		file.imbue(locale(file.getloc(), new codecvt_utf8<wchar_t>));

		wstring line;
		while (getline(file, line)) {
			std::wistringstream ss(line);
			uint32_t vkey;
			uint32_t count;
			// 先读 key 和 count
			if (ss >> vkey >> count) {
				// 剩下的部分就是 keyText（包括所有空格）
				wstring keyText, keyText2;
				getline(ss, keyText, L'\t');
				getline(ss, keyText, L'\t');
				getline(ss, keyText2);
				g_keyCounts[vkey] = { count,keyText,/*keyText2*/ };
				total += count;
			}
		}
	}
	void SaveStatistics() {
		std::wofstream file(statfile);
		file.imbue(locale(file.getloc(), new codecvt_utf8<wchar_t>));

		for (const auto& pair : g_keyCounts) {
			file << pair.first << L'\t' << pair.second.count << L'\t' << pair.second.keyText /*<< L'\t' << pair.second.keyText2*/ << L'\n';
		}
	}
	void count(Record& rec) {
		rec.count++;
		total++;
		if (0 == total % 10)
			SaveStatistics();
	}

	using std::vector; using std::mutex; using std::lock; using std::lock_guard;
	using std::chrono::steady_clock; using std::chrono::duration_cast; using std::chrono::milliseconds;
	using Gdiplus::Graphics; using Gdiplus::Color; using Gdiplus::Pen; using Gdiplus::SolidBrush;
	using Gdiplus::Point; using Gdiplus::PointF; using Gdiplus::Rect; using Gdiplus::RectF;
	using Gdiplus::REAL;

	HINSTANCE g_hInstance;
	HWND           g_hOverlayWnd = nullptr;
	HDC            g_hMemDC = nullptr;
	HBITMAP        g_hMemBmp = nullptr;
	ULONG_PTR      g_gdiplusToken = 0;
	UINT           g_uShowTimerID = 1;
	const UINT     SHOW_DURATION_MS = 4000;      // 显示时长
	const INT      OVERLAY_W = 50;      // 窗口宽度
	const INT      OVERLAY_H = 50;       // 窗口高度
	static REAL   g_bitmapWidth, g_bitmapHeight;
	// 用于存储每个要显示的条目信息
	struct Entry {
		vector<wstring*> keys;                 // 要显示的字符串（可能不含 "Win" 或含 "Win"）
		ULONG mouse;
		steady_clock::time_point startTime; // 记录何时加入
	};

	// 全局静态：所有正在显示或等待淡出/淡入的条目
	static vector<Entry>        g_overlayEntries;
	// 保护 g_overlayEntries 的并发安全（按需，可删除，如果上层只有主线程调用则可忽略）
	static mutex                g_entriesMutex;

	// 下面两个常量可根据实际需求调整
	static const int   FADE_DURATION_MS = 600;      // 淡入/淡出各 200ms
	static const int   ENTRY_TOTAL_MS = SHOW_DURATION_MS / 2; // 2000ms（已有宏）
	static const int   TIMER_INTERVAL_MS = 30;       // 窗口定时器：30ms 一帧

	// GDI+ 字体、画刷等可复用对象，避免每次重建
	static Gdiplus::Font* g_pFont = nullptr;
	static SolidBrush* g_pTextBrush = nullptr;
	
	// 绘制 Windows 徽标（四个旋转的彩色方块）
	RectF DrawWindowsLogo(Graphics& graph, INT x, INT y, REAL h,BYTE alpha=255) {
		const INT spacing = 1;     // 方块间距
		const INT size = (h - spacing) / 2;       // 单个方块大小
		const INT totalSize = size * 2 + spacing;

		Color colors[] = {
			Color(alpha, 0, 120, 215),   // 蓝
			Color(alpha, 0, 153, 68),    // 绿
			Color(alpha, 237, 41, 57),   // 红
			Color(alpha, 255, 185, 0)    // 黄
		};

		Point positions[] = {
			Point(x, y),
			Point(x + size + spacing, y),
			Point(x, y + size + spacing),
			Point(x + size + spacing, y + size + spacing)
		};
		SolidBrush* brush = new SolidBrush(colors[0]);
		for (INT i = 0; i < 4; ++i) {
			brush->SetColor(colors[i]);
			//Rect rect(positions[i].X, positions[i].Y, size, size);

			// 保存当前变换状态
			//Matrix originalMatrix;
			//g.GetTransform(&originalMatrix);

			// 绕中心旋转45度
			//g.TranslateTransform(rect.X + rect.Width / 2.0f,rect.Y + rect.Height / 2.0f);
			//g.RotateTransform(45.0f);

			// 绘制旋转后的方块
			graph.FillRectangle(brush, positions[i].X, positions[i].Y, size, size);

			// 恢复变换
			//g.SetTransform(&originalMatrix);
		}
		return RectF(x, y, h, h);
	}
	RectF DrawMouse(Graphics& graph, INT x, INT y, UINT h, const ULONG ulButtons,BYTE alpha=255) {
		USHORT btn = ulButtons & 0xffff;
		UINT w = static_cast<UINT>(h * 0.8);
		Rect mouseRect(x, y, w, h);

		// 绘制鼠标主体
		Pen whitePen(Color(alpha, 255, 255, 255));

		// 绘制左右键
		UINT buttonHeight = h / 3;
		SolidBrush brush(Color(alpha, 255, 255, 255));

		// 左键
		Rect leftBtn(x, y, w / 2, buttonHeight);
		graph.DrawRectangle(&whitePen, leftBtn);
		if (btn & RI_MOUSE_LEFT_BUTTON_UP) {
			brush.SetColor(Color(alpha, 199, 21, 133));
			graph.FillRectangle(&brush, leftBtn);
		}

		// 右键
		Rect rightBtn(x + w / 2, y, w / 2, buttonHeight);
		if (btn & RI_MOUSE_RIGHT_BUTTON_UP) {
			brush.SetColor(Color(alpha, 123, 104, 238));
			graph.FillRectangle(&brush, rightBtn);
		}
		graph.DrawRectangle(&whitePen, rightBtn);

		int wheelSize = w / 5;
		Rect wheelRect(
			x + w / 2 - wheelSize / 2,
			y + buttonHeight + 7,
			wheelSize,
			wheelSize
		);
		if (btn & RI_MOUSE_MIDDLE_BUTTON_UP) {// 滚轮区域
			brush.SetColor(Color(alpha,160, 160, 160));
			//graph.FillRectangle(&wheelBrush, wheelRect);
			graph.DrawRectangle(&whitePen, wheelRect);
		}
		if (btn & RI_MOUSE_WHEEL) {
			// 绘制滚动方向指示
			Pen arrowPen(Color(alpha,255, 255, 255), 1);
			int centerX = wheelRect.GetLeft() + wheelSize / 2;
			int centerY = wheelRect.GetTop();
			int arrW = 3, arrH = 5;
			SHORT delta = ulButtons>>16;
			if (delta > 0) { // 向上
				graph.DrawLine(&arrowPen, centerX, centerY-arrH, centerX - arrW, centerY);
				graph.DrawLine(&arrowPen, centerX, centerY-arrH, centerX + arrW, centerY);
			}
			else if (delta < 0) { // 向下
				graph.DrawLine(&arrowPen, centerX, centerY +wheelSize+arrH, centerX - arrW, centerY+arrH);
				graph.DrawLine(&arrowPen, centerX, centerY +wheelSize+arrH, centerX + arrW, centerY+arrH);
			}
		}
		// 左侧额外按键
		int extraBtnWidth = w / 6;
		int extraBtnHeight = h / 6;
		Rect extra1(
			x,
			y + h / 2,
			extraBtnWidth,
			extraBtnHeight
		);
		//graph.DrawRectangle(&whitePen, extra1);
		if (btn & RI_MOUSE_BUTTON_5_UP) {
			brush.SetColor(Color(alpha, 50, 205, 50));
			graph.FillRectangle(&brush, extra1);
		}

		Rect extra2(
			x,
			y + h / 2 + extraBtnHeight,
			extraBtnWidth,
			extraBtnHeight
		);
		//graph.DrawRectangle(&whitePen, extra2);
		if (btn & RI_MOUSE_BUTTON_4_UP) {
			brush.SetColor(Color(alpha, 30, 144, 255));
			graph.FillRectangle(&brush, extra2);
		}

		whitePen.SetWidth(2.0f);
		graph.DrawRectangle(&whitePen, mouseRect);
		return RectF(x, y, w, h);
	}
	static void DrawOverlay() {
		auto now = steady_clock::now();

		// 1. 拷贝当前队列并计算 size
		vector<Entry> entriesCopy;
		{
			lock_guard<mutex> lock(g_entriesMutex);
			entriesCopy = g_overlayEntries;
		}
		int size = static_cast<int>(entriesCopy.size());
		if (size == 0) {
			KillTimer(g_hOverlayWnd, g_uShowTimerID);
			ShowWindow(g_hOverlayWnd, SW_HIDE);
			return;
		}

		// 2. 计算每条 alpha 并剔除过期
		vector<BYTE> alphas(size);
		vector<bool> stillAlive(size);
		for (int i = 0; i < size; ++i) {
			auto& e = entriesCopy[i];
			int elapsed = static_cast<int>(duration_cast<milliseconds>(now - e.startTime).count());
			if (elapsed >= ENTRY_TOTAL_MS) {
				alphas[i] = 0;
				stillAlive[i] = false;
			}
			else {
				BYTE alpha = 255;
				if (elapsed < FADE_DURATION_MS) {
					alpha = static_cast<BYTE>((elapsed / (float)FADE_DURATION_MS) * 255.0f);
				}
				else if (elapsed > ENTRY_TOTAL_MS - FADE_DURATION_MS) {
					int fo = elapsed - (ENTRY_TOTAL_MS - FADE_DURATION_MS);
					alpha = static_cast<BYTE>(((FADE_DURATION_MS - fo) / (float)FADE_DURATION_MS) * 255.0f);
				}
				alphas[i] = alpha;
				stillAlive[i] = true;
			}
		}

		{
			lock_guard<mutex> lock(g_entriesMutex);
			auto& vec = g_overlayEntries;
			vec.erase(remove_if(vec.begin(), vec.end(), [&](const Entry& e) {
				auto elapsed = duration_cast<milliseconds>(now - e.startTime).count();
				return elapsed >= ENTRY_TOTAL_MS;
			}), vec.end());
		}

		// 6. 把位图清空成全透明
		Graphics graphics(g_hMemDC);
		graphics.Clear(Color(180, 127, 127, 127));
		graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

		// 7. 逐条绘制
		REAL lineHeight = 0;
		REAL winW = 0;                     // 固定宽度 200（也可以测量条目中文本宽度，取最大）
		REAL winH = lineHeight * size;          // 每行 20 像素
		for (int i = 0; i < size; ++i) {
			if (!stillAlive[i]) continue;
			const auto& entry = entriesCopy[i];
			BYTE alpha = alphas[i];
			Color oldC;
			g_pTextBrush->GetColor(&oldC);
			g_pTextBrush->SetColor(Color(alphas[i], oldC.GetR(), oldC.GetG(), oldC.GetB()));
			Gdiplus::StringFormat fmt;
			RectF rect;
			graphics.MeasureString(L"W", -1, g_pFont, PointF(0, 0), &fmt, &rect);
			RectF rectP;
			graphics.MeasureString(L"+", -1, g_pFont, PointF(0, 0), &fmt, &rectP);
			lineHeight = max(lineHeight, rect.Height);
			winH = lineHeight * size;
			/*LOG(L"winH:%d\t", winH);
			winH -= (1 - alpha / 255.0) * lineHeight;
			LOG(L"winH:%d\t,alpha:%d\n", winH,alpha);*/
			REAL  textX = 0;
			REAL  textY = lineHeight * (size - 1 - i);
			for (size_t j = 0; j < entry.keys.size(); ++j) {
				const wstring& part = *entry.keys[j];
				if (j > 0) {
					graphics.DrawString(L"+", -1, g_pFont, PointF(textX, textY), g_pTextBrush);
					textX += rectP.Width;
				}

				if (part == L"Win") {
					rect = DrawWindowsLogo(graphics, textX + 3, textY + 3, lineHeight - 6, alphas[i]);
					textX += 6;
				}
				else {
					graphics.MeasureString(part.c_str(), -1, g_pFont, PointF(textX, textY), &fmt, &rect);
					graphics.DrawString(part.c_str(), -1, g_pFont, PointF(textX, textY), g_pTextBrush);
				}
				textX += rect.Width;
			}
			if (entry.mouse) {
				if (entry.keys.size()) {
					graphics.DrawString(L"+", -1, g_pFont, PointF(textX, textY), g_pTextBrush);
					textX += rectP.Width;
				}
				rect=DrawMouse(graphics, textX+3, textY+3, lineHeight - 6, entry.mouse, alphas[i]);
				textX += rect.Width+6;
			}
			winW = max(winW, textX);
#if 0

			bool hasWin = (e.text.find(L"Win") != wstring::npos);
			if (hasWin) {
				// 四色方块
				int squareSize = 12, margin = 2;
				DrawWindowsLogo(graphics, textX + 3, textY + 3, lineHeight - 6);
				// 绘制 “Win” 之后的文字，比如 “ + A”
				wstring afterWin;
				size_t pos = e.text.find(L"Win");
				if (pos != wstring::npos) {
					afterWin = e.text.substr(pos + 3);
				}
				if (!afterWin.empty()) {
					//if (afterWin.size() >= 3 && afterWin.substr(0, 3) == L" + ")
						//afterWin = afterWin.substr(3);

					Color oldC;
					g_pTextBrush->GetColor(&oldC);
					g_pTextBrush->SetColor(Color(alpha, oldC.GetR(), oldC.GetG(), oldC.GetB()));
					graphics.DrawString(
						afterWin.c_str(),
						static_cast<INT>(afterWin.length()),
						g_pFont,
						PointF(static_cast<REAL>(textX + (squareSize) * 2 + margin), static_cast<REAL>(textY)),
						g_pTextBrush
					);
				}
			}
			else {
				Color oldC;
				g_pTextBrush->GetColor(&oldC);
				g_pTextBrush->SetColor(Color(alpha, oldC.GetR(), oldC.GetG(), oldC.GetB()));
				graphics.DrawString(
					e.text.c_str(),
					static_cast<INT>(e.text.length()),
					g_pFont,
					PointF(static_cast<REAL>(textX), static_cast<REAL>(textY)),
					g_pTextBrush
				);
			}
#endif
		}

		// 4. 如果当前 DIBSection 尺寸不等于 winW×winH，就要重建
		if (winW > g_bitmapWidth || winH > g_bitmapHeight) {
			// ① 先把之前的位图从 DC 里去除、释放
			if (g_hMemBmp) {
				SelectObject(g_hMemDC, g_hMemBmp);
				DeleteObject(g_hMemBmp);
				g_hMemBmp = nullptr;
			}
			// ② 保存新尺寸
			g_bitmapWidth = winW;
			g_bitmapHeight = winH;
			// ③ 构造新的 DIBSection
			HDC screenDC = GetDC(nullptr);
			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = winW;
			bmi.bmiHeader.biHeight = -winH;    // 负号表示自上而下
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;
			void* bits = nullptr;
			g_hMemBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
			if (g_hMemBmp)SelectObject(g_hMemDC, g_hMemBmp);
			ReleaseDC(nullptr, screenDC);
		}
		// 3. 确定新窗口 / 位图的宽高
		RECT rect;
		int winL, winT;
		if (1 == g_overlayEntries.size() && duration_cast<milliseconds>(now - g_overlayEntries.front().startTime).count() < TIMER_INTERVAL_MS) {
			POINT pt;
			GetCursorPos(&pt);
			winL = pt.x + 10, winT = pt.y + 10;
		}
		else {
			GetWindowRect(g_hOverlayWnd, &rect);
			winL = rect.left, winT = rect.top;
		}

		// 5. 调整窗口本身的大小（一定要用 winW×winH）
		SetWindowPos(g_hOverlayWnd, HWND_TOPMOST, winL, winT, winW, winH, SWP_NOACTIVATE /*| SWP_NOREDRAW*/);
		ShowWindow(g_hOverlayWnd, SW_SHOW);

		// 8. 一次性 Push 到屏幕
		HDC screenDC = GetDC(nullptr);
		POINT zeroPt = { 0, 0 };
		SIZE sizeWnd = { winW, winH };
		BLENDFUNCTION blend = {};
		blend.BlendOp = AC_SRC_OVER;
		blend.SourceConstantAlpha = 255;
		blend.AlphaFormat = AC_SRC_ALPHA;

		UpdateLayeredWindow(g_hOverlayWnd, screenDC, nullptr, &sizeWnd, g_hMemDC, &zeroPt, 0, &blend, ULW_ALPHA);
		ReleaseDC(nullptr, screenDC);
	}
	void HideKeyOverlay() {
		KillTimer(g_hOverlayWnd, g_uShowTimerID);
		ShowWindow(g_hOverlayWnd, SW_HIDE);
		LOG(L"hide key overlay\n");
	}
	void ShowKeyOverlay(vector<wstring*>& keyStr, const ULONG ulButtons=0){
		if (keyStr.empty()&&!ulButtons) return;

		// 1. 拼接多个元素（超过 1 个时用 '+' 号连接）
		// 2. 构造 Entry，然后插入到全局列表
		Entry entry = { keyStr,ulButtons,steady_clock::now() };

		{
			lock_guard<mutex> lock(g_entriesMutex);
			g_overlayEntries.push_back(entry);
		}

		// 4. 创建或重置动画定时器
		// 如果定时器已存在（非首次），则先 Kill 再 Set，让动画重新开始
		KillTimer(g_hOverlayWnd, g_uShowTimerID);
		SetTimer(g_hOverlayWnd, g_uShowTimerID, TIMER_INTERVAL_MS, NULL);

		// 5. 立即强制一帧绘制（不必等到下次 WM_TIMER，立刻可见反馈）
		// 利用 PostMessage 来触发一次 WM_TIMER 也可以，这里直接调用私有函数 DrawOverlay()；
		// 由于 DrawOverlay 要访问 g_overlayEntries，请确保可见下面的实现
		PostMessageW(g_hOverlayWnd, WM_TIMER, g_uShowTimerID, 0);
	}
	void HandleMouse(const RAWMOUSE& mouse) {
		if (!g_config.stats.value&&!g_config.show.value) return;
		if ((mouse.usFlags == MOUSE_MOVE_RELATIVE || mouse.usFlags == MOUSE_MOVE_ABSOLUTE) && mouse.lLastX | mouse.lLastY)
			return;
			//LOG(L"mouse moving:dx=%ld\tdy=%ld\tdata=%d\n", mouse.lLastX, mouse.lLastY,mouse.usButtonData);
		
		struct MouseEvent {
			USHORT flag;
			const wchar_t* name;
		};
		static const MouseEvent mouseEvents[] = {
			{RI_MOUSE_LEFT_BUTTON_UP,    L"mouseL"},
			{RI_MOUSE_RIGHT_BUTTON_UP,   L"mouseR"},
			{RI_MOUSE_MIDDLE_BUTTON_UP,  L"mouseM"},
			{RI_MOUSE_BUTTON_4_UP,       L"mouseX1"},
			{RI_MOUSE_BUTTON_5_UP,       L"mouseX2"},
			{RI_MOUSE_WHEEL,             L"mouseW"},
		};

		for (const auto& evt : mouseEvents) {
			if (mouse.usButtonFlags & evt.flag) {
				/*if (evt.flag == RI_MOUSE_WHEEL) {
					SHORT delta = (SHORT)mouse.usButtonData;
					LOG(L"scrolling: %d\t%s\n", delta, delta > 0 ? L"W↑" : L"W↓");
				}
				else LOG(L"%s\n", evt.name);*/
				Record& rec = g_keyCounts[evt.flag + 0xff];
				if(rec.keyText.empty()) rec.keyText = evt.name;
				if(g_config.stats.value) count(rec);
				if (g_config.show.value) {
					vector<wstring*> keyStr;
#define ASYNC(vk,vkl) if (GetAsyncKeyState(vk) & 0x8000) keyStr.push_back(&g_keyCounts[vkl].keyText)
					ASYNC(VK_SHIFT, VK_LSHIFT);
					ASYNC(VK_CONTROL, VK_LCONTROL);
					ASYNC(VK_MENU, VK_LMENU);
#undef ASYNC
					if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000)
						keyStr.push_back(&keyPatch[VK_LWIN]);
					//keyStr.push_back(&rec.keyText);
					ShowKeyOverlay(keyStr, mouse.ulButtons);
				}
				break;  // 只处理一个事件
			}
		}
	}
	uint32_t lastKB, lastMS, g_CurrentModifiers;
	steady_clock::time_point lastTM;
	UINT cacheKey(RAWKEYBOARD& kb) {
			return kb.VKey << 4 | RI_KEY_MAKE == (kb.Flags & RI_KEY_BREAK);
	}
	UINT cacheKey(RAWMOUSE& mouse) {
		return mouse.usButtonData << 8 | mouse.usFlags << 4 | mouse.usButtonFlags;
	}
	void HandleInput(HRAWINPUT lParam) {
		UINT size = 0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		LPBYTE data = new BYTE[size];

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER)) == size) {
			RAWINPUT* raw = (RAWINPUT*)data;
			//||!IsModifierKey(raw->data.keyboard.VKey)&&raw->data.keyboard.Flags
			//Press and hold the Win key, then repeate another key
			//if (raw->header.dwType == RIM_TYPEKEYBOARD)LOG(L"VKey=%hx,\tFlags=%hu,\tMakeCode=%hu\n", raw->data.keyboard.VKey,raw->data.keyboard.Flags, raw->data.keyboard.MakeCode);
			if (raw->header.dwType == RIM_TYPEKEYBOARD && (lastKB != cacheKey(raw->data.keyboard) || !IsModifierKey(raw->data.keyboard.VKey) && raw->data.keyboard.Flags)) {
				auto& header = raw->header;
				auto& kb = raw->data.keyboard;
				//LOG(L"dwType=%lu,dwSize=0x%lx,hDevice=%p,wParam=%u\n", header.dwType, header.dwSize, (void*)header.hDevice, header.wParam);
				//RawInputKB(raw->data.keyboard);

				if (kb.VKey == 0xff) {
					LOG(L"VKey=0xff,\tFlags=%hu\n", kb.Flags);
				}
				else if (RI_KEY_MAKE == (kb.Flags & RI_KEY_BREAK)) {//presse key
					LOG(L"%ls\tdown:\tVKey=%hX,\tFlags=%hu,\tMakeCode=%hu", VKeyToString(kb).c_str(), kb.VKey, kb.Flags, kb.MakeCode);
					switch (kb.VKey) {
					case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
						g_CurrentModifiers |= MOD_CONTROL; break;
					case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
						g_CurrentModifiers |= MOD_SHIFT; break;
					case VK_MENU: case VK_LMENU: case VK_RMENU:
						g_CurrentModifiers |= MOD_ALT; break;
					case VK_LWIN: case VK_RWIN:
						g_CurrentModifiers |= MOD_WIN; break;
					}
				}
				else if (RI_KEY_BREAK == (kb.Flags & RI_KEY_BREAK)) {//release key
					if (g_config.stats.value) {
						bool isExtended = (kb.Flags & RI_KEY_E0) != 0;
						USHORT vkey = kb.VKey;
						switch (vkey) {
						case VK_SHIFT: vkey = MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX); break;
						case VK_CONTROL: vkey = isExtended ? VK_RCONTROL : VK_LCONTROL; break;
						case VK_MENU: vkey = isExtended ? VK_RMENU : VK_LMENU; break;
						}
						auto& rec = g_keyCounts[vkey];
						if (rec.keyText.empty()) rec.keyText = VKeyToString(kb);
						/*if (rec.keyText2.empty()) {
							switch (vkey) {
							case VK_LWIN: case VK_RWIN: case VK_RSHIFT: case VK_RCONTROL: case VK_RMENU:
								rec.keyText2 = VKeyToString(kb, true);
							}
						}*/
						count(rec); LOG(L"vkey=%hX,", vkey);
					}
					LOG(L"%ls%ls\tup:\t\tVKey=%hX,\tFlags=%hu,\tMakeCode=%hX", g_CurrentModifiers ? L"" : L"", VKeyToString(kb, true).c_str(), kb.VKey, kb.Flags, kb.MakeCode);
					if (g_config.show.value) {
						bool mod = IsModifierKey(kb.VKey);
						vector<wstring*> keyStr;
						if (mod) {
							if (IsModifierKey(lastKB >> 4)) {
#define MOD(mod,vk) if (g_CurrentModifiers & mod) keyStr.push_back(&g_keyCounts[vk].keyText)
								MOD(MOD_SHIFT, VK_LSHIFT);
								MOD(MOD_CONTROL, VK_LCONTROL);
								MOD(MOD_ALT, VK_LMENU);
#undef MOD
								if (g_CurrentModifiers & MOD_WIN) keyStr.push_back(&keyPatch[VK_LWIN] /*& g_keyCounts[VK_LWIN].keyText2*/);
							}
						}
						else {
#define ASYNC(vk,vkl) if (GetAsyncKeyState(vk) & 0x8000) keyStr.push_back(&g_keyCounts[vkl].keyText)
							ASYNC(VK_SHIFT, VK_LSHIFT);
							ASYNC(VK_CONTROL, VK_LCONTROL);
							ASYNC(VK_MENU, VK_LMENU);
#undef ASYNC
							if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000)
								keyStr.push_back(&keyPatch[VK_LWIN]);
							keyStr.push_back(keyPatch[kb.VKey].empty() ? &g_keyCounts[kb.VKey].keyText : &keyPatch[kb.VKey]);// VKeyToString(kb, true));
						}
						ShowKeyOverlay(keyStr);
						g_CurrentModifiers = 0;
					}
				}
				lastKB = cacheKey(raw->data.keyboard);
			}
			else if (raw->header.dwType == RIM_TYPEMOUSE && lastMS != cacheKey(raw->data.mouse) /*&& duration_cast<milliseconds>(high_resolution_clock::now() - lastTM).count() > 10*/) {
				//RawInputMS(raw->data.mouse);
				HandleMouse(raw->data.mouse);
				g_CurrentModifiers = 0;
				lastMS = cacheKey(raw->data.mouse);
				lastTM = std::chrono::high_resolution_clock::now();
			}
		}
		delete[] data;
	}
	static int g_listaryHotkey = 0;
	static HHOOK g_keyboardHook = nullptr;
	// 辅助函数：跳过空白字符
	void SkipWhitespace(const std::string& content, size_t& pos) {
		while (pos < content.size() && isspace(static_cast<unsigned char>(content[pos]))) {
			pos++;
		}
	}

	// 辅助函数：匹配特定字符串
	bool MatchString(const std::string& content, size_t& pos, const std::string& target) {
		SkipWhitespace(content, pos);
		if (content.compare(pos, target.size(), target) == 0) {
			pos += target.size();
			return true;
		}
		return false;
	}

	// 精确解析 JSON 中的 hotkey.main 值
	int ParseListaryHotkey() {
		PWSTR appDataPath = nullptr;
		if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
			LOG(L"无法获取AppData路径");
			return 0;
		}

		std::wstring path = appDataPath;
		CoTaskMemFree(appDataPath);
		path += L"\\Listary\\UserData\\Preferences.json";

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) {
			LOG(L"无法打开Listary配置文件: %s", path.c_str());
			return 0;
		}

		// 读取文件内容
		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::string content(size, '\0');
		if (!file.read(&content[0], size)) {
			LOG(L"读取文件失败: %s", path.c_str());
			return 0;
		}

		size_t pos = 0;
		int hotkeyValue = 0;
		bool found = false;

		// 使用状态机精确解析JSON
		enum ParserState { START, HOTKEY_START, HOTKEY_KEY, MAIN_START, MAIN_VALUE };
		ParserState state = START;
		int braceDepth = 0;

		while (pos < content.size()) {
			SkipWhitespace(content, pos);
			if (pos >= content.size()) break;

			char c = content[pos++];

			switch (state) {
			case START:
				if (c == '{') braceDepth++;
				else if (c == '"' && MatchString(content, pos, "hotkey"))
					state = HOTKEY_START;
				break;

			case HOTKEY_START:
				if (c == ':') {
					SkipWhitespace(content, pos);
					if (pos < content.size() && content[pos] == '{') {
						pos++;
						braceDepth++;
						state = HOTKEY_KEY;
					}
				}
				break;

			case HOTKEY_KEY:
				if (c == '}') {
					braceDepth--;
					if (braceDepth == 0) state = START; // 回到顶层
				}
				else if (c == '"' && MatchString(content, pos, "main")) state = MAIN_START;
				break;

			case MAIN_START:
				if (c == ':') state = MAIN_VALUE;
				break;

			case MAIN_VALUE:
				if (isdigit(static_cast<unsigned char>(c))) {
					// 提取完整数字
					size_t start = pos - 1;
					while (pos < content.size() &&
						isdigit(static_cast<unsigned char>(content[pos]))) {
						pos++;
					}

					try {
						hotkeyValue = std::stoi(content.substr(start, pos - start));
						found = true;
					}
					catch (...) {
						LOG(L"热键值解析失败");
					}
					return hotkeyValue; // 找到后立即返回
				}
				else if (c == '}' || c == ',') state = HOTKEY_KEY;
				break;
			}
		}

		if (!found) LOG(L"未找到hotkey.main值");
		return hotkeyValue;
	}
	static bool g_inSimulation = false; // 防止递归
	void SimulateHotkey(int hotkeyValue) {
		g_inSimulation = true;

		BYTE vk = static_cast<BYTE>((hotkeyValue >> 8) & 0xFF);
		BYTE controlFlags = static_cast<BYTE>(hotkeyValue & 0xFF);

		// 按下修饰键 (从低位到高位: Alt, Ctrl, Shift, Win)
		if (controlFlags & 0x01) keybd_event(VK_MENU, 0, 0, 0);       // Alt
		if (controlFlags & 0x02) keybd_event(VK_CONTROL, 0, 0, 0);   // Ctrl
		if (controlFlags & 0x04) keybd_event(VK_SHIFT, 0, 0, 0);     // Shift
		if (controlFlags & 0x08) keybd_event(VK_LWIN, 0, 0, 0);      // Win

		keybd_event(vk, 0, 0, 0);
		keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);

		// 释放修饰键 (从高位到低位: Win, Shift, Ctrl, Alt)
		if (controlFlags & 0x08) keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
		if (controlFlags & 0x04) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
		if (controlFlags & 0x02) keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
		if (controlFlags & 0x01) keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

		g_inSimulation = false;
	}
	// 上传统计数据到服务器（简单方式 - 直接打开浏览器）
	void UploadStatistics() {
		// 显示上传中提示
		vector<wstring*> inf{ &g_langMap[lan_viewing] };
		ShowKeyOverlay(inf);

		// 读取统计文件内容并处理
		std::wifstream file(statfile);
		if (!file.is_open()) {
			LOG(L"无法打开统计文件: %s", statfile.c_str());
			vector<wstring*> err{ &g_langMap[lan_view_failed] };
			ShowKeyOverlay(err);
			return;
		}

		// 处理数据：只需要code和count，忽略名称
		std::wstringstream processedData;
		std::wstring line;

		while (std::getline(file, line)) {
			if (line.empty()) continue;

			std::wistringstream ss(line);
			uint32_t code, count;
			std::wstring name;

			// 读取三列数据，tab分隔
			if (ss >> code >> count) {
				// 读取第三列（名称）但不使用
				std::getline(ss, name);

				// 只保存code和count
				processedData << code << L"," << count << L";";
			}
		}
		file.close();

		// 将宽字符串转换为UTF-8并进行URL编码
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::string data = converter.to_bytes(processedData.str());

		// 简单的URL编码
		std::string encodedData;
		for (char c : data) {
			if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~' || c == ',' || c == ';') {
				encodedData += c;
			}
			else {
				char buf[4];
				snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
				encodedData += buf;
			}
		}

		// 构建完整的URL
		std::wstring_convert<std::codecvt_utf8<wchar_t>> urlConverter;
		std::string baseUrl = urlConverter.to_bytes(g_config.view_url.value);
		std::string url = baseUrl + "?data=" + encodedData;

		// 转换为宽字符串
		std::wstring wideUrl(url.begin(), url.end());

		// 使用默认浏览器打开URL
		HINSTANCE result = ShellExecute(NULL, L"open", wideUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);

		if ((INT_PTR)result > 32) {
			vector<wstring*> success{ &g_langMap[lan_view_success] };
			ShowKeyOverlay(success);
			LOG(L"已打开浏览器显示统计数据");
		}
		else {
			LOG(L"打开浏览器失败: %d", (INT_PTR)result);
			vector<wstring*> err{ &g_langMap[lan_view_failed] };
			ShowKeyOverlay(err);
		}
	}
}

// 自定义窗口过程实现点击穿透
static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;  // 关键代码：标记所有区域透明
	case WM_TIMER:
		if (wParam == g_uShowTimerID) DrawOverlay();// 每一帧都重绘 Overlay
		break;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		LOG(L"FOCUS:%x", uMsg);
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}
//-------------------------------------
// 创建分层窗口并初始化 GDI+
//-------------------------------------
static void InitOverlay() {
	// GDI+ 初始化
	Gdiplus::GdiplusStartupInput gdiplusInput;
	Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr);

	// 注册 Overlay 窗口类
	WNDCLASSEX wc = { sizeof(wc) };
	wc.lpfnWndProc = OverlayWndProc;      // 不需要自定义处理
	wc.hInstance = g_hInstance;
	wc.lpszClassName = L"KeyOverlayClass";
	RegisterClassEx(&wc);

	// 创建无边框、无任务栏图标、顶层分层窗口
	g_hOverlayWnd = CreateWindowExW(
		WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
		wc.lpszClassName, L"", WS_POPUP, 0, 0, OVERLAY_W, OVERLAY_H,
		nullptr, nullptr, g_hInstance, nullptr);

	// 准备内存 DC & 32位位图
	HDC screenDC = GetDC(nullptr);
	g_hMemDC = CreateCompatibleDC(screenDC);
	BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), OVERLAY_W, -OVERLAY_H, 1, 32, BI_RGB };
	void* bits = nullptr;
	g_hMemBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (g_hMemBmp)SelectObject(g_hMemDC, g_hMemBmp);
	ReleaseDC(nullptr, screenDC);

	// “微软雅黑” 12 号，样式常规；如需其他字体、自行调整
	g_pFont = new Gdiplus::Font(L"微软雅黑", 24, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	// 纯白色文字（alpha 在绘制时动态调节）
	g_pTextBrush = new SolidBrush(Color(255, 255, 255, 255));

	ShowWindow(g_hOverlayWnd, SW_SHOW);
}
// 捕获快捷键的窗口过程
static LRESULT CALLBACK HotkeyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static UINT mod = 0;
	static UINT vk = 0;
	static wstring kn;

	switch (msg) {
	case WM_CREATE:
		g_config.show.value = false;
		SetWindowPos(hWnd, HWND_TOP,
			(GetSystemMetrics(SM_CXSCREEN) - 300) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - 100) / 2 - 100,
			300, 100, SWP_SHOWWINDOW);
		//SetTimer(hWnd, 1, 100, nullptr); // 用于刷新显示
		if (g_config.hotkey.value) {
			mod = g_config.hotkey.value >> 16;
			vk = g_config.hotkey.value & 0xffff;
		}
		else mod = vk = 0;
		LOG(L"WM_CREATE,mod:%x,vk:%u", mod, vk);
		break;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		if (g_config.hotkey.value) {
			mod = g_config.hotkey.value >> 16;
			vk = g_config.hotkey.value & 0xffff;
		}
		else mod = vk = 0;
		InvalidateRect(hWnd, NULL, TRUE);
		LOG(L"FOCUS:%x", msg);
		break;
	case WM_COMMAND:break;
		//case WM_TIMER:InvalidateRect(hWnd, NULL, TRUE);break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		vk = (UINT)wParam;
		if (IsModifierKey(vk)) {
			vk = 0;
			UINT state = 0;
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000) state |= MOD_CONTROL;
			if (GetAsyncKeyState(VK_MENU) & 0x8000)    state |= MOD_ALT;
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   state |= MOD_SHIFT;
			if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) state |= MOD_WIN;
			if (mod != state) mod = state, InvalidateRect(hWnd, NULL, TRUE);
		}
		else {
			InvalidateRect(hWnd, NULL, TRUE);
			//LOG(L"KEYDOWN:s:%ls", ss.str().c_str());
			if (vk == VK_DELETE) {
				mod = vk = 0; kn = L"";
				UnregisterHotKey(g_hMainWnd, id_hotkey);
				g_config.hotkey.value = 0;
				SaveConfig(g_config.hotkey);
				CheckMenuItem(hMenu, WM_APP_HOTKEY, MF_BYCOMMAND | MF_UNCHECKED);
				return 0;
			}
			else kn = keyName(lParam, (UINT)wParam);
		}
		break;

	case WM_KEYUP:case WM_SYSKEYUP:
		InvalidateRect(hWnd, NULL, TRUE);
		if (vk == 0) mod = vk = 0;
		else {
			if (vk == VK_DELETE) {
				UnregisterHotKey(g_hMainWnd, id_hotkey);
				mod = vk = 0; kn = L"";
			}
			else if (mod) {
				UnregisterHotKey(g_hMainWnd, id_hotkey);
				MyEnum msg = lan_fai;
				if (RegisterHotKey(g_hMainWnd, id_hotkey, mod, vk)) {
					g_config.hotkey.value = mod << 16 | vk;
					SaveConfig(g_config.hotkey);
					CheckMenuItem(hMenu, WM_APP_HOTKEY, MF_BYCOMMAND | MF_CHECKED);
					/*if (IDOK == MessageBox(NULL, L"快捷键已设置成功", L"提示", MB_OK))
						DestroyWindow(g_hHotkeyWnd);*/
					msg = lan_suc;
				}

				g_trayIcon.uFlags |= NIF_INFO;
				wcscpy_s(g_trayIcon.szInfoTitle, g_langMap[lan_hotkey_title].c_str());
				wcscpy_s(g_trayIcon.szInfo, g_langMap[msg].c_str());
				g_trayIcon.dwInfoFlags = NIIF_INFO;
				Shell_NotifyIcon(NIM_MODIFY, &g_trayIcon);
			}
			else { mod = vk = 0; kn = L""; }
		}
		break;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		RECT rc; GetClientRect(hWnd, &rc);
		HFONT hFont = CreateFontW(
			24, 0, 0, 0, FW_NORMAL,
			FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, // 抗锯齿
			DEFAULT_PITCH | FF_SWISS, L"Segoe UI");  // 更现代的字体
		SelectObject(hdc, hFont);
		RECT rc1(rc); rc1.bottom = rc.top + (rc.bottom - rc.top) / 2;
		RECT rc2(rc); rc2.top = rc.top + (rc.bottom - rc.top) / 2;
		DrawText(hdc, g_langMap[lan_hotkey_tip].c_str(), -1, &rc1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		//LOG(L"WM_PAINT:mod:%x,vk:%u", mod, vk);
		std::wstringstream ss;
		if (mod & MOD_CONTROL) ss << keyName(0, VK_CONTROL) << L"+";
		if (mod & MOD_ALT) ss << keyName(0, VK_MENU) << L"+";
		if (mod & MOD_SHIFT) ss << keyName(0, VK_SHIFT) << L"+";
		if (mod & MOD_WIN) ss << keyName(0, VK_LWIN) << L"+";
		if (vk && kn.empty()) {
			WCHAR keyName[64] = {};
			GetKeyNameText(MapVirtualKey(vk, MAPVK_VK_TO_VSC) << 16, keyName, 64);
			ss << keyName;
		}
		if (mod && !kn.empty())ss << kn;
		//LOG(L"WM_PAINT:ss:%s", ss.str().c_str());
		DrawText(hdc, ss.str().c_str(), -1, &rc2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		DeleteObject(hFont);
		EndPaint(hWnd, &ps);
		break;
	}

	case WM_DESTROY:
		LoadConfig(g_config.show);
		g_hHotkeyWnd = nullptr;
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
// 弹出快捷键录入窗口（避免重复）
static void ShowHotkeyInputDialog(HWND parent) {
	if (g_hHotkeyWnd && IsWindow(g_hHotkeyWnd)) { SetForegroundWindow(g_hHotkeyWnd); return; }

	wstring classname = wstring(APP_NAME) + L"CaptureClass";
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = HotkeyWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = classname.c_str();
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClass(&wc);

	g_hHotkeyWnd = CreateWindowExW(WS_EX_TOPMOST,
		wc.lpszClassName, g_langMap[lan_hotkey_title].c_str(),
		WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		300, 100, parent, NULL, wc.hInstance, NULL);

	SetForegroundWindow(g_hHotkeyWnd);
}
static void HandleMenu(MyEnum id, HWND hWnd) {
	switch (id) {
	case WM_APP_ONBOOT: {
		toggle(id);
		MyEnum cmd = g_config.boot.value ? cmd_create : cmd_del;
		if (RunAsAdmin(to_wstring(cmd).c_str()))SaveConfig(g_config.boot);
		else toggle(id);
		break;
	}
	case WM_APP_STAT:case WM_APP_SHOW:
		toggle(id);
		SaveConfig(*menuMap[id]);
		break;
	case WM_APP_HOTKEY:
		ShowHotkeyInputDialog(g_hMainWnd);
		break;
	case WM_APP_VIEW:
	std::thread(UploadStatistics).detach();
	break;
	case WM_APP_EXIT:DestroyWindow(hWnd); break;
	}
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_CREATE:
		initPath();
		LoadConfig();
		LoadLan();
		initIcon(hWnd);
		initConfigFile(configfile);
		LoadStatistics();
		initMenu();
		onboot();
		RegisterRawInput(hWnd);
		InitOverlay();
#ifdef _DEBUG
#else
		checklog();
#endif
		break;
	case WM_HOTKEY:
		if (wParam == id_hotkey) {
			toggle(WM_APP_SHOW);
			MyEnum enable = g_config.show.value ? lan_enable : lan_disable;
			vector<wstring*> inf{ &g_langMap[enable]};
			ShowKeyOverlay(inf);
			SaveConfig();
		}
		break;
	case WM_COMMAND:
		HandleMenu((MyEnum)LOWORD(wParam), hWnd);
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	case WM_INPUT:
		if (!g_config.stats.value && !g_config.show.value) break;
		HandleInput((HRAWINPUT)lParam);
		break;
	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		LOG(L"FOCUS:%x", message); break;
	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &g_trayIcon);
		if (g_keyboardHook) {
			if (!UnhookWindowsHookEx(g_keyboardHook)) LOG(L"卸载键盘钩子失败: %d", GetLastError());
			g_keyboardHook = nullptr;
		}
		PostQuitMessage(0);
		break;
	case WM_APP_MENU:
		switch (lParam)
		{
		case WM_MOUSEMOVE:UpdateTrayTooltip(); break;
		case NIN_BALLOONSHOW:case NIN_BALLOONHIDE:case NIN_BALLOONTIMEOUT:case NIN_BALLOONUSERCLICK:
		case NIN_POPUPOPEN:case NIN_POPUPCLOSE:
			break;
		default:ShowContextMenu(hWnd); break;
		}
		break;
	case WM_APP_TOOLTIP: {
		vector<wstring*> inf{ &g_langMap[lan_dup] };
		ShowKeyOverlay(inf);
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode < 0)
		return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);

	KBDLLHOOKSTRUCT* pKB = (KBDLLHOOKSTRUCT*)lParam;

	// 忽略注入事件和递归保护
	if ((pKB->flags & LLKHF_INJECTED) || g_inSimulation)
		return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);

	// 只处理左Win键按下事件
	static DWORD lastHK;
	if (wParam == WM_KEYDOWN)lastHK = pKB->vkCode;
	if (wParam == WM_KEYUP && pKB->vkCode == VK_LWIN && lastHK == VK_LWIN) {
		SimulateHotkey(g_listaryHotkey);
		return 1; // 阻止原始按键传递
	}

	return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}
INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR cmd, _In_ INT) {
	HandleTaskArguments(cmd); // 如果以管理员方式运行创建/删除任务，直接执行
	HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		// 查找主窗口并发送激活消息
		HWND hMainWnd = FindWindow(APP_NAME, NULL);
		if (hMainWnd) PostMessage(hMainWnd, WM_APP_TOOLTIP, 0, 0);
		if (hMutex)CloseHandle(hMutex);
		return 0;
	}
	// 注册窗口类
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = APP_NAME;
	RegisterClass(&wc);
	// 创建隐藏窗口
	g_hMainWnd = CreateWindow(APP_NAME, NULL, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	uint32_t hk = g_config.hotkey.value;
	if (hk)RegisterHotKey(g_hMainWnd, id_hotkey, hk >> 16, hk & 0xffff);
	PostMessage(g_hMainWnd, WM_NULL, 0, 0);
	if (g_listaryHotkey = ParseListaryHotkey())
		g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
