#include "map_downloader.h"
#include "world.h"
#include "peer.h"
#include "config.h"
#include "mapfetch.h"
#include "os.h"
#include "sha1.h"
#include <array>
#include <cstring>
#include <stdio.h>

MapDownloader::MapDownloader(World & w) : world(w) {}

MapDownloader::~MapDownloader(){
	JoinAndShutdown();
}

void MapDownloader::JoinAndShutdown(){
	// Increment generation first so any in-flight result is discarded after
	// the join returns.
	mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
	if(mapjointhread.joinable()) mapjointhread.join();
	if(dlthread.joinable()) dlthread.join();
	mapUploadGeneration.fetch_add(1, std::memory_order_relaxed);
	if(mapUploadThread.joinable()) mapUploadThread.join();
}

std::vector<std::string> MapDownloader::ListFiles(const char * directory){
	std::vector<std::string> files;
#ifdef POSIX
	DIR * dir = opendir(directory);
	if(dir){
		dirent * info;
		while((info = readdir(dir))){
			struct stat st;
			char filename[PATH_MAX];
			strcpy(filename, directory);
			strcat(filename, "/");
			strcat(filename, info->d_name);
			if(stat(filename, &st) == 0){
				if(info->d_type != DT_DIR && !S_ISDIR(st.st_mode) && info->d_name[0] != '.'){
					files.push_back(std::string(info->d_name));
				}
			}
		}
		closedir(dir);
	}
#elif _WIN32
	WIN32_FIND_DATA info;
	char directory2[MAX_PATH];
	strcpy(directory2, directory);
	strcat(directory2, "\\*");
	HANDLE dir = FindFirstFile(directory2, &info);
	if(dir != INVALID_HANDLE_VALUE){
		do{
			char fullname[MAX_PATH];
			snprintf(fullname, sizeof fullname, "%s\\%s", directory, info.cFileName);
			if(!(GetFileAttributes(fullname) & FILE_ATTRIBUTE_DIRECTORY)){
				files.push_back(std::string(info.cFileName));
			}
		}while(FindNextFile(dir, &info));
		FindClose(dir);
	}
#endif
	return files;
}

std::string MapDownloader::FindMap(const char * name, unsigned char (*hash)[20], const char * directory){
	if(!directory){
		std::string result;
		result = FindMap(name, hash, "level");
		if(result.length() > 0){
			return result;
		}
		result = FindMap(name, hash, "level/download");
		if(result.length() > 0){
			return result;
		}
		result = FindMap(name, hash, "level/archive");
		if(result.length() > 0){
			return result;
		}
	}else{
		bool isarchive = false;
		if(strcmp(directory, "level/archive") == 0){
			isarchive = true;
		}
		CDResDir();
		// Capture the absolute resources path while cwd = Resources
		// (GetResDir returns "" on macOS).
		std::string absResDir = GetResDir();
		if(absResDir.empty()){
			char _cwd[PATH_MAX];
			if(getcwd(_cwd, PATH_MAX)) absResDir = std::string(_cwd) + "/";
		}
		std::vector<std::string> files = ListFiles((GetResDir() + directory).c_str());
		CDDataDir();
		std::vector<std::string> files2 = ListFiles((GetDataDir() + directory).c_str());
		files.insert(files.end(), files2.begin(), files2.end());
		for(std::vector<std::string>::iterator it = files.begin(); it != files.end(); it++){
			std::string cname = (*it);
			if(isarchive){
				std::size_t p = cname.find_first_of(".");
				if(p != std::string::npos){
					cname.erase(0, p + 1);
				}
			}
			if(cname.compare(name) == 0){
				std::string filename = GetDataDir() + directory;
				filename.append("/");
				filename.append(*it);
				SDL_IOStream * file = SDL_IOFromFile(filename.c_str(), "rb");
				if(!file){
					filename = absResDir + directory;
					filename.append("/");
					filename.append(*it);
				}else{
					SDL_CloseIO(file);
				}
				if(!hash){
					return filename;
				}else{
					unsigned char filehash[20];
					CalculateMapHash(filename.c_str(), &filehash);
					if(memcmp(*hash, filehash, sizeof(*hash)) == 0){
						return filename;
					}
				}
			}
		}
	}
	std::string empty;
	return empty;
}

std::string MapDownloader::SaveMap(const char * name, unsigned char * data, int size){
	CDDataDir();
	std::string filename = GetDataDir() + "level/download/";
	CreateDirectory((GetDataDir() + "level/download").c_str());
	CreateDirectory((GetDataDir() + "level/archive").c_str());
	filename.append(name);
	SDL_IOStream * file = SDL_IOFromFile(filename.c_str(), "wb");
	if(file){
		SDL_WriteIO(file, data, size);
		SDL_CloseIO(file);
	}
	unsigned char hash[20];
	CalculateMapHash(filename.c_str(), &hash);
	std::string archivefilename = GetDataDir() + "level/archive/";
	archivefilename.append(StringFromHash(&hash));
	archivefilename.append(".");
	archivefilename.append(name);
	file = SDL_IOFromFile(archivefilename.c_str(), "wb");
	if(file){
		SDL_WriteIO(file, data, size);
		SDL_CloseIO(file);
	}
	return filename;
}

bool MapDownloader::CalculateMapHash(const char * filename, unsigned char (*hash)[20]){
	std::vector<Uint8> mapdata(65535);
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile(filename, "rb");
	if(!file){
		CDResDir();
		file = SDL_IOFromFile(filename, "rb");
	}
	if(file){
		int mapdatasize = SDL_ReadIO(file, mapdata.data(), mapdata.size());
		SDL_CloseIO(file);
		sha1::calc(mapdata.data(), mapdatasize, *hash);
		return true;
	}
	return false;
}

std::string MapDownloader::StringFromHash(unsigned char (*hash)[20]){
	char hashstring[(20 * 2) + 1];
	memset(hashstring, 0, sizeof(hashstring));
	for(int i = 0; i < 20; i++){
		unsigned char byte = (*hash)[i];
		snprintf(&hashstring[i * 2], sizeof(hashstring) - i * 2, "%.2X", byte);
	}
	return std::string(hashstring);
}

void MapDownloader::LoadMapData(const char * filename){
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile((GetDataDir() + filename).c_str(), "rb");
	if(!file){
		CDResDir();
		file = SDL_IOFromFile((GetResDir() + filename).c_str(), "rb");
	}
	if(file){
		int length = SDL_ReadIO(file, world.currentmapdata.data(), world.currentmapdata.size());
		world.currentmapdata.resize(length);
		world.currentmapdataend = true;
		SDL_CloseIO(file);
	}
}

void MapDownloader::ProcessMapDownload(void){
	Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
	if(localpeer){
		if(localpeer->gameinfoloaded){
			if(!localpeer->mapdownloaded){
				if(!mapexistchecked){
					std::string mapfilename = FindMap(world.gameinfo.mapname, &world.gameinfo.maphash);
					if(mapfilename.size() > 0){
						world.SendMapDownloaded();
						LoadMapData(mapfilename.c_str());
						mapexistchecked = true;
					} else {
						// Map not available locally. Try the community map server
						// asynchronously so the game loop (and UDP keepalives) stay
						// responsive during the fetch. Falling back to peer-to-peer
						// chunk transfer happens once the async result is known.
						int js = mapjoinstate.load(std::memory_order_acquire);
						if(js == 0){
							mapjoinstate.store(1, std::memory_order_relaxed);
							uint32_t gen = mapjoingeneration.load(std::memory_order_relaxed);
							std::string dlname = world.gameinfo.mapname;
							std::array<unsigned char, 20> sha1;
							memcpy(sha1.data(), world.gameinfo.maphash, 20);
							std::string apiURL = Config::GetInstance().mapapiurl;
							mapjointhread = std::thread([this, dlname, sha1, apiURL, gen]() mutable {
								std::string path = FetchMapFromServer(dlname.c_str(), sha1.data(), apiURL.c_str());
								if(mapjoingeneration.load(std::memory_order_relaxed) != gen) return;
								std::lock_guard<std::mutex> lk(mapjoinmutex);
								mapjoinpath = path;
								mapjoinstate.store(path.empty() ? 3 : 2, std::memory_order_release);
							});
						} else if(js == 2){
							// Server had the map; hand it to the engine.
							std::string path;
							{ std::lock_guard<std::mutex> lk(mapjoinmutex); path = mapjoinpath; }
							world.SendMapDownloaded();
							LoadMapData(path.c_str());
							mapexistchecked = true;
						} else if(js == 3){
							// Server doesn't have it; fall through to P2P chunk transfer.
							mapexistchecked = true;
						}
						// js == 1: fetch in progress; come back next tick.
					}
				}else{
					if(!world.currentmapdataprocessed || world.tickcount - lastmapchunkrequest > 24){
						// request map chunks after received, or if last request was a while ago
						if(world.currentmapdataend){
							std::string mapfilename = SaveMap(world.gameinfo.mapname, world.currentmapdata.data(), world.currentmapdata.size());
							world.SendMapDownloaded();
							LoadMapData(mapfilename.c_str());
						}else{
							world.GetMapChunk(world.currentmapdata.size());
							world.currentmapdataprocessed = true;
						}
					}
					if(world.currentmapdataend && world.tickcount % 48 == 0){
						// this is just in case the SendMapDownloaded packet is lost
						if(FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).size() > 0){
							world.SendMapDownloaded();
						}
					}
				}
			}
		}
	}
}
