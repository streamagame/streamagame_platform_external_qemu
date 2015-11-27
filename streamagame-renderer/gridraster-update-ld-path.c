#include "gridraster-update-ld-path.h"

static int
getHostOSBitness()
{
  /*
     This function returns 64 if host is running 64-bit OS, or 32 otherwise.

     It uses the same technique in ndk/build/core/ndk-common.sh.
     Here are comments from there:

  ## On Linux or Darwin, a 64-bit kernel (*) doesn't mean that the user-land
  ## is always 32-bit, so use "file" to determine the bitness of the shell
  ## that invoked us. The -L option is used to de-reference symlinks.
  ##
  ## Note that on Darwin, a single executable can contain both x86 and
  ## x86_64 machine code, so just look for x86_64 (darwin) or x86-64 (Linux)
  ## in the output.

    (*) ie. The following code doesn't always work:
        struct utsname u;
        int host_runs_64bit_OS = (uname(&u) == 0 && strcmp(u.machine, "x86_64") == 0);
  */
    return system("file -L \"$SHELL\" | grep -q \"x86[_-]64\"") == 0 ? 64 : 32;
}

/* Find the target-specific emulator binary. This will be something
 * like  <programDir>/emulator-<targetArch>, where <programDir> is
 * the directory of the current program.
 */
char*
getTargetEmulatorPath(const char* progName, const char* avdArch, const int force_32bit)
{
    char*  progDir;
    char   path[PATH_MAX], *pathEnd=path+sizeof(path), *p;
    const char* emulatorPrefix = "gridraster-";
    const char* emulator64Prefix = "gridraster64-";
#ifdef _WIN32
    const char* exeExt = ".exe";
    /* ToDo: currently amd64-mingw32msvc-gcc doesn't work (http://b/issue?id=5949152)
             which prevents us from generating 64-bit emulator for Windows */
    int search_for_64bit_emulator = 0;
#else
    const char* exeExt = "";
    int search_for_64bit_emulator = !force_32bit && getHostOSBitness() == 64;
#endif

    /* Get program's directory name in progDir */
    path_split(progName, &progDir, NULL);

    if (search_for_64bit_emulator) {
        /* Find 64-bit emulator first */
        p = bufprint(path, pathEnd, "%s/%s%s%s", progDir, emulator64Prefix, avdArch, exeExt);
        if (p >= pathEnd) {
            APANIC("Path too long: %s\n", progName);
        }
        if (path_exists(path)) {
            free(progDir);
            return strdup(path);
        }
    }

    /* Find 32-bit emulator */
    p = bufprint(path, pathEnd, "%s/%s%s%s", progDir, emulatorPrefix, avdArch, exeExt);

    free(progDir);
    if (p >= pathEnd) {
        APANIC("Path too long: %s\n", progName);
    }

    if (path_exists(path)) {
        return strdup(path);
    }

    /* Mmm, the file doesn't exist, If there is no slash / backslash
     * in our path, we're going to try to search it in our path.
     */
#ifdef _WIN32
    if (strchr(progName, '/') == NULL && strchr(progName, '\\') == NULL) {
#else
    if (strchr(progName, '/') == NULL) {
#endif
        if (search_for_64bit_emulator) {
           p = bufprint(path, pathEnd, "%s%s%s", emulator64Prefix, avdArch, exeExt);
           if (p < pathEnd) {
               char*  resolved = path_search_exec(path);
               if (resolved != NULL)
                   return resolved;
           }
        }

        p = bufprint(path, pathEnd, "%s%s%s", emulatorPrefix, avdArch, exeExt);
        if (p < pathEnd) {
            char*  resolved = path_search_exec(path);
            if (resolved != NULL)
                return resolved;
        }
    }

    /* Otherwise, the program is missing */
    APANIC("Missing arch-specific emulator program: %s\n", path);
    return NULL;
}

/* return 1 iff <path>/<filename> exists */
int
probePathForFile(const char* path, const char* filename)
{
    char  temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
    p = bufprint(temp, end, "%s/%s", path, filename);
    D("Probing for: %s\n", temp);
    return (p < end && path_exists(temp));
}

/* Find the directory containing a given shared library required by the
 * emulator (for GLES emulation). We will probe several directories
 * that correspond to various use-cases.
 *
 * Caller must free() result string. NULL if not found.
 */

char*
getSharedLibraryPath(const char* progName, const char* libName)
{
    char* progDir;
    char* result = NULL;
    char  temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
    /* Get program's directory name */
    path_split(progName, &progDir, NULL);

    /* First, try to probe the program's directory itself, this corresponds
     * to the standalone build with ./android-configure.sh where the script
     * will copy the host shared library under external/qemu/objs where
     * the binaries are located.
     */
    if (probePathForFile(progDir, libName)) {
        return progDir;
    }

    /* Try under $progDir/lib/, this should correspond to the SDK installation
     * where the binary is under tools/, and the libraries under tools/lib/
     */
    {
        p = bufprint(temp, end, "%s/lib", progDir);
        if (p < end && probePathForFile(temp, libName)) {
            result = strdup(temp);
            goto EXIT;
        }
    }

    /* try in $progDir/../lib, this corresponds to the platform build
     * where the emulator binary is under out/host/<system>/bin and
     * the libraries are under out/host/<system>/lib
     */
    {
        char* parentDir = path_parent(progDir, 1);

        if (parentDir == NULL) {
            parentDir = strdup(".");
        }
        p = bufprint(temp, end, "%s/lib", parentDir);
        free(parentDir);
        if (p < end && probePathForFile(temp, libName)) {
            result = strdup(temp);
            goto EXIT;
        }
    }

    /* Nothing found! */
EXIT:
    free(progDir);
    return result;
}

/* Prepend the path in 'prefix' to either LD_LIBRARY_PATH or PATH to
 * ensure that the shared libraries inside the path will be available
 * through dlopen() to the emulator program being launched.
 */
void
prependSharedLibraryPath(const char* prefix)
{
    char temp[2048], *p=temp, *end=p+sizeof(temp);
#ifdef _WIN32
    const char* path = getenv("PATH");
    if (path == NULL || path[0] == '\0') {
        p = bufprint(temp, end, "PATH=%s", prefix);
    } else {
        p = bufprint(temp, end, "PATH=%s;%s", path, prefix);
    }
    /* Ignore overflow, this will push some paths out of the variable, but
     * so be it. */
    D("Setting %s\n", temp);
    putenv(strdup(temp));
#else
    const char* path = getenv("LD_LIBRARY_PATH");
    if (path != NULL && path[0] != '\0') {
        p = bufprint(temp, end, "%s:%s", prefix, path);
        prefix = temp;
    }
    setenv("LD_LIBRARY_PATH",prefix,1);
    D("Setting LD_LIBRARY_PATH=%s\n", prefix);
#endif
}

#ifdef _WIN32
char*
quotePath(const char* path)
{
    int   len = strlen(path);
    char* ret = malloc(len+3);

    ret[0] = '"';
    memcpy(ret+1, path, len);
    ret[len+1] = '"';
    ret[len+2] = '\0';

    return ret;
}
#endif /* _WIN32 */

