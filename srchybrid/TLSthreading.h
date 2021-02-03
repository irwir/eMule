#pragma once

#include "mbedtls/config.h"
#include "mbedtls/threading.h"

void threading_mutex_init_alt(mbedtls_threading_mutex_t *mutex) noexcept;
void threading_mutex_free_alt(mbedtls_threading_mutex_t *mutex) noexcept;
int threading_mutex_lock_alt(mbedtls_threading_mutex_t *mutex) noexcept;
int threading_mutex_unlock_alt(mbedtls_threading_mutex_t *mutex) noexcept;
CString SSLerror(int ret);