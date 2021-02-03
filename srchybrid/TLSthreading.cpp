#include "stdafx.h"

#include "TLSthreading.h"
#include "mbedtls/error.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void threading_mutex_init_alt(mbedtls_threading_mutex_t *mutex) noexcept
{
	if (mutex) {
		InitializeCriticalSection(&mutex->cs);
		mutex->is_valid = 1;
	}
}

void threading_mutex_free_alt(mbedtls_threading_mutex_t *mutex) noexcept
{
	if (mutex && mutex->is_valid) {
		DeleteCriticalSection(&mutex->cs);
		mutex->is_valid = 0;
	}
}

int threading_mutex_lock_alt(mbedtls_threading_mutex_t *mutex) noexcept
{
	if (mutex == NULL || !mutex->is_valid)
		return(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA);
	EnterCriticalSection(&mutex->cs);
	return(0);
}

int threading_mutex_unlock_alt(mbedtls_threading_mutex_t *mutex) noexcept
{
	if (mutex == NULL || !mutex->is_valid)
		return(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA);
	LeaveCriticalSection(&mutex->cs);
	return(0);
}

CString SSLerror(int ret)
{
	char buf[256];
	mbedtls_strerror(ret, buf, sizeof buf);
	buf[sizeof buf - 1] = '\0';
	return CString(buf);
}