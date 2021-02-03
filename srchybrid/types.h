#pragma once

typedef unsigned char		uchar;
typedef unsigned char		uint8;
typedef	  signed char		sint8;

typedef unsigned short		uint16;
typedef	  signed short		sint16;

typedef unsigned int		uint32;
typedef	  signed int		sint32;

typedef unsigned __int64	uint64;
typedef   signed __int64	sint64;

#ifdef _DEBUG
#include "Debug_FileSize.h"
#define USE_DEBUG_EMFILESIZE
typedef CEMFileSize			EMFileSize;
#else
typedef unsigned __int64	EMFileSize;
#endif

