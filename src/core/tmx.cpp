#include "tmx.h"

#include <cstdio>
#include <cstring>

#include <windows.h>
#include <wininet.h>

#include "json.h"

namespace lr {

namespace {

const char* const kUserAgent = "LegacyRandomizer/0.1";
const char* const kInfoFields =
    "TrackId,TrackName,UId,Uploader.Name,Difficulty,Environment,AuthorTime,GoldTarget,SilverTarget,"
    "BronzeTarget,Tags,UnlimiterVersion,Mood,Routes,PrimaryType,Car";

struct HttpReply {
    DWORD status = 0;
    std::string location;
    std::string body;
};

std::string winErr(const char* what, DWORD code) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s failed (Windows error %lu)", what, static_cast<unsigned long>(code));
    return buf;
}

// One request. verb is "GET" or "HEAD"; redirects are NOT followed (location is returned).
TmxError httpOnce(const char* host, const std::string& path, const char* verb, int timeoutMs, HttpReply& out) {
    out = HttpReply();
    HINTERNET net = InternetOpenA(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!net) return {winErr("InternetOpen", GetLastError())};
    DWORD t = static_cast<DWORD>(timeoutMs);
    InternetSetOptionA(net, INTERNET_OPTION_CONNECT_TIMEOUT, &t, sizeof t);
    InternetSetOptionA(net, INTERNET_OPTION_SEND_TIMEOUT, &t, sizeof t);
    InternetSetOptionA(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &t, sizeof t);

    TmxError err;
    HINTERNET con = InternetConnectA(net, host, INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr,
                                     INTERNET_SERVICE_HTTP, 0, 0);
    if (!con) {
        err.message = std::string("cannot connect to ") + host + ": " + winErr("InternetConnect", GetLastError());
        InternetCloseHandle(net);
        return err;
    }
    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_NO_COOKIES |
                  INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD;
    HINTERNET req = HttpOpenRequestA(con, verb, path.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!req) {
        err.message = winErr("HttpOpenRequest", GetLastError());
    } else if (!HttpSendRequestA(req, nullptr, 0, nullptr, 0)) {
        DWORD e = GetLastError();
        err.message = (e == ERROR_INTERNET_TIMEOUT ? std::string("request to ") + host + " timed out"
                                                    : std::string("request to ") + host + ": " + winErr("HttpSendRequest", e));
    } else {
        DWORD st = 0, sz = sizeof st;
        HttpQueryInfoA(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &st, &sz, nullptr);
        out.status = st;
        char loc[1024];
        DWORD ls = sizeof loc;
        if (HttpQueryInfoA(req, HTTP_QUERY_LOCATION, loc, &ls, nullptr)) out.location.assign(loc, ls);
        char buf[16384];
        for (;;) {
            DWORD got = 0;
            if (!InternetReadFile(req, buf, sizeof buf, &got)) {
                err.message = (GetLastError() == ERROR_INTERNET_TIMEOUT ? std::string("download from ") + host + " timed out"
                                                                         : winErr("InternetReadFile", GetLastError()));
                break;
            }
            if (got == 0) break;
            out.body.append(buf, got);
            if (out.body.size() > 64u * 1024 * 1024) { err.message = "response too large"; break; }
        }
    }
    if (req) InternetCloseHandle(req);
    InternetCloseHandle(con);
    InternetCloseHandle(net);
    return err;
}

bool retryable(const TmxError& e, const HttpReply& r) {
    if (!e.ok()) return true;
    return r.status >= 500 || r.status == 429;
}

// With retries; returns an error for transport failures and for HTTP statuses that are neither
// 2xx nor 3xx.
TmxError httpRequest(Site s, const std::string& path, const char* verb, const HttpOptions& o, HttpReply& out) {
    TmxError err;
    for (int attempt = 0; attempt <= o.retries; ++attempt) {
        err = httpOnce(siteHost(s), path, verb, o.timeoutMs, out);
        if (!retryable(err, out)) break;
        if (attempt < o.retries) Sleep(500);
    }
    if (!err.ok()) return err;
    if (out.status >= 400 || out.status == 0) {
        char buf[200];
        std::snprintf(buf, sizeof buf, "%s answered HTTP %lu for %s", siteHost(s),
                      static_cast<unsigned long>(out.status), path.substr(0, 60).c_str());
        return {buf};
    }
    return {};
}

void appendQuery(std::string& q, const std::string& kv) {
    if (!q.empty()) q += '&';
    q += kv;
}

} // namespace

const char* siteHost(Site s) {
    switch (s) {
    case Site::TMNF: return "tmnf.exchange";
    case Site::TMUF: return "tmuf.exchange";
    case Site::TMN_ESWC: return "nations.tm-exchange.com";
    case Site::TMS: return "sunrise.tm-exchange.com";
    case Site::TMO: return "original.tm-exchange.com";
    }
    return "";
}

const char* siteName(Site s) {
    switch (s) {
    case Site::TMNF: return "TMNF";
    case Site::TMUF: return "TMUF";
    case Site::TMN_ESWC: return "TMN ESWC";
    case Site::TMS: return "TM Sunrise";
    case Site::TMO: return "TM Original";
    }
    return "";
}

std::string mapFileName(Site s, long long id) {
    std::string n = std::string(siteName(s)) + "_" + std::to_string(id) + ".Challenge.Gbx";
    for (char& c : n)
        if (c == ' ') c = '_';
    return n;
}

std::string buildRandomPath(const TmxFilter& f) {
    std::string q;
    if (f.excludeUnlimiter) appendQuery(q, "inunlimiter=0");
    if (f.requireAuthorTime) appendQuery(q, "authortimemin=" + std::to_string(f.authorTimeMinMs > 0 ? f.authorTimeMinMs : 1));
    else if (f.authorTimeMinMs > 0) appendQuery(q, "authortimemin=" + std::to_string(f.authorTimeMinMs));
    if (f.authorTimeMaxMs > 0) appendQuery(q, "authortimemax=" + std::to_string(f.authorTimeMaxMs));
    // The environment query value is the field value minus one (verified on all five sites).
    if (f.environment > 0) appendQuery(q, "environment=" + std::to_string(f.environment - 1));
    if (f.difficulty >= 0) appendQuery(q, "difficulty=" + std::to_string(f.difficulty));
    if (f.tag >= 0) appendQuery(q, "tag=" + std::to_string(f.tag));
    if (!f.name.empty()) appendQuery(q, "name=" + urlEncode(f.name));
    if (!f.author.empty()) appendQuery(q, "author=" + urlEncode(f.author));
    if (f.primaryType >= 0) appendQuery(q, "primarytype=" + std::to_string(f.primaryType));
    auto mask = [&q](const char* key, int m, int bits) {
        if (m <= 0) return;
        std::string v;
        for (int i = 0; i < bits; ++i)
            if (m & (1 << i)) v += (v.empty() ? "" : "%2C") + std::to_string(i);
        appendQuery(q, std::string(key) + "=" + v);
    };
    mask("mood", f.moodMask, 4);
    mask("routes", f.routesMask, 3);
    if (f.lbType >= 0) appendQuery(q, "lbtype=" + std::to_string(f.lbType));
    if (f.vehicle > 0) appendQuery(q, "vehicle=" + std::to_string(f.vehicle - 1));
    if (!f.uploadedAfter.empty()) appendQuery(q, "uploadedafter=" + urlEncode(f.uploadedAfter));
    if (!f.uploadedBefore.empty()) appendQuery(q, "uploadedbefore=" + urlEncode(f.uploadedBefore));
    for (int i = 0; i < FlagCount; ++i)
        if (f.flags[i] >= 0 && i != FlagReplays) appendQuery(q, std::string(tmxFlagQuery(i)) + "=" + (f.flags[i] ? "1" : "0"));
    return q.empty() ? "/trackrandom" : "/trackrandom?" + q;
}

const char* tmxFlagQuery(int flag) {
    switch (flag) {
    case FlagScreenshot: return "inscreenshot";
    case FlagFeatured: return "infeatured";
    case FlagHasRecord: return "inhasrecord";
    case FlagReplays: return "inreplays";
    case FlagLatestAuthor: return "inlatestauthor";
    case FlagSupporter: return "insupporter";
    }
    return "";
}

std::string urlEncode(const std::string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            o += static_cast<char>(c);
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 15];
        }
    }
    return o;
}

std::string buildInfoPath(long long id) {
    return "/api/tracks?id=" + std::to_string(id) + "&fields=" + kInfoFields;
}

std::string downloadUrl(Site s, long long id) {
    return std::string("https://") + siteHost(s) + "/trackgbx/" + std::to_string(id);
}

long long parseTrackIdFromLocation(const std::string& loc) {
    size_t slash = loc.rfind('/');
    if (slash == std::string::npos || slash + 1 >= loc.size()) return -1;
    if (loc.find("/trackshow/") == std::string::npos) return -1;
    long long v = 0;
    size_t i = slash + 1;
    size_t digits = 0;
    for (; i < loc.size() && loc[i] >= '0' && loc[i] <= '9'; ++i, ++digits) {
        v = v * 10 + (loc[i] - '0');
        if (v > 2000000000LL) return -1;
    }
    return digits ? v : -1;
}

TmxError parseInfoJson(Site s, const std::string& json, MapInfo& out) {
    Json root;
    std::string perr;
    if (!JsonParser::parse(json, root, perr)) return {"TMX answer is not valid JSON: " + perr};
    if (const Json* detail = root.get("detail")) return {"TMX error: " + detail->asString()};
    const Json* results = root.get("Results");
    if (!results || results->type != Json::Array) return {"TMX answer has no Results array"};
    if (results->arr.empty()) return {"TMX has no such map"};
    const Json& r = results->arr[0];
    MapInfo m;
    m.site = s;
    if (const Json* v = r.get("TrackId")) m.id = v->asInt();
    if (m.id <= 0) return {"TMX answer has no TrackId"};
    if (const Json* v = r.get("TrackName")) m.name = v->asString();
    if (const Json* v = r.get("UId")) m.uid = v->asString();
    if (const Json* u = r.get("Uploader"))
        if (const Json* n = u->get("Name")) m.author = n->asString();
    if (const Json* v = r.get("AuthorTime")) m.authorMs = static_cast<int>(v->asInt());
    if (const Json* v = r.get("GoldTarget")) m.goldMs = static_cast<int>(v->asInt());
    if (const Json* v = r.get("SilverTarget")) m.silverMs = static_cast<int>(v->asInt());
    if (const Json* v = r.get("BronzeTarget")) m.bronzeMs = static_cast<int>(v->asInt());
    if (const Json* v = r.get("Environment")) m.environment = static_cast<int>(v->asInt());
    if (const Json* v = r.get("Difficulty")) m.difficulty = static_cast<int>(v->asInt(-1));
    if (const Json* v = r.get("Tags"))
        if (v->type == Json::Array)
            for (const Json& t : v->arr) m.tags.push_back(static_cast<int>(t.asInt()));
    if (const Json* v = r.get("UnlimiterVersion")) m.unlimiter = v->type != Json::Null;
    if (const Json* v = r.get("Mood")) m.mood = static_cast<int>(v->asInt(-1));
    if (const Json* v = r.get("Routes")) m.routes = static_cast<int>(v->asInt(-1));
    if (const Json* v = r.get("PrimaryType")) m.primaryType = static_cast<int>(v->asInt(-1));
    if (const Json* v = r.get("Car")) m.car = static_cast<int>(v->asInt(-1));
    m.downloadUrl = downloadUrl(s, m.id);
    out = m;
    return {};
}

bool matchesFilter(const MapInfo& m, const TmxFilter& f) {
    if (f.excludeUnlimiter && m.unlimiter) return false;
    if (f.requireAuthorTime && m.authorMs <= 0) return false;
    if (f.authorTimeMinMs > 0 && m.authorMs < f.authorTimeMinMs) return false;
    if (f.authorTimeMaxMs > 0 && (m.authorMs <= 0 || m.authorMs > f.authorTimeMaxMs)) return false;
    if (f.environment > 0 && m.environment != f.environment) return false;
    if (f.difficulty >= 0 && m.difficulty != f.difficulty) return false;
    if (f.primaryType >= 0 && m.primaryType >= 0 && m.primaryType != f.primaryType) return false;
    if (f.moodMask > 0 && m.mood >= 0 && !(f.moodMask & (1 << m.mood))) return false;
    if (f.routesMask > 0 && m.routes >= 0 && !(f.routesMask & (1 << m.routes))) return false;
    if (f.vehicle > 0 && m.car > 0 && m.car != f.vehicle) return false;
    if (f.tag >= 0) {
        bool has = false;
        for (int t : m.tags) has = has || t == f.tag;
        if (!has) return false;
    }
    return true;
}

TmxError getMapInfo(Site s, long long id, MapInfo& out, const HttpOptions& o) {
    HttpReply r;
    TmxError e = httpRequest(s, buildInfoPath(id), "GET", o, r);
    if (!e.ok()) return e;
    return parseInfoJson(s, r.body, out);
}

TmxError getRandomMap(Site s, const TmxFilter& f, MapInfo& out, const HttpOptions& o) {
    std::string lastWhy = "no attempt made";
    for (int attempt = 1; attempt <= f.maxAttempts; ++attempt) {
        HttpReply r;
        TmxError e = httpRequest(s, buildRandomPath(f), "HEAD", o, r);
        if (!e.ok()) {
            // 404 from /trackrandom = nothing matches the filter; do not hammer the site
            if (r.status == 404) return {std::string("no ") + siteName(s) + " map matches the filter"};
            return e;
        }
        long long id = parseTrackIdFromLocation(r.location);
        if (id < 0) { lastWhy = "random pick did not redirect to a map (HTTP " + std::to_string(r.status) + ")"; continue; }
        MapInfo m;
        e = getMapInfo(s, id, m, o);
        if (!e.ok()) { lastWhy = e.message; continue; }
        if (!matchesFilter(m, f)) { lastWhy = "map " + std::to_string(id) + " did not match the filter"; continue; }
        out = m;
        return {};
    }
    return {std::string("gave up after ") + std::to_string(f.maxAttempts) + " picks: " + lastWhy};
}

TmxError downloadMap(Site s, long long id, const std::string& path, std::string* bytesOut, const HttpOptions& o) {
    HttpReply r;
    TmxError e = httpRequest(s, "/trackgbx/" + std::to_string(id), "GET", o, r);
    if (!e.ok()) return e;
    if (r.status != 200) return {"download answered HTTP " + std::to_string(r.status)};
    if (r.body.size() < 16 || std::memcmp(r.body.data(), "GBX", 3) != 0)
        return {"downloaded file is not a GBX (" + std::to_string(r.body.size()) + " bytes)"};
    e = writeMapFile(path, r.body);
    if (!e.ok()) return e;
    if (bytesOut) *bytesOut = std::move(r.body);
    return {};
}

TmxError writeMapFile(const std::string& path, const std::string& bytes) {
    // path is UTF-8; fall back to ANSI for byte strings that are not valid UTF-8
    std::wstring wpath;
    int wn = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
    if (wn > 0) {
        wpath.resize(static_cast<size_t>(wn));
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wn);
        wpath.resize(static_cast<size_t>(wn - 1));
    } else {
        int an = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
        if (an <= 0) return {"bad file name " + path};
        wpath.resize(static_cast<size_t>(an));
        MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &wpath[0], an);
        wpath.resize(static_cast<size_t>(an - 1));
    }
    std::wstring tmp = wpath + L".part";
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, tmp.c_str(), L"wb") != 0 || !fp) return {"cannot write " + path + ".part"};
    size_t wrote = std::fwrite(bytes.data(), 1, bytes.size(), fp);
    int closed = std::fclose(fp);
    if (wrote != bytes.size() || closed != 0) { DeleteFileW(tmp.c_str()); return {"short write to " + path + ".part"}; }
    if (!MoveFileExW(tmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD e2 = GetLastError();
        DeleteFileW(tmp.c_str());
        return {winErr("rename of downloaded file", e2)};
    }
    return {};
}

std::string thumbnailPath(long long id) { return "/trackshow/" + std::to_string(id) + "/image/1"; }

bool looksLikeImage(const std::string& b) {
    if (b.size() >= 3 && static_cast<unsigned char>(b[0]) == 0xFF && static_cast<unsigned char>(b[1]) == 0xD8 &&
        static_cast<unsigned char>(b[2]) == 0xFF)
        return true;  // JPEG
    if (b.size() >= 8 && std::memcmp(b.data(), "\x89PNG\r\n\x1a\n", 8) == 0) return true;
    return false;
}

TmxError downloadThumbnail(Site s, long long id, std::string& bytes, const HttpOptions& o) {
    HttpReply r;
    TmxError e = httpRequest(s, thumbnailPath(id), "GET", o, r);
    if (!e.ok()) return e;
    if (r.status != 200) return {"thumbnail answered HTTP " + std::to_string(r.status)};
    if (!looksLikeImage(r.body)) return {"thumbnail is not a JPEG/PNG image"};
    bytes = std::move(r.body);
    return {};
}

} // namespace lr
