// Replay folders of the "never a map you finished before" rule: folder list helpers and the scan.
#pragma once
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace lr {

// "C:/a//b" -> "C:\a\b\"; trims spaces and quotes; "" for an empty or unusable entry.
std::string normalizeFolder(const std::string& s);
// The setting is one string, folders separated by '|' (cannot occur in a Windows path).
std::vector<std::string> parseFolderList(const std::string& s);   // normalised, no duplicates
std::string joinFolderList(const std::vector<std::string>& dirs);
// Adds a folder to the list; false when it is empty or already there (case-insensitive).
bool addFolder(std::vector<std::string>& dirs, const std::string& entry);
// `defaults` first, then `extra`, normalised, each folder once (case-insensitive).
std::vector<std::string> mergeFolders(const std::vector<std::string>& defaults, const std::vector<std::string>& extra);

// Number of *.gbx files directly in the folder; -1 when the folder does not exist.
int countReplays(const std::string& dir);

struct ScanResult {
    int files = 0;  // replays seen
    int read = 0;   // replays read from disk (the rest came from the cache)
    bool stopped = false;
};
// Map uids of every replay in `dirs` (*.gbx, not recursive, like the default folders always were). Cache line:
// full path|size|write time|uid, so the same file name in two folders never collides. Missing folders are
// skipped silently. `cachePath` "" = no cache.
ScanResult scanReplayFolders(const std::vector<std::string>& dirs, const std::string& cachePath, std::set<std::string>& finished,
                             const std::function<bool()>& shouldStop);

} // namespace lr
