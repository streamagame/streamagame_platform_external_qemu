#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/utils/panic.h>
#include <android/utils/path.h>
#include <android/utils/bufprint.h>
#include <android/avd/util.h>

/* Required by android/utils/debug.h */
int android_verbose;


#define DEBUG 1

#if DEBUG
#  define D(...)  do { if (android_verbose) printf("emulator:" __VA_ARGS__); } while (0)
#else
#  define D(...)  do{}while(0)
#endif

/* The extension used by dynamic libraries on the host platform */
#ifdef _WIN32
#  define DLL_EXTENSION  ".dll"
#elif defined(__APPLE__)
#  define DLL_EXTENSION  ".dylib"
#else
#  define DLL_EXTENSION  ".so"
#endif

#if defined(__x86_64__)
/* Normally emulator is compiled in 32-bit.  In standalone it can be compiled
   in 64-bit (with ,/android-configure.sh --try-64).  In this case, emulator-$ARCH
   are also compiled in 64-bit and will search for lib64*.so instead of lib*so */
#define  GLES_EMULATION_LIB  "lib64OpenglRender" DLL_EXTENSION
#elif defined(__i386__)
#define  GLES_EMULATION_LIB  "libOpenglRender" DLL_EXTENSION
#else
#error Unknown architecture for codegen
#endif


/* Forward declarations */
char* getTargetEmulatorPath(const char* progName, const char* avdArch , const int force_32bit);
char* getSharedLibraryPath(const char* progName, const char* libName);
void  prependSharedLibraryPath(const char* prefix);

#ifdef _WIN32
char* quotePath(const char* path);
#endif

