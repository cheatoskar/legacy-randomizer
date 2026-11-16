#include "autosave_scan.h"

#include <cstdio>
#include <map>

#include <windows.h>

#include "../core/gbx.h"
#include "util.h"

namespace lr {

namespace {
std::string lowerKey(const std::string& utf8) {
    std::wstring w = toWide(utf8);
    if (!w.empty()) CharLowerBuffW(&w[0], static_cast<DWORD>(w.size()));
    return toUtf8(w);
}
} // namespace

std::string normalizeFolder(const std::string& in) {
    std::string s = in;
    auto bad = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"'; };
    while (!s.empty() && bad(s.back())) s.pop_back();
    size_t b = 0;
    while (b < s.size() && bad(s[b])) ++b;
    s = s.substr(b);
    if (s.empty()) return "";
    for (char& c : s)
        if (c == '/') c = '\\';
    // collapse repeated separators (keep the leading \\ of a network path)
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && !out.empty() && out.back() == '\\' && i > 1) continue;
        out += s[i];
    }
    if (out.empty() || out == "\\" || out == "\\\\") return "";
    if (out.back() != '\\') out += '\\';
    return out;
}

std::vector<std::string> parseFolderList(const std::string& s) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('|', p);
        if (e == std::string::npos) e = s.size();
        addFolder(out, s.substr(p, e - p));
        p = e + 1;
    }
    return out;
}

std::string joinFolderList(const std::vector<std::string>& dirs) {
    std::string s;
    for (const std::string& d : dirs) {
        if (d.empty()) continue;
        if (!s.empty()) s += '|';
        s += d;
    }
    return s;
}

bool addFolder(std::vector<std::string>& dirs, const std::string& entry) {
    std::string n = normalizeFolder(entry);
    if (n.empty()) return false;
    std::string key = lowerKey(n);
    for (const std::string& d : dirs)
        if (lowerKey(d) == key) return false;
    dirs.push_back(n);
    return true;
}

std::vector<std::string> mergeFolders(const std::vector<std::string>& defaults, const std::vector<std::string>& extra) {
    std::vector<std::string> out;
    for (const std::string& d : defaults) addFolder(out, d);
    for (const std::string& d : extra) addFolder(out, d);
    return out;
}

int countReplays(const std::string& dir) {
    std::string d = normalizeFolder(dir);
    if (d.empty()) return -1;
    DWORD attr = GetFileAttributesW(toWide(d).c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return -1;
    int n = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(toWide(d + "*.gbx").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++n;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

ScanResult scanReplayFolders(const std::vector<std::string>& dirs, const std::string& cachePath, std::set<std::string>& finished,
                             const std::function<bool()>& shouldStop) {
    ScanResult res;
    std::map<std::string, std::string> cache, fresh;
    std::string text;
    if (!cachePath.empty() && readFile(cachePath, text)) {
        size_t p = 0;
        while (p < text.size()) {
            size_t e = text.find('\n', p);
            if (e == std::string::npos) e = text.size();
            std::string line = text.substr(p, e - p);
            size_t bar = line.rfind('|');
            if (bar != std::string::npos) cache[line.substr(0, bar)] = line.substr(bar + 1);
            p = e + 1;
        }
    }
    for (const std::string& dir : mergeFolders(dirs, {})) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(toWide(dir + "*.gbx").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;  // missing folder: skipped silently
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ++res.files;
            std::string name = toUtf8(fd.cFileName);
            std::string key = dir + name + "|" + std::to_string(fd.nFileSizeLow) + "|" +
                              std::to_string((static_cast<unsigned long long>(fd.ftLastWriteTime.dwHighDateTime) << 32) | fd.ftLastWriteTime.dwLowDateTime);
            std::string uid;
            auto it = cache.find(key);
            if (it != cache.end()) {
                uid = it->second;
            } else {
                FILE* fp = nullptr;
                if (_wfopen_s(&fp, toWide(dir + name).c_str(), L"rb") == 0 && fp) {
                    std::vector<unsigned char> buf(64 * 1024);
                    size_t n = std::fread(buf.data(), 1, buf.size(), fp);
                    std::fclose(fp);
                    uid = replayMapUid(buf.data(), n);
                    ++res.read;
                }
            }
            fresh[key] = uid;
            if (!uid.empty()) finished.insert(uid);
            if (shouldStop && shouldStop()) {
                FindClose(h);
                res.stopped = true;
                return res;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (!cachePath.empty() && res.read > 0) {
        std::string out;
        for (const auto& kv : fresh) out += kv.first + "|" + kv.second + "\n";
        writeFileAtomic(cachePath, out);
    }
    return res;
}

} // namespace lr
