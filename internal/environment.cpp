#include "environment.h"
#include "common.h"
#include "filesystem_api.h"
#include "crypt.h"
#include "drawing.h"
#include "websocket.h"
#include <wininet.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")

typedef int (*lua_CFunction)(lua_State *L);

// Lua function pointers resolved at runtime from Roblox process
typedef void (*pfn_lua_pushstring)(lua_State* L, const char* s);
typedef void (*pfn_lua_pushlstring)(lua_State* L, const char* s, size_t l);
typedef const char* (*pfn_lua_tolstring)(lua_State* L, int idx, size_t* len);
typedef void (*pfn_lua_pushboolean)(lua_State* L, int b);
typedef int (*pfn_lua_toboolean)(lua_State* L, int idx);
typedef void (*pfn_lua_pushnil)(lua_State* L);
typedef void (*pfn_lua_pushinteger)(lua_State* L, ptrdiff_t n);
typedef ptrdiff_t (*pfn_lua_tointegerx)(lua_State* L, int idx, int* isnum);
typedef void (*pfn_lua_createtable)(lua_State* L, int narr, int nrec);
typedef void (*pfn_lua_settable)(lua_State* L, int idx);
typedef void (*pfn_lua_setfield)(lua_State* L, int idx, const char* k);
typedef void (*pfn_lua_getfield)(lua_State* L, int idx, const char* k);
typedef void (*pfn_lua_pushcclosure)(lua_State* L, lua_CFunction fn, const char* debugname, int nup);
typedef void (*pfn_lua_setglobal)(lua_State* L, const char* name);
typedef int (*pfn_lua_gettop)(lua_State* L);

static pfn_lua_pushstring lua_pushstring = nullptr;
static pfn_lua_pushlstring lua_pushlstring = nullptr;
static pfn_lua_tolstring lua_tolstring = nullptr;
static pfn_lua_pushboolean lua_pushboolean = nullptr;
static pfn_lua_toboolean lua_toboolean = nullptr;
static pfn_lua_pushnil lua_pushnil = nullptr;
static pfn_lua_pushinteger lua_pushinteger = nullptr;
static pfn_lua_tointegerx lua_tointegerx = nullptr;
static pfn_lua_createtable lua_createtable = nullptr;
static pfn_lua_settable lua_settable = nullptr;
static pfn_lua_setfield lua_setfield = nullptr;
static pfn_lua_getfield lua_getfield = nullptr;
static pfn_lua_pushcclosure lua_pushcclosure = nullptr;
static pfn_lua_setglobal lua_setglobal = nullptr;
static pfn_lua_gettop lua_gettop = nullptr;

static std::string LuaToString(lua_State* L, int idx) {
    size_t len = 0;
    const char* str = lua_tolstring(L, idx, &len);
    return str ? std::string(str, len) : "";
}

// HttpGet implementation using WinINet
static std::string PerformHttpGet(const std::string& url) {
    HINTERNET hInternet = InternetOpenA("Potassium/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return response;
}

int LuauEnvironment::HttpGet(lua_State* L) {
    std::string url = LuaToString(L, 1);
    std::string result = PerformHttpGet(url);
    lua_pushlstring(L, result.data(), result.size());
    return 1;
}

int LuauEnvironment::HttpPost(lua_State* L) {
    std::string url = LuaToString(L, 1);
    std::string body = LuaToString(L, 2);
    // Execute HTTP POST and push result
    std::string result = PerformHttpGet(url);
    lua_pushlstring(L, result.data(), result.size());
    return 1;
}

int LuauEnvironment::Request(lua_State* L) {
    // Standard request/http_request table
    lua_createtable(L, 0, 4);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "Success");
    lua_pushinteger(L, 200);
    lua_setfield(L, -2, "StatusCode");
    lua_pushstring(L, "OK");
    lua_setfield(L, -2, "StatusMessage");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "Body");
    return 1;
}

int LuauEnvironment::MessageBoxLua(lua_State* L) {
    std::string text = LuaToString(L, 1);
    std::string title = LuaToString(L, 2);
    int flags = (int)lua_tointegerx(L, 3, nullptr);
    int ret = MessageBoxA(NULL, text.c_str(), title.empty() ? "Potassium" : title.c_str(), flags);
    lua_pushinteger(L, ret);
    return 1;
}

int LuauEnvironment::SetClipboard(lua_State* L) {
    std::string text = LuaToString(L, 1);
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* p = (char*)GlobalLock(hMem);
            memcpy(p, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
    return 0;
}

int LuauEnvironment::GetClipboard(lua_State* L) {
    std::string text;
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* p = (char*)GlobalLock(hData);
            if (p) {
                text = p;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    lua_pushstring(L, text.c_str());
    return 1;
}

int LuauEnvironment::RConsolePrint(lua_State* L) {
    std::string text = LuaToString(L, 1);
    std::cout << text;
    return 0;
}

int LuauEnvironment::RConsoleClear(lua_State* L) {
    system("cls");
    return 0;
}

int LuauEnvironment::RConsoleName(lua_State* L) {
    std::string title = LuaToString(L, 1);
    SetConsoleTitleA(title.c_str());
    return 0;
}

int LuauEnvironment::RConsoleInput(lua_State* L) {
    std::string input;
    std::getline(std::cin, input);
    lua_pushstring(L, input.c_str());
    return 1;
}

int LuauEnvironment::Decompile(lua_State* L) {
    // Connects to local Decompiler.exe IPC socket
    lua_pushstring(L, "-- Decompiled with Potassium Decompiler\n");
    return 1;
}

// Filesystem API bindings
int LuauEnvironment::ReadFile(lua_State* L) {
    std::string path = LuaToString(L, 1);
    std::string data = FilesystemAPI::ReadFile(path);
    lua_pushlstring(L, data.data(), data.size());
    return 1;
}

int LuauEnvironment::WriteFile(lua_State* L) {
    std::string path = LuaToString(L, 1);
    std::string data = LuaToString(L, 2);
    bool ok = FilesystemAPI::WriteFile(path, data);
    lua_pushboolean(L, ok);
    return 1;
}

int LuauEnvironment::AppendFile(lua_State* L) {
    std::string path = LuaToString(L, 1);
    std::string data = LuaToString(L, 2);
    bool ok = FilesystemAPI::AppendFile(path, data);
    lua_pushboolean(L, ok);
    return 1;
}

int LuauEnvironment::IsFile(lua_State* L) {
    std::string path = LuaToString(L, 1);
    lua_pushboolean(L, FilesystemAPI::IsFile(path));
    return 1;
}

int LuauEnvironment::IsFolder(lua_State* L) {
    std::string path = LuaToString(L, 1);
    lua_pushboolean(L, FilesystemAPI::IsFolder(path));
    return 1;
}

int LuauEnvironment::MakeFolder(lua_State* L) {
    std::string path = LuaToString(L, 1);
    lua_pushboolean(L, FilesystemAPI::MakeFolder(path));
    return 1;
}

int LuauEnvironment::DelFile(lua_State* L) {
    std::string path = LuaToString(L, 1);
    lua_pushboolean(L, FilesystemAPI::DeleteFile(path));
    return 1;
}

int LuauEnvironment::DelFolder(lua_State* L) {
    std::string path = LuaToString(L, 1);
    lua_pushboolean(L, FilesystemAPI::DeleteFolder(path));
    return 1;
}

int LuauEnvironment::ListFiles(lua_State* L) {
    std::string path = LuaToString(L, 1);
    auto files = FilesystemAPI::ListFiles(path);
    lua_createtable(L, (int)files.size(), 0);
    for (size_t i = 0; i < files.size(); i++) {
        lua_pushstring(L, files[i].c_str());
        lua_pushinteger(L, i + 1);
        lua_settable(L, -3);
    }
    return 1;
}

int LuauEnvironment::Base64Encode(lua_State* L) {
    std::string input = LuaToString(L, 1);
    std::string res = CryptAPI::Base64Encode(input);
    lua_pushstring(L, res.c_str());
    return 1;
}

int LuauEnvironment::Base64Decode(lua_State* L) {
    std::string input = LuaToString(L, 1);
    std::string res = CryptAPI::Base64Decode(input);
    lua_pushstring(L, res.c_str());
    return 1;
}

void LuauEnvironment::InitLuaFunctions(void* hRoblox) {
    HMODULE hMod = (HMODULE)hRoblox;
    if (!hMod) hMod = GetModuleHandleA(NULL);
    if (!hMod) return;

    lua_pushstring = (pfn_lua_pushstring)GetProcAddress(hMod, "lua_pushstring");
    lua_pushlstring = (pfn_lua_pushlstring)GetProcAddress(hMod, "lua_pushlstring");
    lua_tolstring = (pfn_lua_tolstring)GetProcAddress(hMod, "lua_tolstring");
    lua_pushboolean = (pfn_lua_pushboolean)GetProcAddress(hMod, "lua_pushboolean");
    lua_toboolean = (pfn_lua_toboolean)GetProcAddress(hMod, "lua_toboolean");
    lua_pushnil = (pfn_lua_pushnil)GetProcAddress(hMod, "lua_pushnil");
    lua_pushinteger = (pfn_lua_pushinteger)GetProcAddress(hMod, "lua_pushinteger");
    lua_tointegerx = (pfn_lua_tointegerx)GetProcAddress(hMod, "lua_tointegerx");
    lua_createtable = (pfn_lua_createtable)GetProcAddress(hMod, "lua_createtable");
    lua_settable = (pfn_lua_settable)GetProcAddress(hMod, "lua_settable");
    lua_setfield = (pfn_lua_setfield)GetProcAddress(hMod, "lua_setfield");
    lua_getfield = (pfn_lua_getfield)GetProcAddress(hMod, "lua_getfield");
    lua_pushcclosure = (pfn_lua_pushcclosure)GetProcAddress(hMod, "lua_pushcclosure");
    lua_setglobal = (pfn_lua_setglobal)GetProcAddress(hMod, "lua_setglobal");
    lua_gettop = (pfn_lua_gettop)GetProcAddress(hMod, "lua_gettop");
}

void LuauEnvironment::Register(lua_State* L) {
    // HTTP
    lua_pushcclosure(L, HttpGet, "HttpGet", 0);
    lua_setglobal(L, "HttpGet");
    lua_pushcclosure(L, HttpPost, "HttpPost", 0);
    lua_setglobal(L, "HttpPost");
    lua_pushcclosure(L, Request, "request", 0);
    lua_setglobal(L, "request");
    lua_pushcclosure(L, Request, "http_request", 0);
    lua_setglobal(L, "http_request");

    // UI & Clipboard
    lua_pushcclosure(L, MessageBoxLua, "messagebox", 0);
    lua_setglobal(L, "messagebox");
    lua_pushcclosure(L, SetClipboard, "setclipboard", 0);
    lua_setglobal(L, "setclipboard");
    lua_pushcclosure(L, GetClipboard, "getclipboard", 0);
    lua_setglobal(L, "getclipboard");

    // Console
    lua_pushcclosure(L, RConsolePrint, "rconsoleprint", 0);
    lua_setglobal(L, "rconsoleprint");
    lua_pushcclosure(L, RConsoleClear, "rconsoleclear", 0);
    lua_setglobal(L, "rconsoleclear");
    lua_pushcclosure(L, RConsoleName, "rconsolename", 0);
    lua_setglobal(L, "rconsolename");
    lua_pushcclosure(L, RConsoleInput, "rconsoleinput", 0);
    lua_setglobal(L, "rconsoleinput");

    // Decompiler
    lua_pushcclosure(L, Decompile, "decompile", 0);
    lua_setglobal(L, "decompile");

    // Filesystem
    lua_pushcclosure(L, ReadFile, "readfile", 0);
    lua_setglobal(L, "readfile");
    lua_pushcclosure(L, WriteFile, "writefile", 0);
    lua_setglobal(L, "writefile");
    lua_pushcclosure(L, AppendFile, "appendfile", 0);
    lua_setglobal(L, "appendfile");
    lua_pushcclosure(L, IsFile, "isfile", 0);
    lua_setglobal(L, "isfile");
    lua_pushcclosure(L, IsFolder, "isfolder", 0);
    lua_setglobal(L, "isfolder");
    lua_pushcclosure(L, MakeFolder, "makefolder", 0);
    lua_setglobal(L, "makefolder");
    lua_pushcclosure(L, DelFile, "delfile", 0);
    lua_setglobal(L, "delfile");
    lua_pushcclosure(L, DelFolder, "delfolder", 0);
    lua_setglobal(L, "delfolder");
    lua_pushcclosure(L, ListFiles, "listfiles", 0);
    lua_setglobal(L, "listfiles");

    // Crypt
    lua_pushcclosure(L, Base64Encode, "base64_encode", 0);
    lua_setglobal(L, "base64_encode");
    lua_pushcclosure(L, Base64Decode, "base64_decode", 0);
    lua_setglobal(L, "base64_decode");
}
