#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef std::atomic_ushort u16Atomic;
typedef unsigned int       u32;
typedef unsigned long      u64;
typedef short              i16;
typedef int                i32;
typedef float              f32;

#define U8_MAX  0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define F32_MAX FLT_MAX

typedef FILE           FileHandle;
typedef pthread_t      Thread;
typedef struct timeval TimeValue;

#endif
