/*
 ---------------------------------------------------------------------------
 Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 30/11/2002

 This is a byte oriented version of SHA1 that operates on arrays of bytes
 stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
*/
#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1 //suppress deprecation warning
#include "shahashset.h"
#include "cryptopp/sha.h"
//byte sizes
#define SHA1_BLOCK_SIZE	64
#define SHA1_DIGEST_SIZE	20

typedef struct
{
	BYTE	b[SHA1_DIGEST_SIZE];
} SHA1;

class CSHA : public CAICHHashAlgo
{
// Attributes
	// netfinity: Use cryptlib for non X86 platforms
	CryptoPP::SHA1 m_sha;
	SHA1 m_hash;

// Operations
public:
	// Construction
	CSHA();
	// CAICHHashAlgo interface
	virtual void	Reset();
	virtual void	Add(LPCVOID pData, DWORD nLength);
	virtual void	Finish(CAICHHash &rHash);
	virtual void	GetHash(CAICHHash &rHash);

	void	Finish();
	void	GetHash(SHA1 *pHash) const;
	CString	GetHashString(bool bURN = false) const;

	static CString	HashToString(const SHA1 *pHashIn, bool bURN = false);
	static CString	HashToHexString(const SHA1 *pHashIn, bool bURN = false);
	static bool		HashFromString(LPCTSTR pszHash, SHA1 *pHashIn);
	static bool		HashFromURN(LPCTSTR pszHash, SHA1 *pHashIn);
	static bool		IsNull(SHA1 *pHash);
};

inline bool operator==(const SHA1 &sha1a, const SHA1 &sha1b)
{
	return memcmp(&sha1a, &sha1b, SHA1_DIGEST_SIZE) == 0;
}

inline bool operator!=(const SHA1 &sha1a, const SHA1 &sha1b)
{
	return !(sha1a == sha1b);
}