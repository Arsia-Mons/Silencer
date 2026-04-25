#ifndef MAPFETCH_H
#define MAPFETCH_H

#include <atomic>
#include <string>
#include <vector>

// FetchMapFromServer downloads map `mapname` from the community map API at
// `apiURL` using SHA-1 hash verification.  The file is saved to
// `level/download/<mapname>` under the data directory.
// If `progress` is non-null, it is updated 0-100 during transfer.
//
// Returns the full path of the saved file on success, or an empty string on
// failure.  Safe to call from a background thread.
std::string FetchMapFromServer(const char * mapname,
                               const unsigned char * sha1hash,
                               const char * apiURL,
                               std::atomic<int> * progress = nullptr);

// FetchServerMapList queries GET /api/maps and returns (name, sha1hex) pairs
// for all community maps on the server. No files are written to disk.
std::vector<std::pair<std::string, std::string>> FetchServerMapList(const char * apiURL);

// FetchAndSyncServerMaps queries GET /api/maps from the community map server
// and downloads any maps not already present in level/download/ on disk.
// Called once when the Create Game screen opens to populate community maps.
void FetchAndSyncServerMaps(const char * apiURL);

#endif // MAPFETCH_H
