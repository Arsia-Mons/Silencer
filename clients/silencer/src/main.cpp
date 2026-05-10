#include "shared.h"
#include "game.h"
#include "cocoawrapper.h"
#include "updaterstage2.h"
#include <vector>
#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#include <mach-o/dyld.h>
#endif
#ifdef POSIX
#include <execinfo.h>
#include <signal.h>
static void crash_handler(int sig){
	void * frames[64];
	int n = backtrace(frames, 64);
	fprintf(stderr, "[ds] SIGNAL %d — backtrace (%d frames):\n", sig, n);
	backtrace_symbols_fd(frames, n, 2);
	fflush(stderr);
	signal(sig, SIG_DFL);
	raise(sig);
}
#endif

#ifdef __ANDROID__
JNIEnv * jenv;
JavaVM * jvm;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM * vm, void * pvt){
	//printf("* JNI_OnLoad called\n");
	jvm = vm;
	if(jvm->AttachCurrentThread(&jenv, NULL) != JNI_OK){
		//printf("AttachCurrentThread failed\n");
	}
	return JNI_VERSION_1_6;
}

extern "C" void Java_com_silencer_game_Silencer_SetPath(JNIEnv * env, jclass cls, jobject path){
	const char * pathstring = env->GetStringUTFChars((jstring)path, NULL);
	chdir(pathstring);
}

#ifdef OUYA
extern "C" void Java_com_silencer_game_Silencer_OuyaControllerKeyEvent(JNIEnv * env, jclass cls, jint player, jint type, jint keycode){
	static SDL_Event pushedevent;
	if(type == 1){
		pushedevent.type = SDL_EVENT_KEY_DOWN;
		//printf("native ouya key down %d\n", keycode);
	}else{
		pushedevent.type = SDL_EVENT_KEY_UP;
	}
	int keycode2 = keycode;
	switch(keycode){
		case 17: keycode2 = SDL_SCANCODE_LALT; break; // L2
		case 18: keycode2 = SDL_SCANCODE_RALT; break; // R2
		case 19: keycode2 = SDL_SCANCODE_UP; break;
		case 20: keycode2 = SDL_SCANCODE_DOWN; break;
		case 21: keycode2 = SDL_SCANCODE_LEFT; break;
		case 22: keycode2 = SDL_SCANCODE_RIGHT; break;
		case 82: keycode2 = SDL_SCANCODE_HOME; break; // Menu
		case 96: keycode2 = SDL_SCANCODE_RETURN; break; // O
		case 97: keycode2 = SDL_SCANCODE_ESCAPE; break; // A
		case 200: keycode2 = SDL_SCANCODE_KP_2; break; // RUp
		case 201: keycode2 = SDL_SCANCODE_KP_4; break; // RLeft
		case 202: keycode2 = SDL_SCANCODE_KP_6; break; // RRight
		case 203: keycode2 = SDL_SCANCODE_KP_8; break; // RDown
	}
	pushedevent.key.scancode = (SDL_Scancode)keycode2;
	SDL_PushEvent(&pushedevent);
}
#endif

#endif

void CDDataDir(void){
#ifdef __APPLE__
	char path[PATH_MAX];
	sprintf(path, "%s/Silencer", GetAppSupportDirectory());
	mkdir(path, 0777);
	chdir(path);
#endif
}

static char resdir[PATH_MAX] = {0};

void CDResDir(void){
#ifdef __APPLE__
	if(resdir[0]){
		chdir(resdir);
		return;
	}
	// Try bundle resources first
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	if(mainBundle){
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
		if(resourcesURL){
			char path[PATH_MAX];
			if(CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)){
				CFRelease(resourcesURL);
				// Check if asset files exist in the bundle resource dir
				char testpath[PATH_MAX];
				snprintf(testpath, PATH_MAX, "%s/PALETTE.BIN", path);
				FILE *f = fopen(testpath, "r");
				if(f){
					fclose(f);
					strcpy(resdir, path);
					chdir(resdir);
					return;
				}
			}else{
				CFRelease(resourcesURL);
			}
		}
	}
	// Fallback: look for assets/ relative to the executable
	char exepath[PATH_MAX];
	uint32_t exesize = PATH_MAX;
	if(_NSGetExecutablePath(exepath, &exesize) == 0){
		char *lastslash = strrchr(exepath, '/');
		if(lastslash) *lastslash = 0;
		char testpath[PATH_MAX];
		// Check ../assets (build/silencer -> assets/)
		snprintf(testpath, PATH_MAX, "%s/../assets/PALETTE.BIN", exepath);
		FILE *f = fopen(testpath, "r");
		if(f){
			fclose(f);
			snprintf(resdir, PATH_MAX, "%s/../assets", exepath);
			chdir(resdir);
			return;
		}
		// Check ./assets
		snprintf(testpath, PATH_MAX, "%s/assets/PALETTE.BIN", exepath);
		f = fopen(testpath, "r");
		if(f){
			fclose(f);
			snprintf(resdir, PATH_MAX, "%s/assets", exepath);
			chdir(resdir);
			return;
		}
	}
#endif
}

#ifdef _WIN32
// Inno Setup writes uninstall metadata under HKCU at this AppId-derived key
// (see clients/silencer/installer/silencer.iss — must stay in sync). Stage-2
// only swaps files; without this, Add/Remove Programs keeps showing the
// install-time DisplayVersion forever.
static void SyncInstalledVersionRegistry(void) {
#ifndef SILENCER_VERSION
#define SILENCER_VERSION "00000"
#endif
	HKEY key;
	const char *path = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{F6A1252E-1BF3-4768-ABD8-C1A9C140E459}_is1";
	if (RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
	const char *ver = SILENCER_VERSION;
	RegSetValueExA(key, "DisplayVersion", 0, REG_SZ,
		(const BYTE*)ver, (DWORD)strlen(ver) + 1);
	// Pre-AppVerName installers wrote DisplayName="Silencer version <ver>".
	// Overwrite so existing installs match the new bare-name convention.
	const char *name = "Silencer";
	RegSetValueExA(key, "DisplayName", 0, REG_SZ,
		(const BYTE*)name, (DWORD)strlen(name) + 1);
	RegCloseKey(key);
}

static void SweepSidelinedFiles(const std::string &dir) {
	WIN32_FIND_DATAA fd;
	std::string pattern = dir + "\\*";
	HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		std::string n = fd.cFileName;
		if (n == "." || n == "..") continue;
		std::string child = dir + "\\" + n;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			SweepSidelinedFiles(child);
		} else if (n.find(".old-") != std::string::npos) {
			DeleteFileA(child.c_str());
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}
#endif

static void CleanupPreviousUpdate(void) {
#ifdef __APPLE__
	// .app install: sibling foo.app.old. We don't know our exact install dir
	// here without mach-o/dyld logic; skip cleanup on macOS and rely on the
	// user trashing .app.old manually.
#else
	char buf[1024];
	int n = 0;
#ifdef _WIN32
	GetModuleFileNameA(NULL, buf, sizeof(buf));
	n = (int)strlen(buf);
#else
	n = (int)readlink("/proc/self/exe", buf, sizeof(buf) - 1);
#endif
	if (n <= 0) return;
	buf[n] = 0;
	std::string exe = buf;
	size_t slash = exe.find_last_of("/\\");
	if (slash == std::string::npos) return;
	std::string install_dir = exe.substr(0, slash);

	// Pre-per-file-replace builds left a `<install>.old` sibling; sweep until
	// they age out.
	std::string old_dir = install_dir + ".old";
	struct stat st;
	if (stat(old_dir.c_str(), &st) == 0) {
		fprintf(stderr, "[updater] cleaning up prior install: %s\n", old_dir.c_str());
#ifdef _WIN32
		std::string cmd = "rd /s /q \"" + old_dir + "\"";
#else
		std::string cmd = "rm -rf '" + old_dir + "'";
#endif
		system(cmd.c_str());
	}

#ifdef _WIN32
	// ReplaceFileAtomic sidelines locked targets as `<file>.old-<ticks>`;
	// they're usually unlocked by next launch.
	SweepSidelinedFiles(install_dir);
#endif
#endif
}

#ifdef POSIX
int main(int argc, char * argv[]){
#endif

#ifdef POSIX
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "--self-update-stage2") == 0){
			return UpdaterStage2::Run(argc, argv);
		}
	}
#endif

#ifdef POSIX
	char cmdlinestr[1024];
	cmdlinestr[0] = 0;
	for(int i = 1; i < argc; i++){
		strcat(cmdlinestr, argv[i]);
		if(i < argc){
			strcat(cmdlinestr, " ");
		}
	}
	char * cmdline = cmdlinestr;
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
	char * cmdline = lpCmdLine;
#endif

#ifndef POSIX
	if(lpCmdLine && strstr(lpCmdLine, "--self-update-stage2")){
		// Use MSVCRT's pre-parsed argv. The previous strtok(" ") split on
		// every space and wasn't quote-aware — paths like
		// "C:\Users\Space Command\..." passed via CreateProcessA fragmented
		// into orphan tokens, and stage-2 saw empty --install-dir / --relaunch
		// values.
		return UpdaterStage2::Run(__argc, __argv);
	}
#endif

	bool dedicatedmode = (cmdline && strncmp(cmdline, "-s", 2) == 0);
#ifdef POSIX
	if(dedicatedmode){
		signal(SIGSEGV, crash_handler);
		signal(SIGABRT, crash_handler);
		signal(SIGBUS, crash_handler);
	}
#endif

	CleanupPreviousUpdate();
#ifdef _WIN32
	SyncInstalledVersionRegistry();
#endif

#ifndef POSIX
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	
#ifdef __APPLE__
	/*CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if(!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)){
        // error!
		return -1;
    }
    CFRelease(resourcesURL);
	
    chdir(path);*/
	
	/*FSRef ref;
	OSType folderType = kApplicationSupportFolderType;
	char apppath[PATH_MAX];
	FSFindFolder(kUserDomain, folderType, kCreateFolder, &ref);
	FSRefMakePath(&ref, (UInt8 *)&apppath, PATH_MAX);*/
#endif

	Game game;
	if(!game.Load(cmdline)){
#ifdef __ANDROID__
		exit(-1);
#endif
		return -1;
	}

	if(game.preview_screen[0]){
		int rc = game.RunPreview();
#ifdef __ANDROID__
		exit(rc);
#endif
		return rc;
	}

	float x = 0, y = 0;
	if(!dedicatedmode){
		SDL_GetMouseState(&x, &y);
	}
	srand((int)x + (int)y + (int)time(NULL));
	while(1){
		if(!game.HandleSDLEvents()){
#ifdef __ANDROID__
			exit(0);
#endif
			return 0;
		}
		if(!game.Loop()){
#ifdef __ANDROID__
			exit(0);
#endif
			return 0;
		}
	}
#ifdef __ANDROID__
	exit(0);
#endif
	return 0;
}