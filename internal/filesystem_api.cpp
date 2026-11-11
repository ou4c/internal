#include "filesystem_api.h"
#include <fstream>
#include <sstream>

fs::path FilesystemAPI::s_WorkspacePath = "";
fs::path FilesystemAPI::s_AutoexecPath = "";

bool FilesystemAPI::Initialize() {
    char localAppData[MAX_PATH] = {0};
    DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return false;

    fs::path basePath = fs::path(localAppData) / "Potassium";
    s_WorkspacePath = basePath / "workspace";
    s_AutoexecPath = basePath / "autoexec";

    std::error_code ec;
    fs::create_directories(s_WorkspacePath, ec);
    fs::create_directories(s_AutoexecPath, ec);
    return true;
}

fs::path FilesystemAPI::GetWorkspacePath() {
    if (s_WorkspacePath.empty())
        Initialize();
    return s_WorkspacePath;
}

fs::path FilesystemAPI::GetAutoexecPath() {
    if (s_AutoexecPath.empty())
        Initialize();
    return s_AutoexecPath;
}

fs::path FilesystemAPI::SafeResolveWorkspacePath(const std::string& relativePath) {
    fs::path base = GetWorkspacePath();
    fs::path target = base / relativePath;
    // Normalize and prevent directory traversal outside workspace
    std::error_code ec;
    target = fs::weakly_canonical(target, ec);
    std::string baseStr = base.string();
    std::string targetStr = target.string();
    if (targetStr.compare(0, baseStr.length(), baseStr) != 0) {
        return ""; // Escaped workspace
    }
    return target;
}

std::string FilesystemAPI::ReadFile(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty() || !fs::is_regular_file(resolved))
        return "";

    std::ifstream file(resolved, std::ios::binary);
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FilesystemAPI::WriteFile(const std::string& relativePath, const std::string& content) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty())
        return false;

    fs::path parent = resolved.parent_path();
    std::error_code ec;
    if (!parent.empty())
        fs::create_directories(parent, ec);

    std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;

    file.write(content.data(), content.size());
    return file.good();
}

bool FilesystemAPI::AppendFile(const std::string& relativePath, const std::string& content) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty())
        return false;

    std::ofstream file(resolved, std::ios::binary | std::ios::app);
    if (!file.is_open())
        return false;

    file.write(content.data(), content.size());
    return file.good();
}

bool FilesystemAPI::IsFile(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    return !resolved.empty() && fs::is_regular_file(resolved);
}

bool FilesystemAPI::IsFolder(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    return !resolved.empty() && fs::is_directory(resolved);
}

bool FilesystemAPI::MakeFolder(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty())
        return false;
    std::error_code ec;
    return fs::create_directories(resolved, ec);
}

bool FilesystemAPI::DeleteFile(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty() || !fs::is_regular_file(resolved))
        return false;
    std::error_code ec;
    return fs::remove(resolved, ec);
}

bool FilesystemAPI::DeleteFolder(const std::string& relativePath) {
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty() || !fs::is_directory(resolved))
        return false;
    std::error_code ec;
    return fs::remove_all(resolved, ec) > 0;
}

std::vector<std::string> FilesystemAPI::ListFiles(const std::string& relativePath) {
    std::vector<std::string> results;
    fs::path resolved = SafeResolveWorkspacePath(relativePath);
    if (resolved.empty() || !fs::is_directory(resolved))
        return results;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(resolved, ec)) {
        results.push_back(entry.path().filename().string());
    }
    return results;
}
