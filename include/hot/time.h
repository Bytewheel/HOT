#ifndef HOT_TIME_H
#define HOT_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t hot_time_t;

#define HOT_BIT_32COMPAT  (UINT64_C(1) << 63)
#define HOT_VALUE_MASK    (UINT64_C(0x7FFFFFFFFFFFFFFF))
#define HOT_ERA_SHIFT     32
#define HOT_ERA_MASK      (UINT64_C(0x7FFFFFFF))
#define HOT_ERA_MAX       UINT32_C(0x7FFFFFFF)

#define HOT_EAMBIGUOUS  (-1)
#define HOT_ERANGE      (-2)
#define HOT_ENOSPC      (-3)
#define HOT_EMALFORMED  (-4)

enum hot_wrap_policy { HOT_WRAP, HOT_SATURATE, HOT_ERROR };

hot_time_t hot_now_word(void);
hot_time_t hot_from_64(int64_t seconds);
hot_time_t hot_from_32(int32_t legacy, uint32_t era);
hot_time_t hot_from_u32(uint32_t legacy, uint32_t era);
int64_t    hot_seconds(const hot_time_t word);
bool       hot_is_compat(const hot_time_t word);
uint32_t   hot_era_of(const hot_time_t word);
int        hot_compat_active(void);
int        hot_from_32_bounded(int32_t legacy, int64_t lower, int64_t upper,
                              hot_time_t *out);
int        hot_to_32(int64_t seconds, enum hot_wrap_policy policy,
                    int32_t *out);

#endif