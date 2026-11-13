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
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")  // the hanging local server of the cancel test

#include "../src/core/gbx.h"
#include "../src/core/json.h"
#include "../src/core/session.h"
#include "../src/core/timefmt.h"
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
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom?inunlimiter=0&authortimemin=1&primarytype=0"));
    f.environment = 7; f.difficulty = 2; f.tag = 5; f.authorTimeMaxMs = 60000;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom?inunlimiter=0&authortimemin=1&authortimemax=60000&environment=6&difficulty=2&tag=5&primarytype=0"));
    f = TmxFilter();
    f.excludeUnlimiter = false; f.requireAuthorTime = false; f.primaryType = -1;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom"));
    // RandomizerTMF parity filters
    f.name = "a b&c"; f.author = "Chakal"; f.moodMask = 2 | 8; f.routesMask = 2; f.lbType = 1; f.vehicle = 1;
    f.uploadedAfter = "2024-01-01"; f.flags[FlagScreenshot] = 1; f.flags[FlagFeatured] = 0;
    CHECK_EQ(buildRandomPath(f), std::string("/trackrandom?name=a%20b%26c&author=Chakal&mood=1%2C3&routes=1&lbtype=1&vehicle=0"
                                             "&uploadedafter=2024-01-01&inscreenshot=1&infeatured=0"));
    {
        MapInfo mi;
        mi.authorMs = 30000; mi.mood = 2; mi.routes = 1; mi.primaryType = 0; mi.car = 1;
        TmxFilter g;
        g.moodMask = 4;  // sunset
        CHECK(matchesFilter(mi, g));
        g.moodMask = 2;  // day only
        CHECK(!matchesFilter(mi, g));
        g = TmxFilter();
        mi.primaryType = 3;  // stunts
        CHECK(!matchesFilter(mi, g));
    }

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

// HttpCancel: a request that hangs (a local server that accepts and never answers) returns as soon as
// another thread cancels it, not after its timeout; a cancelled token fails new requests at once.
static void testHttpCancel() {
    section("http cancel");
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { std::printf("  SKIP (no winsock)\n"); return; }
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int len = sizeof a;
    bool up = ls != INVALID_SOCKET && bind(ls, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0 && listen(ls, 4) == 0 &&
              getsockname(ls, reinterpret_cast<sockaddr*>(&a), &len) == 0;
    CHECK(up);
    if (!up) { WSACleanup(); return; }
    std::vector<SOCKET> held;
    std::thread acceptor([&] {
        for (;;) {
            SOCKET c = accept(ls, nullptr, nullptr);
            if (c == INVALID_SOCKET) break;
            held.push_back(c);
        }
    });
    setTestEndpoint("127.0.0.1", ntohs(a.sin_port));
    HttpOptions o;
    o.timeoutMs = 8000;
    o.retries = 1;
    o.cancel = std::make_shared<HttpCancel>();
    TmxError e;
    LARGE_INTEGER f, t0, t1, t2;
    QueryPerformanceFrequency(&f);
    std::thread worker([&] {
        MapInfo info;
        e = getMapInfo(Site::TMNF, 1, info, o);
    });
    Sleep(300);  // the request is waiting for an answer now
    QueryPerformanceCounter(&t0);
    o.cancel->cancel();
    QueryPerformanceCounter(&t1);
    worker.join();
    QueryPerformanceCounter(&t2);
    double cancelMs = (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart, returnMs = (t2.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
    std::printf("  cancel() %.2f ms, the hanging request returned after %.2f ms (timeout 8000 ms x 2)\n", cancelMs, returnMs);
    CHECK(!e.ok());
    CHECK(cancelMs < 16.0);
    CHECK(returnMs < 1000.0);
    MapInfo info;
    QueryPerformanceCounter(&t0);
    CHECK(getMapInfo(Site::TMNF, 1, info, o).message == "cancelled");  // fails at once afterwards
    QueryPerformanceCounter(&t1);
    CHECK((t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart < 16.0);
    setTestEndpoint("", 0);
    closesocket(ls);
    acceptor.join();
    for (SOCKET c : held) closesocket(c);
    WSACleanup();
}

// The map's file name is sanitised, the folder is not: TMLoader's data folder is "...\Horizon Host\data\..."
static void testMapFiles() {
    section("map file names and the download write");
    for (Site s : {Site::TMNF, Site::TMUF, Site::TMN_ESWC, Site::TMS, Site::TMO}) {
        std::string n = mapFileName(s, 123456);
        CHECK(!n.empty());
        CHECK(n.find(' ') == std::string::npos);
        CHECK(n.find('\\') == std::string::npos && n.find('/') == std::string::npos);
        CHECK(n.find("123456") != std::string::npos);
        CHECK(n.size() > 14 && n.compare(n.size() - 14, 14, ".Challenge.Gbx") == 0);
    }
    std::error_code ec;
    fs::path base = fs::temp_directory_path(ec) / "lr test dir with spaces";
    fs::remove_all(base, ec);
    fs::path dirW = base / "Horizon Host" / "data" / "maps";
    CHECK(fs::create_directories(dirW, ec));
    std::string dir = dirW.u8string() + "\\";
    CHECK(dir.find(' ') != std::string::npos);
    const std::string bytes = std::string("GBX\x06\x00", 5) + "fake map body";
    std::string path = dir + mapFileName(Site::TMS, 42);
    CHECK(writeMapFile(path, bytes).ok());
    fs::path expect = dirW / fs::u8path(mapFileName(Site::TMS, 42));
    CHECK(fs::exists(expect, ec));
    CHECK(!fs::exists(fs::u8path(path + ".part"), ec));
    CHECK_EQ(readAll(expect), bytes);
    CHECK(writeMapFile(path, bytes + "x").ok());  // replaces an existing file
    CHECK_EQ(readAll(expect), bytes + "x");
    CHECK(!writeMapFile((base / "missing folder" / "x.Gbx").u8string(), bytes).ok());
    fs::remove_all(base, ec);
    CHECK(!fs::exists(base, ec));
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

// ---------------------------------------------------------------- replay headers (finished-before rule)
static void testReplays(const std::vector<fs::path>& dirs) {
    section("replay map uid");
    CHECK(replayMapUid(reinterpret_cast<const unsigned char*>("nope"), 4).empty());
    for (const fs::path& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) { std::printf("  SKIP %s (not found)\n", dir.string().c_str()); continue; }
        int files = 0, withUid = 0;
        std::string sample;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file() || _stricmp(e.path().extension().string().c_str(), ".gbx") != 0) continue;
            if (++files > 200) break;
            std::string bytes = readAll(e.path());
            std::string uid = replayMapUid(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
            if (!uid.empty()) { ++withUid; if (sample.empty()) sample = uid; }
        }
        std::printf("  %s: %d replays, %d with a map uid (e.g. %s)\n", dir.string().c_str(), files > 200 ? 200 : files, withUid, sample.c_str());
        CHECK(files == 0 || withUid == (files > 200 ? 200 : files));
    }
}

// ---------------------------------------------------------------- auto-skip goals (RandomizerTMF modes)
static void testGoals() {
    section("session: auto-skip goals");
    CHECK_EQ(std::string(Session::goalKey(Goal::Silver)), std::string("silver"));
    CHECK(Session::goalFromKey("finished") == Goal::Finished);
    CHECK(Session::goalFromKey("nonsense") == Goal::Author);
    {
        SessionConfig c = SessionConfig::rmc(Goal::Silver);
        Session s(c);
        s.start();
        s.mapStarted(mk(1));
        CHECK(!s.finish(45000).counted);  // bronze only
        FinishOutcome o = s.finish(39000);
        CHECK(o.counted && o.medal == Medal::Silver);
        CHECK_EQ(s.finishedCount(), 1);
        CHECK(s.history().back().result == EntryResult::Finished);
        s.mapStarted(mk(2));
        o = s.finish(34000);  // gold also meets a silver goal and counts as gold
        CHECK(o.counted && o.medal == Medal::Gold);
        CHECK_EQ(s.goldCount(), 1);
        CHECK(!s.canGoldSkip());  // gold skip only with goal Author
    }
    {
        Session s(SessionConfig::rmc(Goal::Finished));
        s.start();
        s.mapStarted(mk(1));
        FinishOutcome o = s.finish(99000);  // slower than bronze: still a finish
        CHECK(o.counted && o.medal == Medal::None);
        CHECK_EQ(s.finishedCount(), 1);
    }
    {
        Session s(SessionConfig::rmc(Goal::Bronze));
        s.start();
        s.mapStarted(mk(1));
        CHECK(!s.finish(60000).counted);
        CHECK(s.finish(50000).counted);
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

static void testDurations() {
    int ms = -1;
    CHECK(parseDurationMs("", &ms) && ms == 0);
    CHECK(parseDurationMs("  ", &ms) && ms == 0);
    CHECK(parseDurationMs("0:30", &ms) && ms == 30000);
    CHECK(parseDurationMs("1:30", &ms) && ms == 90000);
    CHECK(parseDurationMs("12:05", &ms) && ms == 725000);
    CHECK(parseDurationMs("1:30.250", &ms) && ms == 90250);
    CHECK(parseDurationMs("0:42.5", &ms) && ms == 42500);
    CHECK(parseDurationMs("1:02:03", &ms) && ms == 3723000);
    CHECK(parseDurationMs("1:02:03.004", &ms) && ms == 3723004);
    CHECK(parseDurationMs(" 1:30 ", &ms) && ms == 90000);
    ms = 7;
    CHECK(!parseDurationMs("90", &ms));
    CHECK(!parseDurationMs("1:3", &ms));
    CHECK(!parseDurationMs("1:60", &ms));
    CHECK(!parseDurationMs("1:30.", &ms));
    CHECK(!parseDurationMs("1:30.1234", &ms));
    CHECK(!parseDurationMs("1:2:03", &ms));
    CHECK(!parseDurationMs("a:30", &ms));
    CHECK(!parseDurationMs("1:30:00:00", &ms));
    CHECK_EQ(ms, 7);
    CHECK_EQ(formatDurationMs(0), std::string());
    CHECK_EQ(formatDurationMs(30000), std::string("0:30"));
    CHECK_EQ(formatDurationMs(90250), std::string("1:30.250"));
    CHECK_EQ(formatDurationMs(3723000), std::string("1:02:03"));
    for (int v : {30000, 90250, 599999, 3723004}) {
        int back = 0;
        CHECK(parseDurationMs(formatDurationMs(v), &back) && back == v);
    }
}

int main(int argc, char** argv) {
    bool live = false;
    // LR_TEST_ESWC_DIR: an ESWC game folder (with GameData\) for the tests on real maps and replays
    fs::path eswc;
    {
        wchar_t* v = nullptr;
        size_t n = 0;
        if (_wdupenv_s(&v, &n, L"LR_TEST_ESWC_DIR") == 0 && v) {
            if (*v) eswc = v;
            free(v);
        }
    }
    fs::path maps;
    if (!eswc.empty()) maps = eswc / "GameData" / "Tracks" / "Challenges" / "Downloaded" / "_NostalgicCompletionist";
    else std::printf("note: LR_TEST_ESWC_DIR is not set, the tests on the game's own maps and autosaves are skipped\n");
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--live") == 0) live = true;
        else if (std::strcmp(argv[i], "--maps") == 0 && i + 1 < argc) maps = argv[++i];
    }
    testJson();
    testTmxHelpers();
    testMapFiles();
    testHttpCancel();
    testGbx(maps);
    testSession();
    testGoals();
    testDurations();
    {
        std::vector<fs::path> replayDirs;
        if (!eswc.empty()) replayDirs.push_back(eswc / "GameData" / "Tracks" / "Replays" / "Autosaves");
        wchar_t* docs = nullptr;
        size_t n = 0;
        if (_wdupenv_s(&docs, &n, L"USERPROFILE") == 0 && docs) {
            replayDirs.push_back(fs::path(docs) / "Documents" / "TrackMania" / "Tracks" / "Replays" / "Autosaves");
            free(docs);
        }
        testReplays(replayDirs);
    }
    if (live) testLive();
    std::printf("\n%d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
