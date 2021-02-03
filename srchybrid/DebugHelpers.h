#pragma once

#define CHECK_OBJ(pObj)		if (pObj != NULL) ASSERT_VALID(pObj)
#define CHECK_PTR(ptr)		ASSERT( ptr == NULL || AfxIsValidAddress(ptr, sizeof(*ptr)) );
#define CHECK_ARR(ptr, len)	ASSERT( (ptr == NULL && len == 0) || (ptr != NULL && len != 0 && AfxIsValidAddress(ptr, len)) );
#define	CHECK_BOOL(bVal)	ASSERT( (int)(bVal) == 0 || (int)(bVal) == 1 );

#define	CRASH_HERE()		(*((int*)NULL) = 0)
