#pragma once
#include <string>

struct lua_State;

class LuauEnvironment {
public:
    static void InitLuaFunctions(void* hRoblox);
    static void Register(lua_State* L);

    // Luau C Function Callbacks
    static int Request(lua_State* L);
    static int HttpGet(lua_State* L);
    static int HttpPost(lua_State* L);

    static int MessageBoxLua(lua_State* L);
    static int SetClipboard(lua_State* L);
    static int GetClipboard(lua_State* L);

    static int RConsolePrint(lua_State* L);
    static int RConsoleClear(lua_State* L);
    static int RConsoleName(lua_State* L);
    static int RConsoleInput(lua_State* L);

    static int Decompile(lua_State* L);

    // Filesystem
    static int ReadFile(lua_State* L);
    static int WriteFile(lua_State* L);
    static int AppendFile(lua_State* L);
    static int LoadFile(lua_State* L);
    static int ListFiles(lua_State* L);
    static int IsFile(lua_State* L);
    static int IsFolder(lua_State* L);
    static int MakeFolder(lua_State* L);
    static int DelFile(lua_State* L);
    static int DelFolder(lua_State* L);

    // Crypt
    static int Base64Encode(lua_State* L);
    static int Base64Decode(lua_State* L);

    // WebSocket
    static int WebSocketConnect(lua_State* L);

    // Drawing
    static int DrawingNew(lua_State* L);
    static int DrawingClear(lua_State* L);
};
