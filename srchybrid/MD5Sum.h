#pragma once

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptopp/md5.h"

#define MD5_BLOCK_SIZE	64
#define MD5_DIGEST_SIZE	16

typedef struct
{
	byte	b[MD5_DIGEST_SIZE];
} MD5;

class MD5Sum
{
public:
	MD5Sum();
	explicit MD5Sum(const CString &sSource);
	MD5Sum(const byte *pachSource, size_t nLength);

	void Calculate(const CString &sSource);
	void Calculate(const byte *pachSource, size_t nLength);

	CString GetHashString() const;
	const byte* GetRawHash() const		{ return m_hash.b; }

private:
	MD5 m_hash;
};