#include <cstring>
#include "utils.h"

const char *path_basename(const char *path)
{
    const char *ret = strrchr(path, '/');
    if (!ret) {
        ret = path;
    }
    return ret;
}

