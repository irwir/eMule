//
// MD4.h
//
// Copyright (c) Shareaza Development Team, 2002-2004.
// This file is part of SHAREAZA (www.shareaza.com)
//
// Shareaza is free software; you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Shareaza is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Shareaza; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptopp/md4.h"
#include "otherfunctions.h"

#define MD4_BLOCK_SIZE	64
#define MD4_DIGEST_SIZE	16

typedef struct
{
	byte	b[MD4_DIGEST_SIZE];
} MD4;

class CMD4
{
// Attributes
	CryptoPP::Weak::MD4 m_md4; // netfinity: Use cryptlib
	MD4 m_hash;
// Operations
public:
	// Construction
	CMD4();
	void	Reset();
	void	Add(LPCVOID pData, size_t nLength);
	void	Finish();
	const byte* GetHash() const;
};

inline bool operator==(const MD4 &md4a, const MD4 &md4b)
{
	return md4equ(&md4a, &md4b);
}

inline bool operator!=(const MD4 &md4a, const MD4 &md4b)
{
	return !(md4a == md4b);
}