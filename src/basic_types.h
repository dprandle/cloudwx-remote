#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <climits>

// Check if all of the flags in provided flags
#define test_all_flags(bitmask, flags) (((bitmask) & (flags)) == (flags))

// Check if any of the bits provided in \param flags are set in \param bitmask
#define test_flags(bitmask, flags) (((bitmask) & (flags)) != 0)

// Unset all of the bits provided by \param flags in \param bitmask
#define unset_flags(bitmask, flags) ((bitmask) &= ~(flags))

// Set all of the bits provided by \param flags in \param bitmask
#define set_flags(bitmask, flags) ((bitmask) |= (flags))

#define set_flag_from_bool(bitmask, flag, boolval) ((boolval) ? set_flags(bitmask, flag) : unset_flags(bitmask, flag))

#define intern static

#define SMALL_STR_LEN 24

#if defined(NDEBUG)
    #define asrt(param) (!(param)) ? elog("Assertion: " #param " failed") : (void)0
    #define asrt_break(msg) elog("Assertion break: " #msg)
#else
    #define asrt assert
    #define asrt_break(msg) assert(!msg)
#endif

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using uchar = unsigned char;
using wchar = wchar_t;
using c16 = char16_t;
using c32 = char32_t;
using sizet = std::size_t;
using f32 = float;
using f64 = double;
using f128 = long double;
using b32 = bool;
using cstr = const char *;

const sizet KB_SIZE = 1024;
const sizet MB_SIZE = 1024 * KB_SIZE;

struct small_str {
    char data[SMALL_STR_LEN];
    sizet size{};
};

inline constexpr const sizet INVALID_IND = ULONG_LONG_MAX;
inline constexpr const u32 INVALID_ID = UINT_MAX;

inline bool is_valid(sizet v)
{
    return (v != INVALID_IND);
}
inline bool is_valid(u32 v)
{
    return (v != INVALID_ID);
}
