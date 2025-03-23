#pragma once

const char *path_basename(const char *path);
int mkdir_p(const char *path, int mode);

template<typename T>
bool fequals(T a, T b, T epsilon = 0.0001)
{
    return (a < (b + epsilon)) && (a > (b - epsilon));
}
