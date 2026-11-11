#include "util.h"

#include <cstdio>

#include <windows.h>

namespace lr {

std::wstring toWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

bool ensureDir(const std::string& dir) {
    std::wstring w = toWide(dir);
    while (!w.empty() && (w.back() == L'\\' || w.back() == L'/')) w.pop_back();
    if (w.empty()) return false;
    DWORD a = GetFileAttributesW(w.c_str());
    if (a != INVALID_FILE_ATTRIBUTES) return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    size_t slash = w.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 2) ensureDir(toUtf8(w.substr(0, slash)));
    return CreateDirectoryW(w.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool readFile(const std::string& path, std::string& out) {
    out.clear();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, toWide(path).c_str(), L"rb") != 0 || !fp) return false;
    char buf[16384];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, fp)) > 0) {
        out.append(buf, n);
        if (out.size() > 32u * 1024 * 1024) break;
    }
    std::fclose(fp);
    return true;
}

bool writeFileAtomic(const std::string& path, const std::string& data) {
    std::wstring w = toWide(path), tmp = w + L".tmp";
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, tmp.c_str(), L"wb") != 0 || !fp) return false;
    size_t wrote = std::fwrite(data.data(), 1, data.size(), fp);
    bool ok = std::fclose(fp) == 0 && wrote == data.size();
    if (ok) ok = MoveFileExW(tmp.c_str(), w.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(tmp.c_str());
    return ok;
}

void deleteFile(const std::string& path) { DeleteFileW(toWide(path).c_str()); }

std::vector<std::string> listFiles(const std::string& dir, const char* pattern) {
    std::vector<std::string> out;
    std::string d = dir;
    if (!d.empty() && d.back() != '\\') d += '\\';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(toWide(d + pattern).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(toUtf8(fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

std::string isoLocalTime() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[40];
    std::snprintf(b, sizeof b, "%04u-%02u-%02uT%02u:%02u:%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

std::string fileStampLocal() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[40];
    std::snprintf(b, sizeof b, "%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else {
                o += static_cast<char>(c);
            }
        }
    }
    return o;
}

std::string formatMs(int ms) {
    if (ms <= 0) return "-:--.---";
    char b[40];
    int h = ms / 3600000, m = (ms / 60000) % 60, s = (ms / 1000) % 60, f = ms % 1000;
    if (h > 0) std::snprintf(b, sizeof b, "%d:%02d:%02d.%03d", h, m, s, f);
    else std::snprintf(b, sizeof b, "%d:%02d.%03d", m, s, f);
    return b;
}

std::string formatClock(int ms) {
    if (ms < 0) ms = 0;
    int total = (ms + 999) / 1000;  // count down: 0:00 only when really over
    int h = total / 3600, m = (total / 60) % 60, s = total % 60;
    char b[32];
    if (h > 0) std::snprintf(b, sizeof b, "%d:%02d:%02d", h, m, s);
    else std::snprintf(b, sizeof b, "%d:%02d", m, s);
    return b;
}

} // namespace lr
