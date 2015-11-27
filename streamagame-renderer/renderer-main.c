#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
  #include <windows.h>
  #include <tchar.h>
#else

  #include <pthread.h>
#endif

/* Required by android/utils/debug.h */
//int android_verbose;

extern void streamagameRenderer(int width, int height);
extern void join_sdl_thread();

int main(int argc, char **argv)
{
    int width = 704; //768;
	int height = 1248; //1184;

    // Parse command-line;
    int i;
    for (i = 1; i < argc; i++) {
        char* param = argv[i];

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

	// Launching renderer
	streamagameRenderer(width, height);
	join_sdl_thread();

	return 0;
}
