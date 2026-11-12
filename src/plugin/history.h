// Saved runs: one JSON file per run in <data folder>\runs\run-YYYYMMDD-HHMMSS.json.
#pragma once
#include <set>
#include <string>
#include <vector>

#include "../core/session.h"

namespace lr {

struct RunMapRecord {
    long long tmxId = 0;
    std::string uid, name /* raw, $ codes */, author;
    std::string result;      // author, gold, gold_skip, skipped, free_skip, unfinished
    std::string medal;       // none, bronze, silver, gold, author
    int bestMs = 0, spentMs = 0;
    MapMedals medals;
    std::string replay;      // path of a saved replay, "" none
    std::string site;        // TMX host of this map ("" in older files: the run's site)
};

struct RunRecord {
    std::string file;        // file name (not saved inside)
    std::string game, site, mode, goal, started, ended, endedBy;
    int timeLimitMs = 0, elapsedMs = 0;
    int authors = 0, golds = 0, skips = 0;
    std::vector<RunMapRecord> maps;
};

const char* resultKey(EntryResult r);
std::string runToJson(const RunRecord& r);
bool runFromJson(const std::string& text, RunRecord& out);

class History {
public:
    void setFolder(const std::string& dataFolder);  // <data>\runs\ is used
    std::string runsFolder() const { return folder_; }
    void reload();                                  // reads every run file (newest first)
    bool save(RunRecord& r, std::string* pathOut);  // sets r.file
    // the in-memory part of save(): sets r.file, lists the run, returns the file's path ("" no folder);
    // the caller writes runToJson(r) there (writeRun, any thread)
    std::string add(RunRecord& r);
    static bool writeRun(const std::string& path, const std::string& json);
    const std::vector<RunRecord>& runs() const { return runs_; }
    std::set<long long> playedIds(const std::string& site) const;  // TMX ids of every map in saved runs of that site
    std::set<long long> skippedIds(const std::string& site) const; // TMX ids skipped in saved runs of that site

private:
    std::string folder_;
    std::vector<RunRecord> runs_;
};

} // namespace lr
