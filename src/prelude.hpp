#ifndef __PRELUDE_H__
#define __PRELUDE_H__

#include <atomic>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t  i16;
typedef int32_t  i32;
typedef float    f32;

typedef FILE File;

typedef struct timeval TimeValue;

typedef pthread_t            Thread;
typedef std::atomic_uint16_t u16Atomic;

#define F32_MAX FLT_MAX

#define SEQ_CST std::memory_order_seq_cst

#define IMAGE_WIDTH  768u
#define IMAGE_HEIGHT 512u
#define N_PIXELS     393216u

#define FLOAT_WIDTH  768.0f
#define FLOAT_HEIGHT 512.0f

#endif
