#pragma once
#ifdef _MSC_VER
#include <winsock2.h>

typedef struct
{
	CRITICAL_SECTION cs;
	char is_valid;
} mbedtls_threading_mutex_t;

#endif