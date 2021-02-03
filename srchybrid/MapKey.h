//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once
#include "otherfunctions.h"

// use this class if the hash is stored somewhere else (and stays valid as long as this object exists)
class CCKey : public CObject
{
public:
	explicit CCKey(const uchar *key = 0)	{ m_key = key; }
	explicit CCKey(const CCKey &k1)			{ m_key = k1.m_key; }

	CCKey& operator=(const CCKey &k1)		{ m_key = k1.m_key; return *this; }
	friend bool operator==(const CCKey &k1, const CCKey &k2);

	const uchar *m_key;
};

template<> inline UINT AFXAPI HashKey(const CCKey &key)
{
	uint32 hash = 1;
	for (int i = 0; i < 16; ++i)
		hash += (key.m_key[i] + 1) * ((i * i) + 1);
	return hash;
};

// use this class if the hash is stored somewhere inside the key (in any case safer but needs more memory)
class CSKey : public CObject
{
public:
	explicit CSKey(const uchar *key = 0)	{ if (key) md4cpy(m_key, key); else md4clr(m_key); }
	CSKey(const CSKey &k1)					{ md4cpy(m_key, k1.m_key); }

	CSKey& operator=(const CSKey &k1)		{ md4cpy(m_key, k1.m_key); return *this; }
	friend bool operator==(const CSKey &k1, const CSKey &k2);

	uchar m_key[16];
};

template<> inline UINT AFXAPI HashKey(const CSKey &key)
{
	uint32 hash = 1;
	for (int i = 0; i < 16; ++i)
		hash += (key.m_key[i] + 1) * ((i * i) + 1);
	return hash;
};