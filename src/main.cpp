//
// Created by Gegel85 on 04/12/2020
//

#include <algorithm>
#include <SokuLib.hpp>
#include <windows.h>
#include <Shlwapi.h>
#include <dinput.h>
#include "Utils/InputBox.hpp"
#include "Exceptions.hpp"
#include "Network/Handlers.hpp"
#include "State.hpp"
#include "DeprecatedElements.hpp"
#include "package.hpp"

namespace ShadyCore {
	void* Allocate(size_t s) { return SokuLib::NewFct(s); }
	void Deallocate(void* p) { SokuLib::DeleteFct(p); }
}

ShadyCore::PackageEx *package = nullptr;

wchar_t soku2Path[1024 + MAX_PATH];
char profilePath[1024 + MAX_PATH];
char parentPath[1024 + MAX_PATH];

int __fastcall CTitle_OnProcess(Title *This) {
	// super
	int ret = (This->*s_origCTitle_Process)();

	needReset = true;
	needRefresh = true;
	checkKeyInputs();
	return ret;
}

int __fastcall CBattleWatch_OnProcess(BattleWatch *This) {
	// super
	int ret = (This->*s_origCBattleWatch_Process)();

	updateCache(true);
	return ret;
}

int __fastcall CBattle_OnProcess(Battle *This) {
	// super
	int ret = (This->*s_origCBattle_Process)();

	if (SokuLib::mainMode == SokuLib::BATTLE_MODE_VSPLAYER)
		updateCache(false);
	return ret;
}

void loadCommon()
{
	needRefresh = true;
	checkKeyInputs();
}

int __fastcall CLoading_OnProcess(Loading *This) {
	// super
	int ret = (This->*s_origCLoading_Process)();

	loadCommon();
	return ret;
}

int __fastcall CLoadingWatch_OnProcess(LoadingWatch *This) {
	// super
	int ret = (This->*s_origCLoadingWatch_Process)();

	loadCommon();
	return ret;
}

int __fastcall CBattleManager_KO(SokuLib::BattleManager *This) {
	// super
	int ret = (This->*s_origCBattleManager_KO)();

	onKO();
	return ret;
}

int __fastcall CBattleManager_Start(SokuLib::BattleManager *This) {
	// super
	int ret = (This->*s_origCBattleManager_Start)();

	onRoundStart();
	return ret;
}

int __fastcall CBattleManager_Render(SokuLib::BattleManager *This) {
	// super
	int ret = (This->*s_origCBattleManager_Render)();

	checkKeyInputs();
	return ret;
}

void loadSoku2Config()
{
	puts("Looking for Soku2 config...");

	int argc;
	wchar_t app_path[MAX_PATH];
	wchar_t setting_path[MAX_PATH];
	wchar_t **arg_list = CommandLineToArgvW(GetCommandLineW(), &argc);

	wcsncpy(app_path, arg_list[0], MAX_PATH);
	PathRemoveFileSpecW(app_path);
	if (GetEnvironmentVariableW(L"SWRSTOYS", setting_path, sizeof(setting_path)) <= 0) {
		if (arg_list && argc > 1 && StrStrIW(arg_list[1], L"ini")) {
			wcscpy(setting_path, arg_list[1]);
			LocalFree(arg_list);
		} else {
			wcscpy(setting_path, app_path);
			PathAppendW(setting_path, L"\\SWRSToys.ini");
		}
		if (arg_list) {
			LocalFree(arg_list);
		}
	}
	printf("Config file is %S\n", setting_path);

	wchar_t moduleKeys[1024];
	wchar_t moduleValue[MAX_PATH];
	GetPrivateProfileStringW(L"Module", nullptr, nullptr, moduleKeys, sizeof(moduleKeys), setting_path);
	for (wchar_t *key = moduleKeys; *key; key += wcslen(key) + 1) {
		wchar_t module_path[MAX_PATH];

		GetPrivateProfileStringW(L"Module", key, nullptr, moduleValue, sizeof(moduleValue), setting_path);

		wchar_t *filename = wcsrchr(moduleValue, '/');

		printf("Check %S\n", moduleValue);
		if (!filename)
			filename = app_path;
		else
			filename++;
		for (int i = 0; filename[i]; i++)
			filename[i] = tolower(filename[i]);
		if (wcscmp(filename, L"soku2.dll") != 0)
			continue;

		wcscpy(module_path, app_path);
		PathAppendW(module_path, moduleValue);
		while (auto result = wcschr(module_path, '/'))
			*result = '\\';
		PathRemoveFileSpecW(module_path);
		printf("Found Soku2 module folder at %S\n", module_path);
		PathAppendW(module_path, L"\\config\\info");
		wcscpy(soku2Path, module_path);
		return;
	}
}

// �ݒ胍�[�h
void LoadSettings()
{
	// �����V���b�g�_�E��
	enabled = GetPrivateProfileInt("SokuStreaming", "Enabled", 1, profilePath) != 0;
	if (!enabled)
		return;

	/*FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);*/
	port = GetPrivateProfileInt("Server", "Port", 80, profilePath);
	keys[KEY_DECREASE_L_SCORE] = GetPrivateProfileInt("Keys", "DecreaseLeftScore",  '1', profilePath);
	keys[KEY_INCREASE_L_SCORE] = GetPrivateProfileInt("Keys", "IncreaseLeftScore",  '2', profilePath);
	keys[KEY_CHANGE_L_NAME]    = GetPrivateProfileInt("Keys", "ChangeLeftName",     '3', profilePath);
	keys[KEY_RESET_SCORES]     = GetPrivateProfileInt("Keys", "ResetScores",        '5', profilePath);
	keys[KEY_RESET_STATE]      = GetPrivateProfileInt("Keys", "ResetState",         '6', profilePath);
	keys[KEY_DECREASE_R_SCORE] = GetPrivateProfileInt("Keys", "DecreaseRightScore", '8', profilePath);
	keys[KEY_INCREASE_R_SCORE] = GetPrivateProfileInt("Keys", "IncreaseRightScore", '9', profilePath);
	keys[KEY_CHANGE_R_NAME]    = GetPrivateProfileInt("Keys", "ChangeRightName",    '0', profilePath);

	loadSoku2Config();

	webServer = std::make_unique<WebServer>(GetPrivateProfileIntA("Server", "Cache", 0, profilePath));
	webServer->addRoute("^/$", root);
	webServer->addRoute("^/state$", state);
	webServer->addRoute("^/connectRoute$", connectRoute);
	webServer->addRoute("^/charName/\\d+$", getCharName);
	webServer->addRoute("^/internal(/.*)?$", loadInternalAsset);
	webServer->addRoute("^/skillSheet/\\d+$", loadSkillSheet);
	webServer->addStaticFolder("/static", std::string(parentPath) + "/static");
	webServer->start(port);
	webServer->onWebSocketConnect(onNewWebSocket);
}

void __fastcall buildDatList(const char *path)
{
	auto old = package;

	package = new ShadyCore::PackageEx();
	package->merge(new ShadyCore::Package(path));
	if (old)
		package->merge(old);
}

void __declspec(naked) buildDatList_hook()
{
	__asm {
		PUSH ECX
		MOV ECX, [ESP + 12]
		CALL buildDatList
		POP ECX
		RET
	}
}

void hookFunctions()
{
	DWORD old;

	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	s_origCTitle_Process = SokuLib::union_cast<int (Title::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CTitle + SokuLib::OFFSET_ON_PROCESS,
			reinterpret_cast<DWORD>(CTitle_OnProcess)
		)
	);
	s_origCBattleWatch_Process = SokuLib::union_cast<int (BattleWatch::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CBattleWatch + SokuLib::OFFSET_ON_PROCESS,
			reinterpret_cast<DWORD>(CBattleWatch_OnProcess)
		)
	);
	s_origCLoadingWatch_Process = SokuLib::union_cast<int (LoadingWatch::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CLoadingWatch + SokuLib::OFFSET_ON_PROCESS,
			reinterpret_cast<DWORD>(CLoadingWatch_OnProcess)
		)
	);
	s_origCBattle_Process = SokuLib::union_cast<int (Battle::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CBattle + SokuLib::OFFSET_ON_PROCESS,
			reinterpret_cast<DWORD>(CBattle_OnProcess)
		)
	);
	s_origCLoading_Process = SokuLib::union_cast<int (Loading::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CLoading + SokuLib::OFFSET_ON_PROCESS,
			reinterpret_cast<DWORD>(CLoading_OnProcess)
		)
	);
	s_origCBattleManager_Start = SokuLib::union_cast<int (SokuLib::BattleManager::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CBattleManager + SokuLib::BATTLE_MGR_OFFSET_ON_SAY_START,
			reinterpret_cast<DWORD>(CBattleManager_Start)
		)
	);
	s_origCBattleManager_KO = SokuLib::union_cast<int (SokuLib::BattleManager::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CBattleManager + SokuLib::BATTLE_MGR_OFFSET_ON_KO,
			reinterpret_cast<DWORD>(CBattleManager_KO)
		)
	);
	/*s_origCBattleManager_Render = SokuLib::union_cast<int (SokuLib::BattleManager::*)()>(
		SokuLib::TamperDword(
			SokuLib::vtbl_CBattleManager + SokuLib::BATTLE_MGR_OFFSET_ON_RENDER,
			reinterpret_cast<DWORD>(CBattleManager_Render)
		)
	);*/
	::VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	SokuLib::TamperNearCall(0x40D1D4, buildDatList_hook);
	SokuLib::TamperNearJmp(0x40D1D9, 0x41BB50);
	::VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);
	::FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}

extern "C"
__declspec(dllexport) bool CheckVersion(const BYTE hash[16])
{
	return true;
}

extern "C"
__declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule)
{
	try {
		GetModuleFileName(hMyModule, profilePath, 1024);
		PathRemoveFileSpec(profilePath);
		strcpy(parentPath, profilePath);
		PathAppend(profilePath, "SokuStreaming.ini");
		LoadSettings();

		if (!enabled)
			return true;
		hookFunctions();
	} catch (std::exception &e) {
		MessageBoxA(nullptr, e.what(), "Cannot init SokuStreaming", MB_OK | MB_ICONERROR);
	} catch (...) {
		MessageBoxA(nullptr, "Wtf ?", "Huh... ok", MB_OK | MB_ICONERROR);
		abort();
	}
	return true;
}

extern "C"
int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	return TRUE;
}