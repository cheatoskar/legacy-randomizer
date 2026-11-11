// Durations as people type them: "m:ss", "m:ss.mmm", "h:mm:ss" (and "h:mm:ss.mmm"), and back.
// Header only, so the core test and the plugin share it.
#pragma once
#include <cstdio>
#include <string>

namespace lr {

// Parses into milliseconds. Empty (or only spaces) gives 0 and true ("any"). Returns false for
// anything else that is not one of the formats; *ms is left alone then.
inline bool parseDurationMs(const std::string& text, int* ms) {
    std::string s;
    for (char c : text)
        if (c != ' ' && c != '\t') s += c;
    if (s.empty()) {
        *ms = 0;
        return true;
    }
    // split off the fraction
    int frac = 0;
    size_t dot = s.find('.');
    std::string whole = s.substr(0, dot);
    if (dot != std::string::npos) {
        std::string f = s.substr(dot + 1);
        if (f.empty() || f.size() > 3) return false;
        for (char c : f)
            if (c < '0' || c > '9') return false;
        frac = std::stoi(f);
        for (size_t i = f.size(); i < 3; ++i) frac *= 10;  // ".5" = 500 ms
    }
    // fields separated by ':'; two (m:ss) or three (h:mm:ss)
    int parts[3] = {0, 0, 0};
    int n = 0;
    std::string cur;
    for (size_t i = 0; i <= whole.size(); ++i) {
        if (i == whole.size() || whole[i] == ':') {
            if (cur.empty() || cur.size() > 3 || n == 3) return false;
            parts[n++] = std::stoi(cur);
            cur.clear();
        } else if (whole[i] >= '0' && whole[i] <= '9') {
            cur += whole[i];
        } else {
            return false;
        }
    }
    long long total;
    if (n == 2) {
        if (whole.substr(whole.find(':') + 1).size() != 2 || parts[1] > 59) return false;
        total = (parts[0] * 60LL + parts[1]) * 1000 + frac;
    } else if (n == 3) {
        size_t c1 = whole.find(':'), c2 = whole.find(':', c1 + 1);
        if (c2 - c1 - 1 != 2 || whole.size() - c2 - 1 != 2 || parts[1] > 59 || parts[2] > 59) return false;
        total = ((parts[0] * 60LL + parts[1]) * 60 + parts[2]) * 1000 + frac;
    } else {
        return false;
    }
    if (total > 24LL * 3600 * 1000) return false;
    *ms = static_cast<int>(total);
    return true;
}

// The shortest of the accepted formats that keeps the value: "1:30", "1:30.250", "1:02:03". 0 gives "".
inline std::string formatDurationMs(int ms) {
    if (ms <= 0) return std::string();
    int frac = ms % 1000, s = ms / 1000;
    char b[40];
    if (s >= 3600) {
        if (frac) std::snprintf(b, sizeof b, "%d:%02d:%02d.%03d", s / 3600, s / 60 % 60, s % 60, frac);
        else std::snprintf(b, sizeof b, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    } else if (frac) {
        std::snprintf(b, sizeof b, "%d:%02d.%03d", s / 60, s % 60, frac);
    } else {
        std::snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    }
    return b;
}

} // namespace lr
