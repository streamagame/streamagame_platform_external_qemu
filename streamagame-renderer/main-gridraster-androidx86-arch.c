#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/utils/panic.h>
#include <android/utils/path.h>
#include <android/utils/bufprint.h>
#include <android/android.h>
#include "gridraster-update-ld-path.h"


#ifdef WIN32
  #include <windows.h>
  #include <tchar.h>
#else

  #include <pthread.h>
#endif

/* Required by android/utils/debug.h */
int android_verbose;
extern int main_appstream(int argc, const char* argv[]);

extern const char * get_vm_ip();
extern void stop_vm();
extern void gridraster_opengles_fb_update(void* context,
                              int w, int h, int ydir,
                              int format, int type,
                              unsigned char* pixels);
extern void event_loop(int width,int  height);
extern void standalone_gridraster_vm_thread(int width, int height);

extern void join_sdl_thread();

//#define DEBUG 1

#if DEBUG
#  define D(...)  do { if (android_verbose) printf("emulator:" __VA_ARGS__); } while (0)
#else
#  define D(...)  do{}while(0)
#endif

int main(int argc, char **argv)
{
    char*       avdArch = "x86";
    char*       emulatorPath;
    int         force_32bit = 0;
	int         appstream_mode = 0;
	int         launch_appstream = 0;
    int width = 384; //768;
	int height = 592; //1184;

  /* Define ANDROID_EMULATOR_DEBUG to 1 in your environment if you want to
   * see the debug messages from this launcher program.
   */
  const char* debug = getenv("ANDROID_EMULATOR_DEBUG");

  if (debug != NULL && *debug && *debug != '0')
      android_verbose = 1;


    /* Parse command-line and look for
     * 1) '-appstream-mode' which requests running the program in appstream mode. Othersie local mode is chosen.
     */
    int  nn;
    for (nn = 1; nn < argc; nn++) {
        const char* opt = argv[nn];

        if (!strcmp(opt,"-force-32bit")) {
            force_32bit = 1;
            continue;
        }
        if (!strcmp(opt,"-appstream")) {
            appstream_mode = 1;
            continue;
        }
        if (!strncmp(opt, "-width", 5)) {
			char* delimiter = "=";
	        char* token = strtok(opt, delimiter);
	        if(!token) continue;
			char* width_str = strtok(NULL, delimiter);
			if(!width_str) continue;
			width = atoi(width_str);
	        continue;
        }
        if(!strncmp(opt, "-height", 6)) {
			char* delimiter = "=";
	        char* token = strtok(opt, delimiter);
	        if(!token) continue;
			char* height_str = strtok(NULL, delimiter);
			if(!height_str) continue;
			height = atoi(height_str);
			continue;
        }

    }

    /* Find the architecture-specific program in the same directory */
    emulatorPath = getTargetEmulatorPath(argv[0], avdArch, force_32bit);
    D("Found target-specific emulator binary: %s\n", emulatorPath);

    /* Replace it in our command-line */
    argv[0] = emulatorPath;

#ifdef _WIN32
    /* Looks like execv() in mingw (or is it MSVCRT.DLL?) doesn't
     * support a space in argv[0] unless we explicitely quote it.
     * IMPORTANT: do not quote the first argument to execv() or it will fail.
     * This was tested on a 32-bit Vista installation.
     */
    if (strchr(emulatorPath, ' ')) {
        argv[0] = quotePath(emulatorPath);
        D("Quoted emulator binary path: %s\n", emulatorPath);
    }
#endif

    /* We need to find the location of the GLES emulation shared libraries
     * and modify either LD_LIBRARY_PATH or PATH accordingly
     */
    {
        char*  sharedLibPath = getSharedLibraryPath(emulatorPath, GLES_EMULATION_LIB);

        if (sharedLibPath != NULL) {
            D("Found OpenGLES emulation libraries in %s\n", sharedLibPath);
            prependSharedLibraryPath(sharedLibPath);
        } else {
            D("Could not find OpenGLES emulation host libraries!\n");
        }
    }

    /* -appstream-mode is only supported in windows. Otherwise launch VM directly.
     */
#ifdef _WIN32
   if(appstream_mode){
      D("appstream_mode requested on windows platform \n");
      launch_appstream = 1;
   }
   else{
      D("VM mode requested on windows platform \n");
   }
#else
   D("On non-windows platform, appstream_mode will be ignored. VM mode will be launched by default \n");
#endif

  if(launch_appstream){
#ifdef _WIN32
    main_appstream(argc, argv);
#endif
  }
  else{
    // Launching VM mode for local testing
    standalone_gridraster_vm_thread(width, height);

  }
  join_sdl_thread();
  return (0);
}
