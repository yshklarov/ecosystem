#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


/**** Typedefs ****/

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;


/**** Clamping ****/

#ifndef MIN
#define MIN(a,b) ((a < b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a < b) ? (b) : (a))
#endif

#define CLAMP(x, lower, upper) ((x < lower) ? (lower) : ((x > upper) ? (upper) : (x)))
u16 clamp_i32_u16(i32 x) {
    return (u16)CLAMP(x, 0, 0xFFFF);
}

u32 clamp_i32_u32(i32 x) {
    return (u32)MAX(0, x);
}


/**** Random number generator ****/

// JSF (Jenkins Small Fast) random number generator
// https://burtleburtle.net/bob/rand/smallprng.html
typedef struct rand_state { u64 a; u64 b; u64 c; u64 d; } rand_state;
#define ROT32(x,k) (((x)<<(k))|((x)>>(32-(k))))
u64 rand_raw(rand_state* x) {
    u64 e = x->a - ROT32(x->b, 27);
    x->a = x->b ^ ROT32(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}
void rand_init_from_seed(rand_state* x, u64 seed) {
    u64 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; ++i) {
        (void)rand_raw(x);
    }
}
void rand_init_from_time(rand_state* x) {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u64 seed = ts.tv_nsec;
    rand_init_from_seed(x, seed);
}
rand_state rand_state_global;
inline u64 rand_raw() {
    return rand_raw(&rand_state_global);
}


/**** JSON ****/

// TODO

