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

/* extern void gridraster_opengles_fb_update(void* context,
                              int w, int h, int ydir,
                              int format, int type,
                              unsigned char* pixels);*/
// extern void event_loop(int width,int  height);
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
    int width = 384; //768;
	int height = 592; //1184;

    // Parse command-line;
    int i;
    for (i = 1; i < argc; i++) {
        char* param = argv[i];

        if (!strcmp(param,"-force-32bit")) {
            force_32bit = 1;
            continue;
        }
        if (!strncmp(param, "-width", 5)) {
			char* delimiter = "=";
	        char* token = strtok(param, delimiter);
	        if(!token) continue;
			char* width_str = strtok(NULL, delimiter);
			if(!width_str) continue;
			width = atoi(width_str);
	        continue;
        }
        if(!strncmp(param, "-height", 6)) {
			char* delimiter = "=";
	        char* token = strtok(param, delimiter);
	        if(!token) continue;
			char* height_str = strtok(NULL, delimiter);
			if(!height_str) continue;
			height = atoi(height_str);
			continue;
        }

    }

    /* Find the architecture-specific program in the same directory */
    emulatorPath = getTargetEmulatorPath(argv[0], avdArch, force_32bit);
    printf("Found target-specific emulator binary: %s\n", emulatorPath);

    /* Replace it in our command-line */
    argv[0] = emulatorPath;

    /* We need to find the location of the GLES emulation shared libraries
     * and modify either LD_LIBRARY_PATH or PATH accordingly
     */
    {
        char*  sharedLibPath = getSharedLibraryPath(emulatorPath, GLES_EMULATION_LIB);

        if (sharedLibPath != NULL) {
            printf("Found OpenGLES emulation libraries in %s\n", sharedLibPath);
            prependSharedLibraryPath(sharedLibPath);
        } else {
            printf("Could not find OpenGLES emulation host libraries!\n");
        }
    }

	// Launching VM mode for local testing
	standalone_gridraster_vm_thread(width, height);

	join_sdl_thread();
	return 0;
}
