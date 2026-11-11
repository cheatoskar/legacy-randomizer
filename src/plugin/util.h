// Small Windows helpers: UTF-8 paths, files, timestamps.
#pragma once
#include <string>
#include <vector>

namespace lr {

std::wstring toWide(const std::string& utf8);
std::string toUtf8(const std::wstring& w);
bool ensureDir(const std::string& utf8Dir);  // creates missing parents too
bool readFile(const std::string& utf8Path, std::string& out);
bool writeFileAtomic(const std::string& utf8Path, const std::string& data);  // via .tmp + rename
void deleteFile(const std::string& utf8Path);
std::vector<std::string> listFiles(const std::string& utf8Dir, const char* pattern);  // file names only
std::string isoLocalTime();      // "2026-11-11T10:22:05"
std::string fileStampLocal();    // "20261111-102205"
std::string jsonEscape(const std::string& s);
std::string formatMs(int ms);    // "0:42.310", "-:--.---" for <= 0
std::string formatClock(int ms); // timer: "47:12", "1:02:03"

} // namespace lr
