#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>

#include "utils.h"
#include "logging.h"

const char *path_basename(const char *path)
{
    const char *ret = strrchr(path, '/');
    if (!ret) {
        ret = path;
    }
    return ret;
}

int mkdir_p(const char *path, int mode)
{
    char path_buf[256]{};
    strcpy(path_buf, path);

    // Iterate over each char and when we get to a /, replace it with a null and pass the path to mkdir - this will
    // create all the parent directories needed
    char *p = path_buf+1;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(path_buf, mode) != 0 && errno != EEXIST) {
                wlog("Could not create directory %s: %s", path_buf, strerror(errno));
                return -1;
            }
            *p = '/';
        }
        ++p;
    }
    
    // And now one final mkdir with the original path
    if (mkdir(path, mode) != 0 && errno != EEXIST) {
        wlog("Could not create directory %s: %s", path, strerror(errno));
        return -1;
    }
    return 0;
}

small_str generate_id()
{
    // Generate in this format 774f0899-9666471a-b1f9
    small_str ret{};
    u32 r1 = rand();
    u32 r2 = rand();
    u16 r3 = rand();
    sprintf(ret.data, "%08x-%08x-%04x", r1, r2, r3);
    ret.size = strnlen(ret.data, SMALL_STR_LEN);
    return ret;
}
