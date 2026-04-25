#ifndef MAPFETCH_H
#define MAPFETCH_H

#include <string>

// FetchMapFromServer downloads map `mapname` from the community map API at
// `apiURL` using SHA-1 hash verification.  The file is saved to
// `level/download/<mapname>` under the data directory.
//
// Returns the full path of the saved file on success, or an empty string on
// failure (network error, hash mismatch, or file too large).  The call is
// synchronous with a short timeout (3 s connect / 10 s transfer) and is
// intended to be called at most once per game-join attempt.
std::string FetchMapFromServer(const char * mapname,
                               const unsigned char * sha1hash,
                               const char * apiURL);

// FetchAndSyncServerMaps queries GET /api/maps from the community map server
// and downloads any maps not already present in level/download/ on disk.
// Called once when the Create Game screen opens to populate community maps.
void FetchAndSyncServerMaps(const char * apiURL);

#endif // MAPFETCH_H
