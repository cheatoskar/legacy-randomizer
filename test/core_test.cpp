// Unit tests for src/core (session rules, GBX header reader, TMX helpers, tiny JSON reader).
//   core_test.exe                 offline tests
//   core_test.exe --live          plus a handful of live TMX requests (one random map per site)
//   core_test.exe --maps <dir>    folder with real .Challenge.Gbx files (default: the ESWC download folder)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <windows.h>

#include "../src/core/gbx.h"
#include "../src/core/json.h"
#include "../src/core/session.h"
#include "../src/core/tmx.h"

namespace fs = std::filesystem;
using namespace lr;

static int g_checks = 0, g_fails = 0;

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        ++g_checks;                                                                          \
        if (!(cond)) { ++g_fails; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define CHECK_EQ(a, b) CHECK((a) == (b))

static void section(const char* name) { std::printf("[%s]\n", name); }

// ---------------------------------------------------------------- json
static void testJson() {
    section("json");
    Json j;
    std::string err;
    CHECK(JsonParser::parse(R"({"a":[1,2.5,-3],"s":"x\u00e9\n","n":null,"t":true,"o":{"k":"v"}})", j, err));
    CHECK_EQ(j.get("a")->arr.size(), 3u);
    CHECK_EQ(j.get("a")->arr[2].asInt(), -3);
    CHECK_EQ(j.get("s")->asString(), std::string("x\xC3\xA9\n"));
    CHECK(j.get("n")->type == Json::Null);
    CHECK(j.get("t")->b);
    CHECK_EQ(j.get("o")->get("k")->asString(), std::string("v"));
    CHECK(j.get("missing") == nullptr);
    CHECK(!JsonParser::parse("{\"a\":", j, err));
    CHECK(!JsonParser::parse("[1,2", j, err));
    CHECK(!JsonParser::parse("{} x", j, err));
}

// ---------------------------------------------------------------- tmx helpers
static void testTmxHelpers() {
    section("tmx helpers");
    CHECK_EQ(thumbnailPath(42), std::string("/trackshow/42/image/1"));
    CHECK(looksLikeImage(std::string("\xFF\xD8\xFF\xE0", 4)));
    CHECK(looksLikeImage(std::string("\x89PNG\r\n\x1a\n", 8)));
    CHECK(!looksLikeImage("<html>"));
    CHECK_EQ(parseTrackIdFromLocation("/trackshow/13775497"), 13775497LL);
    CHECK_EQ(parseTrackIdFromLocation("/trackshow/"), -1LL);
    CHECK_EQ(parseTrackIdFromLocation("/error"), -1LL);
    CHECK_EQ(parseTrackIdFromLocation(""), -1LL);

    TmxFilter f;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom?inunlimiter=0&authortimemin=1"));
    f.environment = 7; f.difficulty = 2; f.tag = 5; f.authorTimeMaxMs = 60000;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom?inunlimiter=0&authortimemin=1&authortimemax=60000&environment=6&difficulty=2&tag=5"));
    f = TmxFilter();
    f.excludeUnlimiter = false; f.requireAuthorTime = false;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom"));

    CHECK_EQ(std::string(siteHost(Site::TMN_ESWC)), std::string("nations.tm-exchange.com"));
    CHECK_EQ(downloadUrl(Site::TMO, 42), std::string("https://original.tm-exchange.com/trackgbx/42"));

    // real answer from tmnf.exchange (2026-11-11)
    const std::string ok =
        R"({"More":true,"Results":[{"TrackId":13775497,"TrackName":"Lisa.Mix.103","UId":"8JiccFftITbxjnisy1NM9A023A5","Uploader":{"Name":"Chakal"},"Difficulty":1,"Environment":7,"AuthorTime":15110,"GoldTarget":17000,"SilverTarget":19000,"BronzeTarget":23000,"Tags":[0],"UnlimiterVersion":null}]})";
    MapInfo m;
    CHECK(parseInfoJson(Site::TMNF, ok, m).ok());
    CHECK_EQ(m.id, 13775497LL);
    CHECK_EQ(m.name, std::string("Lisa.Mix.103"));
    CHECK_EQ(m.author, std::string("Chakal"));
    CHECK_EQ(m.authorMs, 15110);
    CHECK_EQ(m.goldMs, 17000);
    CHECK_EQ(m.silverMs, 19000);
    CHECK_EQ(m.bronzeMs, 23000);
    CHECK_EQ(m.environment, 7);
    CHECK(!m.unlimiter);
    CHECK_EQ(m.tags.size(), 1u);
    CHECK_EQ(m.downloadUrl, std::string("https://tmnf.exchange/trackgbx/13775497"));

    TmxFilter flt;
    CHECK(matchesFilter(m, flt));
    flt.environment = 3;
    CHECK(!matchesFilter(m, flt));
    flt = TmxFilter(); flt.authorTimeMaxMs = 10000;
    CHECK(!matchesFilter(m, flt));
    flt = TmxFilter(); flt.tag = 0;
    CHECK(matchesFilter(m, flt));
    flt.tag = 9;
    CHECK(!matchesFilter(m, flt));
    MapInfo unl = m; unl.unlimiter = true;
    CHECK(!matchesFilter(unl, TmxFilter()));
    MapInfo noAt = m; noAt.authorMs = 0;
    CHECK(!matchesFilter(noAt, TmxFilter()));

    const std::string withUnl = ok.substr(0, ok.find("\"UnlimiterVersion\":null")) + "\"UnlimiterVersion\":\"1.0\"}]}";
    CHECK(parseInfoJson(Site::TMNF, withUnl, m).ok());
    CHECK(m.unlimiter);

    CHECK(!parseInfoJson(Site::TMNF, R"({"More":false,"Results":[]})", m).ok());
    CHECK(!parseInfoJson(Site::TMNF, R"({"type":"Bad Request","detail":"Specified field 'X' does not exist"})", m).ok());
    CHECK(!parseInfoJson(Site::TMNF, "<html>", m).ok());
}

// ---------------------------------------------------------------- gbx
static std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string xmlAttrOf(const std::string& bytes, const char* key) {
    size_t a = bytes.find(key);
    if (a == std::string::npos) return std::string();
    a += std::strlen(key);
    size_t b = bytes.find('"', a);
    return bytes.substr(a, b - a);
}

static void testGbx(const fs::path& mapsDir) {
    section("gbx header");
    CHECK_EQ(percentDecode("%24000%5e%5e"), std::string("$000^^"));
    CHECK_EQ(stripTmCodes("$f00Red $wWide$$ $0f0x"), std::string("Red Wide$ x"));
    CHECK_EQ(stripTmCodes("$000^^$f0fHukkaportti$000^^"), std::string("^^Hukkaportti^^"));

    // error handling
    CHECK(!parseChallengeHeader(reinterpret_cast<const unsigned char*>("nope"), 4).ok);
    const char bad[] = "GBX\x06\x00TUCR\x00\x30\x00\x24\x10\x00\x00\x00\x01\x00\x00\x00";
    CHECK(!parseChallengeHeader(reinterpret_cast<const unsigned char*>(bad), sizeof bad - 1).ok);

    // synthetic TMF-style header with an info chunk and an embedded thumbnail
    {
        auto u32 = [](std::string& o, uint32_t v) {
            for (int i = 0; i < 4; ++i) o += static_cast<char>((v >> (8 * i)) & 0xFF);
        };
        auto str = [&u32](std::string& o, const std::string& s) { u32(o, static_cast<uint32_t>(s.size())); o += s; };
        std::string info;
        info += '\x05';
        u32(info, 3);
        u32(info, 0x40000000u); str(info, "uidXYZ");
        u32(info, 0x40000000u); str(info, "Stadium");
        u32(info, 0x40000000u); str(info, "someone");
        str(info, "$f00Red$z map");
        const std::string jpg("\xFF\xD8\xFF\xE0" "fakejpeg", 12);
        std::string thumb;
        u32(thumb, 1);
        u32(thumb, static_cast<uint32_t>(jpg.size()));
        thumb += "<Thumbnail.jpg>" + jpg + "</Thumbnail.jpg><Comments>";
        str(thumb, "hello");
        thumb += "</Comments>";
        std::string body;
        u32(body, 2);
        u32(body, 0x03043003u); u32(body, static_cast<uint32_t>(info.size()));
        u32(body, 0x03043007u); u32(body, static_cast<uint32_t>(thumb.size()) | 0x80000000u);
        body += info + thumb;
        std::string file("GBX\x06\x00" "BUCR", 9);
        u32(file, 0x03043000u);
        u32(file, static_cast<uint32_t>(body.size()));
        file += body;
        GbxMapHeader h = parseChallengeHeader(reinterpret_cast<const unsigned char*>(file.data()), file.size());
        CHECK(h.ok);
        CHECK_EQ(h.uid, std::string("uidXYZ"));
        CHECK_EQ(h.name, std::string("$f00Red$z map"));
        CHECK_EQ(h.thumbnailJpeg, jpg);
        CHECK_EQ(h.comments, std::string("hello"));
        // truncated thumbnail chunk: header still parses, thumbnail is dropped
        std::string cut = file.substr(0, file.size() - 20);
        GbxMapHeader h2 = parseChallengeHeader(reinterpret_cast<const unsigned char*>(cut.data()), cut.size());
        CHECK(!h2.ok || h2.thumbnailJpeg.empty());
    }

    std::error_code ec;
    if (!fs::exists(mapsDir, ec)) {
        std::printf("  SKIP real maps (folder not found: %s)\n", mapsDir.string().c_str());
        return;
    }
    int files = 0, okCount = 0, binTimes = 0, binName = 0, xmlTimeMatch = 0, xmlTimeChecked = 0, thumbs = 0;
    bool first = true;
    for (const auto& e : fs::recursive_directory_iterator(mapsDir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string name = e.path().filename().string();
        std::string ext = e.path().extension().string();
        if (_stricmp(ext.c_str(), ".gbx") != 0) continue;
        ++files;
        std::string bytes = readAll(e.path());
        GbxMapHeader h = parseChallengeHeader(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
        if (!h.ok) { std::printf("  FAIL parse %s: %s\n", name.c_str(), h.error.c_str()); ++g_fails; continue; }
        ++okCount;
        binTimes += h.timesFromBinary;
        binName += h.nameFromBinary;
        if (looksLikeImage(h.thumbnailJpeg)) ++thumbs;
        CHECK(!h.uid.empty());
        CHECK(!h.name.empty());
        CHECK(!h.environment.empty());
        CHECK(h.goldMs > 0 && h.silverMs >= h.goldMs && h.bronzeMs >= h.silverMs);
        // the XML copy in the same header must agree with what we read
        std::string xat = xmlAttrOf(bytes, "authortime=\"");
        std::string xgold = xmlAttrOf(bytes, " gold=\"");
        if (!xat.empty() && !xgold.empty()) {
            ++xmlTimeChecked;
            if (std::atoi(xat.c_str()) == h.authorMs && std::atoi(xgold.c_str()) == h.goldMs) ++xmlTimeMatch;
        }
        if (first) {
            first = false;
            std::printf("  sample: %s | by %s | %s | A %s G %s S %s B %s\n", stripTmCodes(h.name).c_str(),
                        h.author.c_str(), h.environment.c_str(), Session::formatTime(h.authorMs).c_str(),
                        Session::formatTime(h.goldMs).c_str(), Session::formatTime(h.silverMs).c_str(),
                        Session::formatTime(h.bronzeMs).c_str());
        }
    }
    std::printf("  %d files, %d parsed, binary times %d, binary identity %d, xml cross-check %d/%d, jpeg thumbnails %d\n",
                files, okCount, binTimes, binName, xmlTimeMatch, xmlTimeChecked, thumbs);
    CHECK(files > 0);
    CHECK_EQ(files, okCount);
    CHECK_EQ(binTimes, okCount);
    CHECK_EQ(binName, okCount);
    CHECK_EQ(xmlTimeMatch, xmlTimeChecked);

    // one known map (ASCII file name), values checked against the XML by hand
    for (const auto& e : fs::recursive_directory_iterator(mapsDir, ec)) {
        if (e.path().filename().string() != "$000^^$f0fHukkaportti$000^^.Challenge.Gbx") continue;
        GbxMapHeader h = readChallengeHeaderFile(e.path().string());
        CHECK(h.ok);
        CHECK_EQ(h.uid, std::string("ujCpmaAbssln7BApEpTHIeGmlcd"));
        CHECK_EQ(h.name, std::string("$000^^$f0fHukkaportti$000^^"));
        CHECK_EQ(h.author, std::string("seta_mitsubishi"));
        CHECK_EQ(h.environment, std::string("Stadium"));
        CHECK_EQ(h.bronzeMs, 38000);
        CHECK_EQ(h.silverMs, 31000);
        CHECK_EQ(h.goldMs, 29000);
        CHECK_EQ(h.authorMs, 25890);
        CHECK_EQ(h.costCoppers, 588);
        break;
    }
}

// ---------------------------------------------------------------- session
static SessionMap mk(long long id, int a = 30000, int g = 35000, int s = 40000, int b = 50000) {
    SessionMap m;
    m.id = id;
    m.name = "map" + std::to_string(id);
    m.medals.authorMs = a; m.medals.goldMs = g; m.medals.silverMs = s; m.medals.bronzeMs = b;
    return m;
}

static void testSession() {
    section("session: medals");
    MapMedals mm; mm.authorMs = 30000; mm.goldMs = 35000; mm.silverMs = 40000; mm.bronzeMs = 50000;
    CHECK(Session::medalFor(30000, mm) == Medal::Author);
    CHECK(Session::medalFor(30001, mm) == Medal::Gold);
    CHECK(Session::medalFor(35000, mm) == Medal::Gold);
    CHECK(Session::medalFor(40000, mm) == Medal::Silver);
    CHECK(Session::medalFor(50000, mm) == Medal::Bronze);
    CHECK(Session::medalFor(50001, mm) == Medal::None);
    CHECK(Session::medalFor(0, mm) == Medal::None);
    CHECK(Session::medalFor(1000, MapMedals()) == Medal::None);  // unknown medal times
    CHECK_EQ(Session::formatTime(754321), std::string("12:34.321"));
    CHECK_EQ(Session::formatTime(3723004), std::string("1:02:03.004"));

    section("session: RMC");
    {
        Session s(SessionConfig::rmc());
        CHECK(s.state() == State::Idle);
        s.tick(5000);  // nothing runs before start
        CHECK_EQ(s.remainingMs(), 3600000);
        s.start();
        s.tick(5000);  // loading: timer does not run
        CHECK_EQ(s.remainingMs(), 3600000);
        s.mapStarted(mk(1));
        CHECK(s.state() == State::Playing);
        s.tick(10000);
        CHECK_EQ(s.remainingMs(), 3590000);
        FinishOutcome o = s.finish(36000);  // silver: no goal yet
        CHECK(o.accepted && !o.counted && o.medal == Medal::Silver);
        CHECK(s.state() == State::Playing);
        CHECK(!s.canGoldSkip());
        o = s.finish(33000);  // gold: not the goal (Author) but gold skip becomes possible
        CHECK(o.accepted && !o.counted && o.medal == Medal::Gold);
        CHECK(s.canGoldSkip());
        CHECK_EQ(s.currentBestMs(), 33000);
        o = s.finish(34000);  // slower: best stays
        CHECK_EQ(s.currentBestMs(), 33000);
        o = s.finish(29999);  // author
        CHECK(o.counted && o.medal == Medal::Author);
        CHECK_EQ(s.authorCount(), 1);
        CHECK_EQ(s.goldCount(), 0);
        CHECK(s.state() == State::WaitingForMap);
        CHECK(!s.finish(1000).accepted);  // no map running
        CHECK_EQ(s.remainingMs(), 3590000);  // no bonus in RMC
        CHECK_EQ(s.history().size(), 1u);
        CHECK(s.history()[0].result == EntryResult::Author);
        CHECK_EQ(s.history()[0].bestMs, 29999);

        // gold skip
        s.mapStarted(mk(2));
        CHECK(!s.goldSkip());  // no gold yet
        s.finish(35000);
        CHECK(s.goldSkip());
        CHECK_EQ(s.goldCount(), 1);
        CHECK_EQ(s.authorCount(), 1);
        CHECK_EQ(s.skipCount(), 0);
        CHECK(s.history()[1].result == EntryResult::GoldSkip);
        CHECK(s.state() == State::WaitingForMap);

        // first skip free with refund, second plain without penalty
        s.mapStarted(mk(3));
        s.tick(20000);
        CHECK_EQ(s.remainingMs(), 3590000 - 20000);
        CHECK_EQ(s.freeSkipsLeft(), 1);
        CHECK(s.skip() == SkipKind::Free);
        CHECK_EQ(s.remainingMs(), 3590000);  // refunded
        CHECK_EQ(s.freeSkipsLeft(), 0);
        s.mapStarted(mk(4));
        s.tick(20000);
        CHECK(s.skip() == SkipKind::Normal);
        CHECK_EQ(s.remainingMs(), 3590000 - 20000);  // time lost, no extra penalty
        CHECK_EQ(s.skipCount(), 2);
        CHECK(s.history()[2].result == EntryResult::FreeSkipped);
        CHECK(s.history()[3].result == EntryResult::Skipped);
        CHECK(s.skip() == SkipKind::NotAllowed);  // nothing running

        // timeout ends the run and records the running map as unfinished
        s.mapStarted(mk(5));
        s.tick(3590000);
        CHECK(s.state() == State::Over);
        CHECK_EQ(s.remainingMs(), 0);
        CHECK(s.history().back().result == EntryResult::Unfinished);
        s.mapStarted(mk(6));  // ignored once over
        CHECK(s.state() == State::Over);
    }
    {
        // goal Gold: gold finish counts, no gold skip needed
        Session s(SessionConfig::rmc(Goal::Gold));
        s.start();
        s.mapStarted(mk(1));
        FinishOutcome o = s.finish(40000);
        CHECK(!o.counted);
        o = s.finish(34000);
        CHECK(o.counted && o.medal == Medal::Gold);
        CHECK_EQ(s.goldCount(), 1);
        CHECK_EQ(s.authorCount(), 0);
        s.mapStarted(mk(2));
        o = s.finish(29000);  // an author medal also meets a gold goal and counts as author
        CHECK(o.counted && o.medal == Medal::Author);
        CHECK_EQ(s.authorCount(), 1);
        s.mapStarted(mk(3));
        s.finish(34000);
        CHECK(!s.goldSkip());  // gold already counted as finish; map is over
    }
    {
        // map without known medal times: nothing can count, skip still works
        Session s(SessionConfig::rmc());
        s.start();
        s.mapStarted(mk(1, 0, 0, 0, 0));
        FinishOutcome o = s.finish(10000);
        CHECK(o.accepted && !o.counted && o.medal == Medal::None);
        CHECK(s.skip() == SkipKind::Free);
    }
    {
        // a new map while one is still running: the old one is logged as unfinished
        Session s(SessionConfig::rmc());
        s.start();
        s.mapStarted(mk(1));
        s.mapStarted(mk(2));
        CHECK_EQ(s.history().size(), 1u);
        CHECK(s.history()[0].result == EntryResult::Unfinished);
        CHECK(s.currentMap() && s.currentMap()->id == 2);
        s.stop();
        CHECK(s.state() == State::Over);
        CHECK_EQ(s.history().size(), 2u);
    }

    section("session: RMS");
    {
        Session s(SessionConfig::rms());
        s.start();
        CHECK_EQ(s.remainingMs(), 15 * 60000);
        s.mapStarted(mk(1));
        s.tick(60000);
        CHECK(s.finish(29000).counted);
        CHECK_EQ(s.remainingMs(), 15 * 60000 - 60000 + 3 * 60000);  // +3 min per author medal
        s.mapStarted(mk(2));
        CHECK(s.skip() == SkipKind::Free);  // first skip: no penalty, no refund
        CHECK_EQ(s.remainingMs(), 17 * 60000);
        s.mapStarted(mk(3));
        CHECK(s.skip() == SkipKind::Normal);
        CHECK_EQ(s.remainingMs(), 16 * 60000);  // -1 min
        s.mapStarted(mk(4));
        s.finish(34000);
        CHECK(s.goldSkip());
        CHECK_EQ(s.remainingMs(), 17 * 60000);  // +1 min for gold
        CHECK_EQ(s.goldCount(), 1);
        CHECK_EQ(s.skipCount(), 2);
        // skip penalty can end the run
        s.mapStarted(mk(5));
        s.tick(16 * 60000 + 30000);
        CHECK_EQ(s.remainingMs(), 30000);
        s.mapStarted(mk(6));
        CHECK(s.skip() == SkipKind::Normal);
        CHECK(s.state() == State::Over);
    }

    section("session: free play");
    {
        Session s(SessionConfig::freePlay());
        s.start();
        s.mapStarted(mk(1));
        s.tick(7 * 3600000 / 7);
        CHECK(s.state() == State::Playing);
        CHECK_EQ(s.elapsedMs(), 3600000);
        CHECK_EQ(s.remainingMs(), 0);
        CHECK(s.skip() == SkipKind::Free);
        s.mapStarted(mk(2));
        CHECK(s.skip() == SkipKind::Free);  // unlimited, never penalised
        s.mapStarted(mk(3));
        CHECK(s.finish(29000).counted);
        CHECK_EQ(s.authorCount(), 1);
        CHECK(s.state() == State::WaitingForMap);
    }
}

// ---------------------------------------------------------------- live
static void testLive() {
    section("live TMX (one random map per site, downloads 1 file)");
    const Site sites[] = {Site::TMNF, Site::TMUF, Site::TMN_ESWC, Site::TMS, Site::TMO};
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    for (Site s : sites) {
        MapInfo m;
        TmxFilter f;
        TmxError e = getRandomMap(s, f, m);
        if (!e.ok()) { std::printf("  FAIL %s random: %s\n", siteName(s), e.message.c_str()); ++g_fails; continue; }
        ++g_checks;
        std::printf("  %-10s id %lld \"%s\" by %s env %d AT %d G %d S %d B %d\n", siteName(s), m.id,
                    stripTmCodes(m.name).c_str(), m.author.c_str(), m.environment, m.authorMs, m.goldMs, m.silverMs,
                    m.bronzeMs);
        CHECK(m.authorMs > 0 && m.id > 0 && !m.uid.empty());
        if (s == Site::TMNF || s == Site::TMO) {
            std::string path = std::string(tmp) + "lr_core_test_" + std::to_string(m.id) + ".Challenge.Gbx";
            std::string bytes;
            e = downloadMap(s, m.id, path, &bytes);
            if (!e.ok()) { std::printf("  FAIL download: %s\n", e.message.c_str()); ++g_fails; continue; }
            GbxMapHeader h = readChallengeHeaderFile(path);
            CHECK(h.ok);
            if (h.ok) {
                // the file header must agree with the API: same uid, same author time
                CHECK_EQ(h.uid, m.uid);
                CHECK_EQ(h.authorMs, m.authorMs);
                CHECK_EQ(h.goldMs, m.goldMs);
                std::printf("    gbx ok: uid %s, A %d G %d (binary times: %d)\n", h.uid.c_str(), h.authorMs, h.goldMs,
                            h.timesFromBinary ? 1 : 0);
            } else {
                std::printf("    gbx parse: %s\n", h.error.c_str());
            }
            DeleteFileA(path.c_str());
        }
    }
    // a filtered request and an impossible one
    TmxFilter f;
    f.authorTimeMaxMs = 20000;
    f.tag = 1;
    MapInfo m;
    TmxError e = getRandomMap(Site::TMNF, f, m);
    CHECK(e.ok());
    if (e.ok()) CHECK(m.authorMs <= 20000);
    f = TmxFilter();
    f.authorTimeMinMs = 5;
    f.authorTimeMaxMs = 4;  // empty window
    e = getRandomMap(Site::TMNF, f, m);
    CHECK(!e.ok());
    std::printf("  impossible filter -> \"%s\"\n", e.message.c_str());
    // bad id
    e = downloadMap(Site::TMNF, 1, std::string(tmp) + "lr_core_test_bad.Challenge.Gbx");
    std::printf("  download of id 1 -> \"%s\"\n", e.ok() ? "ok" : e.message.c_str());
}

int main(int argc, char** argv) {
    bool live = false;
    fs::path maps = R"(C:\Users\osaro\Games\TMN-ESWC-inoffiziell\GameData\Tracks\Challenges\Downloaded\_NostalgicCompletionist)";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--live") == 0) live = true;
        else if (std::strcmp(argv[i], "--maps") == 0 && i + 1 < argc) maps = argv[++i];
    }
    testJson();
    testTmxHelpers();
    testGbx(maps);
    testSession();
    if (live) testLive();
    std::printf("\n%d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
