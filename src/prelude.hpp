#ifndef __PRELUDE_H__
#define __PRELUDE_H__

#include <atomic>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t   usize;

typedef int16_t i16;
typedef int32_t i32;

typedef float f32;

typedef FILE File;

#define null nullptr

typedef struct timeval TimeValue;

typedef pthread_t            Thread;
typedef std::atomic_uint16_t u16Atomic;

#define F32_MAX FLT_MAX

#define SEQ_CST std::memory_order_seq_cst

#define IMAGE_WIDTH  1280
#define IMAGE_HEIGHT 512
#define N_PIXELS     (IMAGE_WIDTH * IMAGE_HEIGHT)

#endif
