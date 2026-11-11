// TMX (TrackMania Exchange) client for the TM1-family sites, WinINet only.
//
// Findings (checked live 2026-11-11): all five sites answer the old-style API
//   GET /api/tracks?<filters>&fields=...&count=N   (JSON {"More":..,"Results":[..]})
//   GET /trackrandom?<filters>                     (302 -> /trackshow/<id>, a real random pick)
//   GET /trackgbx/<id>                             (the .Challenge.Gbx)
// "/api/maps" (the TM2020-style API) does not exist here (404), and "random=1" on /api/tracks is
// ignored (always the same first row), so random picks go through /trackrandom and are verified
// against /api/tracks afterwards.
#pragma once
#include <string>
#include <vector>

namespace lr {

enum class Site { TMNF, TMUF, TMN_ESWC, TMS, TMO };

const char* siteHost(Site s);  // e.g. "tmnf.exchange"
const char* siteName(Site s);  // e.g. "TMNF"

struct TmxFilter {
    // Author time window in ms. Used instead of the API's "Length" field, which is not usable
    // (unrelated to the real length). 0 = no bound.
    int authorTimeMinMs = 0;
    int authorTimeMaxMs = 0;
    // TMX environment id as returned in MapInfo::environment (TMUF: 1 Snow, 2 Desert, 3 Rally,
    // 4 Island, 5 Coast, 6 Bay, 7 Stadium; each site only has its own subset). 0 = any.
    int environment = 0;
    int difficulty = -1;  // 0 Beginner, 1 Intermediate, 2 Expert, 3 Lunatic; -1 = any
    int tag = -1;         // TMX tag id (see <site>/api/meta/tags); -1 = any
    bool excludeUnlimiter = true;     // drop maps with an Unlimiter version
    bool requireAuthorTime = true;    // drop maps without a validated author time
    int maxAttempts = 12;             // random picks before giving up

    // Search filters (TMX query names, checked live 2026-11-11 on
    // /api/tracks of tmnf, nations, sunrise and original: name, author, mood, routes, primarytype,
    // lbtype, uploadedafter/before, inscreenshot/infeatured/inhasrecord; vehicle on tmuf).
    std::string name;                 // part of the map name
    std::string author;               // uploader name (exact)
    int primaryType = 0;              // 0 Race (default: other types can't be played by the run), -1 any
    int moodMask = 0;                 // bits: 1 Sunrise, 2 Day, 4 Sunset, 8 Night; 0 any
    int routesMask = 0;               // bits: 1 Single, 2 Multi, 4 Symmetric; 0 any
    int lbType = -1;                  // 0 Standard, 1 Classic, 2 Nadeo, 3 Uncompetitive, 4 Beta, 5 Star; -1 any
    int vehicle = 0;                  // TMUF only: car by environment id (1..7), 0 any
    std::string uploadedAfter, uploadedBefore;  // "YYYY-MM-DD", "" none
    // hidden In* flags: -1 any, 0 no, 1 yes (index = TmxFlag)
    signed char flags[6] = {-1, -1, -1, -1, -1, -1};
};

// FlagReplays is never sent: on TMX, inreplays=1 returns no maps without a login (checked 2026-11-11).
enum TmxFlag { FlagScreenshot = 0, FlagFeatured, FlagHasRecord, FlagReplays, FlagLatestAuthor, FlagSupporter, FlagCount };
const char* tmxFlagQuery(int flag);  // "inscreenshot", ...
std::string urlEncode(const std::string& s);

struct MapInfo {
    Site site = Site::TMNF;
    long long id = 0;
    std::string uid, name, author;
    int authorMs = 0, goldMs = 0, silverMs = 0, bronzeMs = 0;  // 0 = unknown
    int environment = 0;
    int difficulty = -1;
    std::vector<int> tags;
    bool unlimiter = false;
    int mood = -1, routes = -1, primaryType = -1, car = -1;  // -1 unknown
    std::string downloadUrl;
};

struct TmxError {
    std::string message;  // empty = ok
    bool ok() const { return message.empty(); }
};

struct HttpOptions {
    int timeoutMs = 15000;
    int retries = 1;  // extra attempts after the first
};

// --- pure helpers (no network, unit-tested) ---
std::string buildRandomPath(const TmxFilter& f);                 // "/trackrandom?..."
std::string buildInfoPath(long long id);                         // "/api/tracks?id=..&fields=.."
std::string downloadUrl(Site s, long long id);                   // "https://host/trackgbx/<id>"
long long parseTrackIdFromLocation(const std::string& location); // "/trackshow/123" -> 123, else -1
// Parses the /api/tracks answer; fills the first result.
TmxError parseInfoJson(Site s, const std::string& json, MapInfo& out);
// True when the map satisfies the filter (also used to verify /trackrandom picks).
bool matchesFilter(const MapInfo& m, const TmxFilter& f);

// --- network ---
TmxError getMapInfo(Site s, long long id, MapInfo& out, const HttpOptions& o = HttpOptions());
TmxError getRandomMap(Site s, const TmxFilter& f, MapInfo& out, const HttpOptions& o = HttpOptions());
// Downloads the .Challenge.Gbx to path (UTF-8; via a temp file, only replaced on success) and
// optionally returns the bytes.
TmxError downloadMap(Site s, long long id, const std::string& path, std::string* bytesOut = nullptr,
                     const HttpOptions& o = HttpOptions());

// Screenshot TMX shows for a map: GET /trackshow/<id>/image/1 (JPEG; checked live on
// sunrise.tm-exchange.com and tmnf.exchange 2026-11-11). Fails when the map has none.
std::string thumbnailPath(long long id);       // "/trackshow/<id>/image/1"
bool looksLikeImage(const std::string& bytes); // JPEG or PNG signature
TmxError downloadThumbnail(Site s, long long id, std::string& bytes, const HttpOptions& o = HttpOptions());

} // namespace lr
