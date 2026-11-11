#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class FilesystemAPI {
public:
    static bool Initialize();
    static fs::path GetWorkspacePath();
    static fs::path GetAutoexecPath();
    static fs::path SafeResolveWorkspacePath(const std::string& relativePath);

    static std::string ReadFile(const std::string& relativePath);
    static bool WriteFile(const std::string& relativePath, const std::string& content);
    static bool AppendFile(const std::string& relativePath, const std::string& content);
    static bool IsFile(const std::string& relativePath);
    static bool IsFolder(const std::string& relativePath);
    static bool MakeFolder(const std::string& relativePath);
    static bool DeleteFile(const std::string& relativePath);
    static bool DeleteFolder(const std::string& relativePath);
    static std::vector<std::string> ListFiles(const std::string& relativePath);

private:
    static fs::path s_WorkspacePath;
    static fs::path s_AutoexecPath;
};
