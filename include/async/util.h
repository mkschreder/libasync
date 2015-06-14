///! Author: Martin K. Schröder <mkschreder.uk@gmail.com>

#pragma once 

#define wrap_pi(x) (x < -M_PI ? x+M_PI*2 : (x > M_PI ? x - M_PI*2: x))

static inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static inline float fmap(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define constrain(x, a, b) (((x) < (a))?(a):(((x) > (b))?(b):(x)))

#define container_of(ptr, type, member) __extension__ ({                      \
        __typeof__( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)(void*)( (char *)__mptr - offsetof(type,member) );})

// wraps degrees around a circle
#define wrap_180(x) (x < -180 ? x+360 : (x > 180 ? x - 360: x))
